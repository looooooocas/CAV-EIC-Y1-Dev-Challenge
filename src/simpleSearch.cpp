#include "../include/simpleSearch.h"
#include "../include/utilities.h"
#include <algorithm>
#include <vector>

namespace {

class SimpleAnt {
public:
    enum State {
        EXPLORING = 0,
        RETURNING_HOME,
        WAIT_FOR_SCORE,
        RETURNING_TO_STORED
    };

    Ant* ant{nullptr};
    Territory territory;
    Coord currentVantage{0, 0};
    Coord targetVantage{0, 0};
    Coord heading{0, 0};
    Coord returnCoord{0, 0};
    State state{EXPLORING};
    bool dead{false};
    AntWorld* world{nullptr};

    SimpleAnt() = default;

    SimpleAnt(Ant* ant, const Territory& territory, AntWorld& world)
        : ant(ant),
          territory(territory),
          currentVantage(this->territory.startPoint(ant->foodRadius)),
          targetVantage(currentVantage),
          heading(this->territory.startHeading(ant->foodRadius)),
          returnCoord(currentVantage),
          state(EXPLORING),
          dead(false),
          world(&world) {}

    void headHome() {
        safeMove(*ant, ant->homeCoord, *world);
        state = (ant->position == ant->homeCoord) ? WAIT_FOR_SCORE : RETURNING_HOME;
    }

    void explore() {
        // Safety: check if ant has enough energy to return home from its current position
        int homeCost = shortestPathCost(ant->position, ant->homeCoord, *world);
        // Prevents ants from going too far to return home
        if (ant->energy <= homeCost + 2) {
            headHome();
            return;
        }

        // 1. Scan for visible food at current position
        std::vector<Coord> visibleFood = ant->foodScan(world->foodMap);
        if (!visibleFood.empty()) {
            if (grabAndDeliverFood(visibleFood)) {
                return;
            }
        }

        // 2. If at current vantage or target vantage, advance along patrol route
        if (ant->position == currentVantage || ant->position == targetVantage) {
            currentVantage = territory.nextVantage(currentVantage, heading);
            targetVantage = currentVantage;
        }

        // 3. Check if any adjacent coordinates to currentVantage are cheaper from ant's position
        Coord bestVantage = currentVantage;
        int bestCost = shortestPathCost(ant->position, currentVantage, *world);

        int rows = static_cast<int>(world->terrainMap.size());
        int cols = static_cast<int>(world->terrainMap[0].size());

        static const std::vector<Coord> neighborOffsets = {
            {-1, -1}, {-1,  0}, {-1,  1},
            { 0, -1},           { 0,  1},
            { 1, -1}, { 1,  0}, { 1,  1}
        };

        for (const Coord& offset : neighborOffsets) {
            Coord neighbor = currentVantage + offset;
            if (neighbor.first >= 0 && neighbor.first < rows &&
                neighbor.second >= 0 && neighbor.second < cols) {
                int cost = shortestPathCost(ant->position, neighbor, *world);
                if (cost < bestCost) {
                    bestCost = cost;
                    bestVantage = neighbor;
                }
            }
        }
        targetVantage = bestVantage;

        // Move toward target vantage
        Coord prevPos = ant->position;
        safeMove(*ant, targetVantage, *world);

        if (ant->carryingFood) {
            returnCoord = ant->position;
            headHome();
            return;
        }

        if (ant->position == prevPos) {
            headHome();
            return;
        }
    }

    bool grabAndDeliverFood(std::vector<Coord>& visibleFood) {
        // Sort visible food by shortest path distance from ant position (closest first)
        std::sort(visibleFood.begin(), visibleFood.end(), [this](Coord a, Coord b) {
            return shortestPathCost(ant->position, a, *world) < shortestPathCost(ant->position, b, *world);
        });

        // Select the closest food item that the ant has energy to reach and return home from
        Coord targetFood(-1, -1);
        for (const auto& f : visibleFood) {
            int tripCost = shortestPathCost(ant->position, f, *world) + shortestPathCost(f, ant->homeCoord, *world);
            if (tripCost + 1 <= ant->energy) {
                targetFood = f;
                break;
            }
        }

        // If no visible food can be safely collected and brought home, do not collect
        if (targetFood == Coord(-1, -1)) {
            return false;
        }

        // Store current position before leaving to grab food
        returnCoord = ant->position;

        // Move to target food
        safeMove(*ant, targetFood, *world);

        if (ant->carryingFood) {
            headHome();
            return true;
        } else {
            state = EXPLORING;
            return false;
        }
    }

    void returnHome() {
        headHome();
    }

    void waitForScore() {
        // Wait a turn for the food to be scored in updateWorld().
        state = RETURNING_TO_STORED;
    }

    void returnToStored() {
        // Safety: ensure ant has enough energy to reach returnCoord AND return home
        int roundTripStored = shortestPathCost(ant->position, returnCoord, *world) + 
                              shortestPathCost(returnCoord, ant->homeCoord, *world);
        if (roundTripStored >= ant->energy) {
            // Cannot afford to return all the way to returnCoord and come back; explore locally
            state = EXPLORING;
            explore();
            return;
        }

        Coord prevPos = ant->position;
        safeMove(*ant, returnCoord, *world);
        if (ant->position == returnCoord || ant->position == prevPos) {
            state = EXPLORING;
            explore();
        }
    }

    void die() {
        dead = true;
        ant->energy = 1;
    }

    void forage() {
        if (dead) return;
        switch (state) {
            case EXPLORING:           explore(); break;
            case RETURNING_HOME:      returnHome(); break;
            case WAIT_FOR_SCORE:      waitForScore(); break;
            case RETURNING_TO_STORED: returnToStored(); break;
        }
        if (ant->energy <= 1) die();
    }
};

// Below is for testing and analysis

static std::vector<SimpleAnt> g_simpleAnts;
static AntWorld* g_currentWorld = nullptr;

void ensureInitialized(AntWorld& world) {
    bool needsReset = (g_currentWorld != &world) || 
                      g_simpleAnts.empty() || 
                      (g_simpleAnts.size() != world.ants.size()) ||
                      (g_simpleAnts[0].world != &world) ||
                      (g_simpleAnts[0].ant != &world.ants[0]) ||
                      (g_simpleAnts[0].territory.home != world.homeCoordinates) ||
                      (g_simpleAnts[0].territory.map != &world.terrainMap);

    if (needsReset) {
        g_currentWorld = &world;
        g_simpleAnts.clear();
        int numAnts = static_cast<int>(world.ants.size());
        for (int i = 0; i < numAnts; ++i) {
            Territory territory(world.ants[i].homeCoord, i, numAnts, world.terrainMap);
            g_simpleAnts.emplace_back(&world.ants[i], territory, world);
        }
    }
}

} // namespace

void resetSimpleSearch() {
    g_simpleAnts.clear();
    g_currentWorld = nullptr;
}

void simpleSearch(AntWorld& world) {
    ensureInitialized(world);

    for (auto& sa : g_simpleAnts) {
        sa.forage();
    }

    // When all ants are exhausted (<= 2 energy), zero energies to terminate simulation cleanly
    bool allExhausted = std::all_of(world.ants.begin(), world.ants.end(), [](const Ant& ant) {
        return ant.energy <= 2;
    });
    if (allExhausted) {
        for (auto& ant : world.ants) {
            ant.energy = 0;
        }
    }
}

void simpleSearch(Ant& ant, AntWorld& world) {
    ensureInitialized(world);

    for (size_t i = 0; i < world.ants.size(); ++i) {
        if (&world.ants[i] == &ant) {
            g_simpleAnts[i].forage();
            break;
        }
    }
}