
#include "antworld.h"
#include "utility_functions.h"
#include "utilities.h"
#include <algorithm>

namespace {
// 6-bit signal protocol constants & helpers (3 bits row, 3 bits col, 3-bit signed two's complement)
inline constexpr Coord SIGNAL_DEAD(0, 0);
inline constexpr Coord SIGNAL_OVER(-4, -4);

inline Coord decodeSignal(const bool buffer[6]) {
    int r = (buffer[0] << 2) | (buffer[1] << 1) | buffer[2];
    int c = (buffer[3] << 2) | (buffer[4] << 1) | buffer[5];
    int dr = (r & 4) ? (r - 8) : r;
    int dc = (c & 4) ? (c - 8) : c;
    return Coord(dr, dc);
}

inline bool getSignalBit(Coord rel, int bitIndex) {
    if (bitIndex < 3) {
        return (rel.first >> (2 - bitIndex)) & 1;
    }
    return (rel.second >> (5 - bitIndex)) & 1;
}
} // anonymous namespace

template <typename RoleAnt>
inline void independentSearch(RoleAnt& roleAnt) {
    Ant& ant = *roleAnt.ant;
    AntWorld& world = *roleAnt.world;

    // Move to the current broadcast point (repurposed as search vantage cursor)
    safeMove(ant, roleAnt.broadcastPoint, world);

    // Scan for food from this vantage
    std::vector<Coord> visible = ant.foodScan(world.foodMap);
    if (!visible.empty()){
        // Prefer the closest food to home that we can afford a round trip on
        std::sort(visible.begin(), visible.end(), [&](Coord a, Coord b){
            return sortByDistance(a, b, ant.homeCoord, world);
        });
        for (auto& food : visible){
            if (roundTrip(ant, food, world)){
                // Delivered food to homeCoord; return so updateWorld() scores it before we leave on next turn
                return;
            }
        }
        // Return to broadcast point so the next tick advances cleanly
        safeMove(ant, roleAnt.broadcastPoint, world);
    }

    // Step to the next vantage in the territory grid
    roleAnt.broadcastPoint = roleAnt.territory.nextVantage(roleAnt.broadcastPoint, roleAnt.heading);
}

class Carrier{ // wrapper for carrier ant; allows us to track other info for carrier
    public:
    enum State {
        MOVE_TO_LISTENING_POINT = 0,
        RECEIVE_SIGNAL,
        INDEPENDENT_SEARCH
    };

    Ant* ant;
    State state = MOVE_TO_LISTENING_POINT;
    bool dead = false;
    Territory territory;
    Coord broadcastPoint;
    Coord heading;
    int signalCount = 0;
    bool signalBuffer[6] = {0};
    AntWorld* world;

    Carrier() : ant(nullptr), world(nullptr) {}

    Carrier(Ant* ant, Territory territory, Coord broadcastPoint, Coord heading, AntWorld& world)
        : ant(ant), territory(territory), broadcastPoint(broadcastPoint), heading(heading), world(&world) {}

    void resetSignal(){
        signalCount = 0;
        std::fill(signalBuffer, signalBuffer + 6, false);
    }

    void moveToListeningPoint(){
        // determine listeningPoint on the fly: closest point to homeCoord within pheromoneRange of broadcastPoint
        Coord listeningPoint = broadcastPoint; // fallback: broadcastPoint itself is always valid
        std::vector<Coord> path = shortestPath(world->terrainMap, ant->homeCoord, broadcastPoint);
        for (const auto& step : path){
            if (std::abs(broadcastPoint.first - step.first) <= ant->pheromoneRadius &&
                std::abs(broadcastPoint.second - step.second) <= ant->pheromoneRadius){
                listeningPoint = step;
                break;
            }
        }
        safeMove(*ant, listeningPoint, *world);
        ant->erasePheromone(world->pheromoneMap);

        // If the ant could not reach within pheromone range of broadcast point, search independently
        if (std::abs(broadcastPoint.first - ant->position.first) > ant->pheromoneRadius ||
            std::abs(broadcastPoint.second - ant->position.second) > ant->pheromoneRadius){
            state = INDEPENDENT_SEARCH;
        } else {
            state = RECEIVE_SIGNAL;
        }
    }

    void receiveSignal(){
        if (signalCount < 6){
            signalBuffer[signalCount] = world->pheromoneMap[broadcastPoint.first][broadcastPoint.second];
        }
        signalCount++;

        if (signalCount == 6){
            Coord rel = decodeSignal(signalBuffer);

            if (rel == SIGNAL_DEAD){
                state = INDEPENDENT_SEARCH; // dead scout — switch to independent search
                resetSignal();
                return;
            }
            else if (rel == SIGNAL_OVER){ // "over" signal (-4, -4)
                // Wait for the 7th (buffer) tick before moving so scout and carrier advance in lockstep
                return;
            }

            Coord foodCoord = broadcastPoint + rel;
            int tripCost = shortestPathCost(ant->position, foodCoord, *world) + shortestPathCost(foodCoord, ant->homeCoord, *world);
            if (tripCost < ant->energy && world->pheromoneMap[ant->position.first][ant->position.second] == 0){ // full trip possible & signal unclaimed
                ant->dropPheromone(world->pheromoneMap); // mark that the signal has been answered
                safeMove(*ant, foodCoord, *world);
                if (ant->carryingFood){ 
                    safeMove(*ant, ant->homeCoord, *world);
                    state = MOVE_TO_LISTENING_POINT;
                    resetSignal();
                    return;
                } else { // food was taken before we got here; grab anything visible
                     std::vector<Coord> visible = ant->foodScan(world->foodMap);
                    if (!visible.empty()){
                        int fallbackCost = shortestPathCost(ant->position, visible[0], *world) + shortestPathCost(visible[0], ant->homeCoord, *world);
                        if (fallbackCost < ant->energy){
                            safeMove(*ant, visible[0], *world);
                            if (ant->carryingFood){
                                safeMove(*ant, ant->homeCoord, *world);
                                state = MOVE_TO_LISTENING_POINT;
                                resetSignal();
                                return;
                            }
                        }
                    }
                }
                moveToListeningPoint(); // return to listening point if no food was collected
            }
            // If food not claimed, wait for 7th (buffer) tick before resetting
            return;
        }

        if (signalCount >= 7){
            Coord rel = decodeSignal(signalBuffer);
            if (rel == SIGNAL_OVER){
                broadcastPoint = territory.nextVantage(broadcastPoint, heading);
                state = MOVE_TO_LISTENING_POINT;
            }
            world->pheromoneMap[ant->position.first][ant->position.second] = 0;
            resetSignal();
        }
    }

    void die(){
        dead = true;
        ant->energy = 1;
        ant->erasePheromone(world->pheromoneMap);
    }

    void independentSearch(){
        ::independentSearch(*this);
    }

    void forage(){
        if (dead) return;
        switch(state){
            case MOVE_TO_LISTENING_POINT: moveToListeningPoint(); break;
            case RECEIVE_SIGNAL:          receiveSignal(); break;
            case INDEPENDENT_SEARCH:      independentSearch(); break;
        }
        if (ant->energy <= 1) die();
    }
};

class Scout{ // wrapper for scout ant; allows us to track other info for scout
    public: 
    enum State {
        ASSUME_VANTAGE = 0,
        SIGNAL_FOOD,
        SIGNAL_OVER,
        INDEPENDENT_SEARCH
    };

    Ant* ant;
    State state = ASSUME_VANTAGE;
    bool dead = false;
    int signalCount = 0;
    Coord cachedRelative = Coord(0, 0);
    Territory territory;
    Coord heading;
    Coord broadcastPoint;
    AntWorld* world;

    Scout(Ant* ant, Territory territory, AntWorld& world)
        : ant(ant),
          territory(territory),
          heading(territory.startHeading(ant->foodRadius)),
          broadcastPoint(territory.startPoint(ant->foodRadius)),
          world(&world) {}

    void assumeVantage(){
        safeMove(*ant, broadcastPoint, *world);
        if (ant->position == broadcastPoint){
            state = SIGNAL_FOOD; // reached vantage, begin signalling
        } else {
            // Can't reach vantage — insufficient energy for the terrain cost.
            // Die so carriers detect it via the (0,0) signal.
            die();
        }
    }

    void signalCoord(){
        ant->erasePheromone(world->pheromoneMap);
        if (signalCount < 6){
            if (getSignalBit(cachedRelative, signalCount)){
                ant->dropPheromone(world->pheromoneMap);
            }
            signalCount++;
        } else if (signalCount == 6){
            // 7th tick (index 6): buffer/dead tick. Pheromone already erased above.
            signalCount = 0;
            int dr = (cachedRelative.first & 4) ? (cachedRelative.first - 8) : cachedRelative.first;
            int dc = (cachedRelative.second & 4) ? (cachedRelative.second - 8) : cachedRelative.second;
            Coord foodCoord = ant->position + Coord(dr, dc);
            if (world->foodMap[foodCoord.first][foodCoord.second] == 1){ // food wasn't picked up, do it itself
                safeMove(*ant, foodCoord, *world);
                safeMove(*ant, ant->homeCoord, *world);
                state = INDEPENDENT_SEARCH;
                return;
            }
        }
    }

    void signalFood(){ // if sees food, signals it; if not, tells us to change state
        if (signalCount == 0){
            std::vector<Coord> visibleFood = ant->foodScan(world->foodMap);
            if (visibleFood.empty()){
                state = SIGNAL_OVER;
                signalOver();
                return;
            }
            std::sort(visibleFood.begin(), visibleFood.end(), [this](Coord a, Coord b){
                return sortByDistance(a, b, ant->homeCoord, *world);
            });

            cachedRelative = Coord((visibleFood[0].first  - ant->position.first)  & 7,
                                   (visibleFood[0].second - ant->position.second) & 7);
        }
        signalCoord();
    }

    bool signalOver(){
        if (signalCount < 6){
            // "Over" signal is (-4, -4), encoded in 3-bit two's complement as [1, 0, 0, 1, 0, 0]
            if (signalCount == 0 || signalCount == 3){
                ant->dropPheromone(world->pheromoneMap);
            } else {
                ant->erasePheromone(world->pheromoneMap);
            }
            signalCount++;
            return true;
        }
        ant->erasePheromone(world->pheromoneMap);
        signalCount = 0;
        state = ASSUME_VANTAGE;
        // If the scout picked up food at this vantage (move() auto-collects on arrival),
        // drop it here before advancing. From the next vantage the food is at exactly
        // foodRadius distance and encodes as a non-zero relative coord — safe for carriers.
        if (ant->carryingFood){
            world->foodMap[ant->position.first][ant->position.second] = 1;
            ant->carryingFood = false;
        }
        broadcastPoint = territory.nextVantage(broadcastPoint, heading);
        return false;
    }

    void independentSearch(){
        ::independentSearch(*this);
    }

    void die(){
        dead = true;
        ant->energy = 1;
        ant->erasePheromone(world->pheromoneMap);
    }

    void forage(){
        if (dead) return;
        switch(state){
            case ASSUME_VANTAGE:     assumeVantage(); break;
            case SIGNAL_FOOD:        signalFood(); break;
            case SIGNAL_OVER:        signalOver(); break;
            case INDEPENDENT_SEARCH: independentSearch(); break;
        }
        if (ant->energy <= 1) die();
    }
};

void assignRoles(std::vector<Ant>& ants, std::vector<Scout>& scouts, std::vector<Carrier>& carriers, AntWorld& world, int numScouts = 4){
    if (ants.empty()) return;

    std::sort(ants.begin(), ants.end(), [](const Ant& a, const Ant& b) { return a.energy > b.energy; });
    size_t scoutCount = std::min(static_cast<size_t>(numScouts), ants.size());
    for (size_t i = 0; i < scoutCount; ++i){
        Territory territory(ants[i].homeCoord, i, numScouts, world.terrainMap);
        scouts.emplace_back(&ants[i], territory, world);
    }
    for (size_t i = scoutCount; i < ants.size(); ++i){
        const auto& scout = scouts[i % scouts.size()];
        carriers.emplace_back(&ants[i], scout.territory, scout.broadcastPoint, scout.heading, world);
    }
}

void clearImmediateArea(std::vector<Ant>& ants, AntWorld& world){
    std::sort(ants.begin(), ants.end(), [](const Ant& a, const Ant& b){return a.energy < b.energy;}); // sort ants lowest energy to highest
    std::vector<Coord> visibleFood = ants[0].foodScan(world.foodMap); // see food
    for (auto food : visibleFood){ // gather each food with the lowest energy ant that can make the round trip
        for (auto& ant : ants){
            if (roundTrip(ant, food, world)){
                break;
            }
        }
    }
}

// ============================================================================
// Multi-run Reset Support (Needed to reset state between multiple benchmark seeds)
// To revert: remove this block, remove resetSignalSearch(), and uncomment below.
// ============================================================================
static int phase = 0;
static std::vector<Scout> scouts;
static std::vector<Carrier> carriers;
static AntWorld* signalWorld = nullptr;

void resetSignalSearch(){
    phase = 0;
    scouts.clear();
    carriers.clear();
    signalWorld = nullptr;
}
// ============================================================================

void signallingAnts(AntWorld& world, int numScouts){ // handles first stage control, team creation, and team organization
    // Auto-reset when a new world instance is provided in multi-seed benchmarks
    if (signalWorld != &world || scouts.empty()){
        resetSignalSearch();
        signalWorld = &world;
    }

    /* --- ORIGINAL CODE START (uncomment to revert to single-game static scope) ---
    static int phase = 0;
    static std::vector<Scout> scouts;
    static std::vector<Carrier> carriers;
    --- ORIGINAL CODE END --- */

    switch (phase){
        case 0: 
            clearImmediateArea(world.ants, world);
            assignRoles(world.ants, scouts, carriers, world, numScouts);
            phase = 1;
            break;
        case 1: // allow the ants to determine their own behaviour
            for (auto& scout : scouts){
                scout.forage();
            }
            for (auto& carrier : carriers){
                carrier.forage();
            }
            break;
    }

    bool allExhausted = std::all_of(world.ants.begin(), world.ants.end(), [](const Ant& ant) {
        return ant.energy <= 2;
    });
    if (allExhausted){
        for (auto& ant : world.ants){
            ant.energy = 0;
        }
    }
}