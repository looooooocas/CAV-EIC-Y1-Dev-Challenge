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
        RETURNING_TO_STORED,
        DEAD
    };

    Ant* ant{nullptr};
    Territory territory;
    Coord currentVantage{0, 0};
    Coord heading{0, 0};
    Coord storedCoord{0, 0};
    State state{EXPLORING};
    bool dead{false};
    AntWorld* world{nullptr};

    SimpleAnt() = default;

    SimpleAnt(Ant* ant, const Territory& territory, AntWorld& world)
        : ant(ant), territory(territory), world(&world) {
        this->currentVantage = this->territory.startPoint(ant->foodRadius);
        this->heading = this->territory.startHeading(ant->foodRadius);
        this->storedCoord = this->currentVantage;
        this->state = EXPLORING;
        this->dead = false;
    }

    void explore() {
        if (ant->energy <= 1) {
            die();
            return;
        }

        // Safety: check if ant has enough energy to return home from its current position
        int homeCost = shortestPathCost(ant->position, ant->homeCoord, *world);
        if (ant->energy <= homeCost + 2) {
            safeMove(*ant, ant->homeCoord, *world);
            if (ant->position == ant->homeCoord) {
                state = WAIT_FOR_SCORE;
            }
            return;
        }

        // 1. Scan for visible food at current position
        std::vector<Coord> visibleFood = ant->foodScan(world->foodMap);
        if (!visibleFood.empty()) {
            if (grabAndDeliverFood(visibleFood)) {
                return;
            }
        }

        // 2. If at current vantage, compute the next vantage along the patrol route
        if (ant->position == currentVantage) {
            currentVantage = territory.nextVantage(currentVantage, heading);
        }

        // 3. Move toward current vantage
        safeMove(*ant, currentVantage, *world);

        // 4. Scan again upon reaching new position
        std::vector<Coord> newVisible = ant->foodScan(world->foodMap);
        if (!newVisible.empty()) {
            grabAndDeliverFood(newVisible);
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
            if (tripCost < ant->energy) {
                targetFood = f;
                break;
            }
        }

        // If no visible food can be safely collected and brought home, do not collect
        if (targetFood.first == -1) {
            return false;
        }

        // Store current position before leaving to grab food
        storedCoord = ant->position;

        // Move to target food
        safeMove(*ant, targetFood, *world);

        if (ant->carryingFood) {
            // Take food home
            safeMove(*ant, ant->homeCoord, *world);
            if (ant->position == ant->homeCoord) {
                state = WAIT_FOR_SCORE;
            } else {
                state = RETURNING_HOME;
            }
            return true;
        } else {
            state = EXPLORING;
            return false;
        }
    }

    void returnHome() {
        if (ant->energy <= 1) {
            die();
            return;
        }
        safeMove(*ant, ant->homeCoord, *world);
        if (ant->position == ant->homeCoord) {
            state = WAIT_FOR_SCORE;
        }
    }

    void waitForScore() {
        // Wait a turn for the food to be scored in updateWorld().
        // On the next turn, begin journey back to where the ant left off.
        state = RETURNING_TO_STORED;
    }

    void returnToStored() {
        if (ant->energy <= 1) {
            die();
            return;
        }

        // Safety: ensure ant has enough energy to reach storedCoord AND return home
        int roundTripStored = shortestPathCost(ant->position, storedCoord, *world) + 
                              shortestPathCost(storedCoord, ant->homeCoord, *world);
        if (roundTripStored >= ant->energy) {
            // Cannot afford to return all the way to storedCoord and come back; explore locally
            state = EXPLORING;
            explore();
            return;
        }

        Coord prevPos = ant->position;
        safeMove(*ant, storedCoord, *world);
        if (ant->position == storedCoord || ant->position == prevPos) {
            state = EXPLORING;
            explore();
        }
    }

    void die() {
        this->dead = true;
        this->state = DEAD;
        this->ant->energy = 1;
    }

    void forage() {
        if (dead) return;
        switch (state) {
            case EXPLORING:           explore(); break;
            case RETURNING_HOME:      returnHome(); break;
            case WAIT_FOR_SCORE:      waitForScore(); break;
            case RETURNING_TO_STORED: returnToStored(); break;
            case DEAD:                die(); break;
        }
        if (ant->energy <= 1) die();
    }
};

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