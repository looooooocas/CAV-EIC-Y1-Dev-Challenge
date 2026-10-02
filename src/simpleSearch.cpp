#include "../include/simpleSearch.h"
#include "../include/utilities.h"
#include <algorithm>
#include <vector>
#include <iostream>
#include <cassert>

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

// ============================================================================
// Edge-Case Unit Tests
// ============================================================================

namespace {

struct SimpleAntFixture {
    AntWorld world{0, 10, 10, 1};
    Territory territory;
    SimpleAnt sa;

    SimpleAntFixture(Coord antPos = {0, 0}, int energy = 100)
        : territory(Coord(0, 0), 0, 1, world.terrainMap) {
        world.homeCoordinates = {0, 0};
        world.terrainMap = MapTemplate(10, std::vector<int>(10, 0));
        world.foodMap = MapTemplate(10, std::vector<int>(10, 0));
        world.ants[0].position = antPos;
        world.ants[0].homeCoord = {0, 0};
        world.ants[0].energy = energy;
        sa = SimpleAnt(&world.ants[0], territory, world);
        sa.currentVantage = antPos;
        sa.targetVantage = antPos;
    }
};

} // namespace

// 1. Food trip rejected when ant cannot safely make the round-trip home
static bool testFoodEnergyEdgeCase() {
    SimpleAntFixture f({5, 5}, 4); // round trip to (5,6) and back home requires 13 energy
    f.world.foodMap[5][6] = 1;
    std::vector<Coord> visible = {{5, 6}};
    assert(!f.sa.grabAndDeliverFood(visible));
    assert(f.world.foodMap[5][6] == 1);
    assert(f.sa.ant->position == Coord(5, 5));
    assert(!f.sa.ant->carryingFood);
    return true;
}

// 2. Exploration immediately retreats home when energy <= homeCost + 2
static bool testExploreSafetyRetreat() {
    SimpleAntFixture f({3, 0}, 5); // homeCost is 3; energy 5 <= 3 + 2 forces retreat
    f.sa.explore();
    assert(f.sa.ant->position == f.world.homeCoordinates);
    assert(f.sa.state == SimpleAnt::WAIT_FOR_SCORE);
    return true;
}

// 3. Blocked ant unable to move triggers headHome()
static bool testExploreStuckDetection() {
    SimpleAntFixture f({2, 2}, 10);
    f.world.homeCoordinates = {2, 2};
    f.world.ants[0].homeCoord = {2, 2};
    // Surround ant with impassable elevation
    for (int r = 1; r <= 3; ++r) {
        for (int c = 1; c <= 3; ++c) {
            if (r != 2 || c != 2) f.world.terrainMap[r][c] = 50;
        }
    }
    f.sa.currentVantage = {5, 5};
    f.sa.targetVantage = {5, 5};
    f.sa.explore();
    assert(f.sa.state == SimpleAnt::WAIT_FOR_SCORE);
    return true;
}

// 4. Return to stored aborts to EXPLORING if round trip is unaffordable
static bool testReturnToStoredEnergyEdgeCase() {
    SimpleAntFixture f({0, 0}, 10);
    f.sa.returnCoord = {6, 6}; // Round trip is 24 > 10
    f.sa.state = SimpleAnt::RETURNING_TO_STORED;
    f.sa.returnToStored();
    assert(f.sa.state == SimpleAnt::EXPLORING);
    return true;
}

// 5. Ant death at energy <= 1 and post-death passivity
static bool testDeathAndPassivity() {
    SimpleAntFixture f({3, 3}, 1);
    f.sa.forage();
    assert(f.sa.dead);
    f.sa.forage(); // No-op when dead
    assert(f.sa.ant->position == Coord(3, 3));
    assert(f.sa.ant->energy == 1);
    return true;
}

// 6. Complete state machine pipeline
bool testSimpleSearchTransitions() {
    SimpleAntFixture f({0, 0}, 150);
    assert(f.sa.state == SimpleAnt::EXPLORING);

    // Step 1: Advance along patrol route
    f.sa.forage();
    Coord patrolPos = f.sa.ant->position;
    assert(patrolPos != f.world.homeCoordinates);

    // Step 2: Grab adjacent food and return home
    Coord foodPos(std::min(9, patrolPos.first + 1), patrolPos.second);
    f.world.foodMap[foodPos.first][foodPos.second] = 1;
    f.sa.forage();
    assert(f.sa.ant->position == f.world.homeCoordinates);
    assert(f.sa.state == SimpleAnt::WAIT_FOR_SCORE);
    assert(f.sa.returnCoord == patrolPos);

    // Step 3: Wait at home for scoring cycle
    f.world.updateWorld();
    f.sa.forage();
    assert(f.sa.ant->position == f.world.homeCoordinates);
    assert(f.sa.state == SimpleAnt::RETURNING_TO_STORED);

    // Step 4: Travel back to returnCoord, transitioning back to EXPLORING
    f.sa.forage();
    assert(f.sa.state == SimpleAnt::EXPLORING);

    return true;
}

void tests() {
    std::cout << "\n--- Running SimpleAnt Edge-Case Unit Tests ---\n";
    assert(testFoodEnergyEdgeCase());
    std::cout << "  PASS: Food energy feasibility edge case\n";

    assert(testExploreSafetyRetreat());
    std::cout << "  PASS: Explore safety retreat boundary\n";

    assert(testExploreStuckDetection());
    std::cout << "  PASS: Explore stuck detection\n";

    assert(testReturnToStoredEnergyEdgeCase());
    std::cout << "  PASS: ReturnToStored energy limit\n";

    assert(testDeathAndPassivity());
    std::cout << "  PASS: Death threshold and passivity\n";

    assert(testSimpleSearchTransitions());
    std::cout << "  PASS: State machine pipeline transitions\n";

    std::cout << "All SimpleAnt edge-case tests PASSED!\n\n";
}