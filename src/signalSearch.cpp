
#include "antworld.h"
#include "utility_functions.h"
#include "utilities.h"
#include <algorithm>

namespace {
// 6-bit signal protocol helpers (3 bits row, 3 bits col)
inline Coord decodeSignal(const bool buffer[6]) {
    int r = (buffer[0] << 2) | (buffer[1] << 1) | buffer[2];
    int c = (buffer[3] << 2) | (buffer[4] << 1) | buffer[5];
    return Coord(r, c);
}

inline bool getSignalBit(int relFirst, int relSecond, int bitIndex) {
    if (bitIndex < 3) {
        return (relFirst >> (2 - bitIndex)) & 1;
    }
    return (relSecond >> (5 - bitIndex)) & 1;
}
} // anonymous namespace

class Carrier{ // wrapper for carrier ant; allows us to track other info for carrier
    public:
    Ant* ant;
    int state = 0;
    bool dead = false;
    Territory territory;
    Coord broadcastPoint;
    Coord heading;
    int signalCount = 0;
    bool signalBuffer[6] = {0};
    AntWorld* world;

    Carrier() : ant(nullptr), world(nullptr) {}

    Carrier(Ant* ant, Territory territory, Coord broadcastPoint, Coord heading, AntWorld& world){
        this->ant = ant;
        this->territory = territory;
        this->broadcastPoint = broadcastPoint;
        this->heading = heading;
        this->world = &world;
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
        state = 1;
        // TODO: insufficient energy?
    }

    void receiveSignal(){
        signalBuffer[signalCount++] = world->pheromoneMap[broadcastPoint.first][broadcastPoint.second];
        if (signalCount < 6) return;

        Coord rel = decodeSignal(signalBuffer);
        Coord foodCoord = rel - Coord(ant->foodRadius, ant->foodRadius) + broadcastPoint;

        if (rel.first == 0 && rel.second == 0){
            state = 2; // dead scout — switch to independent search
        }
        else if (rel.first == 7 && rel.second == 7){ // "over" signal
            broadcastPoint = territory.nextVantage(broadcastPoint, heading); // uses the scouts heading
            state = 0; // next step is to move to vantage point
        }
        else if (shortestPathCost(ant->position, foodCoord, *world) * 2 <= ant->energy // round trip possible
            && world->pheromoneMap[ant->position.first][ant->position.second] == 0){ // signal unclaimed by another carrier
            ant->dropPheromone(world->pheromoneMap); // mark that the signal has been answered
            
            safeMove(*ant, foodCoord, *world);
            if (ant->carryingFood){ 
                ant->returnHome(world->terrainMap, world->foodMap);
            } else { // food was taken before we got here; grab anything visible
                std::vector<Coord> visible = ant->foodScan(world->foodMap);
                if (!visible.empty()){
                    roundTrip(*ant, visible[0], *world);
                }
            }
            moveToListeningPoint(); // return to listening point
        }
        signalCount = 0; // reset signal count and buffer
        std::fill(signalBuffer, signalBuffer + 6, false);
    }

    void die(){
        this->dead = true;
        this->ant->energy = 1;
        this->ant->erasePheromone(world->pheromoneMap);
    }

    void independentSearch(){
        // Move to the current broadcast point (repurposed as search vantage cursor)
        safeMove(*ant, broadcastPoint, *world);

        // Scan for food from this vantage
        std::vector<Coord> visible = ant->foodScan(world->foodMap);
        if (!visible.empty()){
            // Prefer the closest food to home that we can afford a round trip on
            std::sort(visible.begin(), visible.end(), [this](Coord a, Coord b){
                return sortByDistance(a, b, ant->homeCoord, *world);
            });
            for (auto& food : visible){
                if (roundTrip(*ant, food, *world)){
                    break; // one food per visit; return to vantage after
                }
            }
            // Return to broadcast point so the next tick advances cleanly
            safeMove(*ant, broadcastPoint, *world);
        }

        // Step to the next vantage in the territory grid
        broadcastPoint = territory.nextVantage(broadcastPoint, heading);
    }

    void forage(){
        if (dead) return;
        switch(state){
            case 0: moveToListeningPoint(); break;
            case 1: receiveSignal(); break;
            case 2: independentSearch(); break; // independent search — scout is dead
            case 3: die(); break;
        }
        if (ant->energy <= 1) die();
    }
};

class Scout{ // wrapper for scout ant; allows us to track other info for scout
    public: 
    Ant* ant;
    int state = 0;
    bool dead = false;
    int signalCount = 0;
    int cachedRelativeFirst = 0;
    int cachedRelativeSecond = 0;
    Territory territory;
    Coord heading;
    Coord vantagePoint;
    AntWorld* world;

    Scout(Ant* ant, Territory territory, AntWorld& world){
        this->ant = ant;
        this->territory = territory;
        this->vantagePoint = territory.startPoint(ant->foodRadius);
        this->heading = territory.startHeading(ant->foodRadius);
        this->world = &world;
    }

    void assumeVantage(){
        safeMove(*ant, vantagePoint, *world);
        if (ant->position == vantagePoint){
            state++; // reached vantage, begin signalling
        } else {
            // Can't reach vantage — insufficient energy for the terrain cost.
            // nextVantage already guarantees in-bounds coords, so this is a genuine
            // energy failure. Die so carriers detect it via the (0,0) signal.
            state = 3;
        }
    }

    void signalCoord(Coord foodCoord){
        ant->erasePheromone(world->pheromoneMap);
        if (signalCount == 0){ // cache the relative offsets once, so all 6 bits describe the same food item
            cachedRelativeFirst  = foodCoord.first  - ant->position.first  + ant->foodRadius;
            cachedRelativeSecond = foodCoord.second - ant->position.second + ant->foodRadius;
        }
        if (signalCount < 6){
            if (getSignalBit(cachedRelativeFirst, cachedRelativeSecond, signalCount)){
                ant->dropPheromone(world->pheromoneMap);
            }
            signalCount++;
        }
        if (signalCount >= 6){ // this signal is over, check if food was removed, reset for the next
            signalCount = 0;
            if(world->foodMap[foodCoord.first][foodCoord.second] == 1){ // food wasn't picked up, do it its self
                safeMove(*ant, foodCoord, *world);
                ant->returnHome(world->terrainMap, world->foodMap);
                state = 3; // kills self, TODO: should be solo explore
            }
        }
    }

    void signalFood(){ // if sees food, signals it; if not, tells us to change state
        std::vector<Coord> visibleFood = ant->foodScan(world->foodMap);
        if (visibleFood.empty()){
            state++;
            return;
        }
        std::sort(visibleFood.begin(), visibleFood.end(), [this](Coord a, Coord b){
            return sortByDistance(a, b, ant->homeCoord, *world);
        });
        signalCoord(visibleFood[0]);
    }

    bool signalOver(){
        if (signalCount < 6){
            ant->dropPheromone(world->pheromoneMap);
            signalCount++;
            return true;
        }
        ant->erasePheromone(world->pheromoneMap);
        signalCount = 0;
        state = 0;
        // If the scout picked up food at this vantage (move() auto-collects on arrival),
        // drop it here before advancing. From the next vantage the food is at exactly
        // foodRadius distance and encodes as a non-zero relative coord — safe for carriers.
        if (ant->carryingFood){
            world->foodMap[ant->position.first][ant->position.second] = 1;
            ant->carryingFood = false;
        }
        vantagePoint = territory.nextVantage(vantagePoint, heading);
        return false;
    }

    void die(){
        this->dead = true;
        this->ant->energy = 1;
        this->ant->erasePheromone(world->pheromoneMap);
    }

    void forage(){
        if (dead) return;
        switch(state){
            case 0: assumeVantage(); break;
            case 1: signalFood(); break;
            case 2: signalOver(); break;
            case 3: die(); break;
        }
        if (ant->energy <= 1) die();
    }
};

void assignRoles(std::vector<Ant>& ants, std::vector<Scout>& scouts, std::vector<Carrier>& carriers, AntWorld& world, int numScouts = 4){
    if (ants.empty()) return;

    std::sort(ants.begin(), ants.end(), [](const Ant& a, const Ant& b) { return a.energy > b.energy; });
    size_t scoutCount = std::min(static_cast<size_t>(numScouts), ants.size());
    for (size_t i = 0; i < scoutCount; ++i){
        // angular slice of the map (from home) that the scout is responsible for searching
        Territory territory(ants[i].homeCoord, (i) * PI / numScouts, (i + 1) * PI / numScouts, world.terrainMap);
        scouts.emplace_back(&ants[i], territory, world);
    }
    for (size_t i = scoutCount; i < ants.size(); ++i){
        const auto& scout = scouts[i % scouts.size()];
        carriers.emplace_back(&ants[i], scout.territory, scout.vantagePoint, scout.heading, world);
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
    // TODO: if no ant can make the round trip (shouldn't really happen)
}

void signallingAnts(AntWorld& world){ // handles first stage control, team creation, and team organization
    static int phase = 0;
    static std::vector<Scout> scouts;
    static std::vector<Carrier> carriers;
    switch (phase){
        case 0: 
            clearImmediateArea(world.ants, world);
            assignRoles(world.ants, scouts, carriers, world);
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