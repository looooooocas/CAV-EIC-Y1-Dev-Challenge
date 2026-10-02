/**
 * @file applicant_solution.cpp
 * @brief Complete applicant solution for the AntWorld foraging challenge.
 *
 * Architecture Overview:
 *  - Challenge Entry Point: AntWorld::forage()
 *  - Section 1: Coordinate Math & Pathfinding Utilities (safeMove, shortestPathCost)
 *  - Section 2: Geometric Territory Partitioning (equal-cell angular decomposition)
 *  - Section 3: SimpleAnt Finite State Machine & Autonomous Foraging Controller
 *  - Section 4: Edge-Case Unit Testing Suite (SimpleAnt & Territory verification)
 */

#include "../include/antworld.h"
#include "utility_functions.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

// Forward declaration of simpleAnt foraging controller
namespace {
void simpleAnt(AntWorld& world);
}

// ============================================================================
// CHALLENGE ENTRY POINT: AntWorld::forage()
// ============================================================================

/**
 * @brief Primary foraging entry point invoked on every simulation tick.
 * Executes autonomous SimpleAnt agents across partitioned territorial sectors.
 */
void AntWorld::forage() {
    simpleAnt(*this);
}

// ============================================================================
// SECTION 1: COORDINATE MATH & SAFE PATHFINDING UTILITIES
// ============================================================================

Coord operator+(Coord a, Coord b) {
    return Coord(a.first + b.first, a.second + b.second);
}

Coord operator-(Coord a, Coord b) {
    return Coord(a.first - b.first, a.second - b.second);
}

Coord operator*(Coord a, int scalar) {
    return Coord(a.first * scalar, a.second * scalar);
}

/**
 * @brief Computes shortest path energy cost between two coordinates.
 * Returns 999999 for out-of-bounds queries.
 */
int shortestPathCost(Coord start, Coord dest, AntWorld& world) {
    int rows = static_cast<int>(world.terrainMap.size());
    int cols = static_cast<int>(world.terrainMap[0].size());
    if (start.first < 0 || start.first >= rows || start.second < 0 || start.second >= cols ||
        dest.first < 0 || dest.first >= rows || dest.second < 0 || dest.second >= cols) {
        return 999999;
    }
    return calculatePathCost(world.terrainMap, shortestPath(world.terrainMap, start, dest));
}

bool sortByDistance(Coord a, Coord b, Coord referencePoint, AntWorld& world) {
    return shortestPathCost(a, referencePoint, world) < shortestPathCost(b, referencePoint, world);
}

/**
 * @brief Moves an ant along the shortest path toward dest while strictly guaranteeing
 * that ant.energy >= 1 at all times. Automatically shortens the path if full distance
 * cannot be afforded in a single turn.
 */
Coord safeMove(Ant& ant, Coord dest, AntWorld& world) {
    int rows = static_cast<int>(world.terrainMap.size());
    int cols = static_cast<int>(world.terrainMap[0].size());
    if (dest.first < 0 || dest.first >= rows || dest.second < 0 || dest.second >= cols) {
        return ant.position;
    }

    std::vector<Coord> path = shortestPath(world.terrainMap, ant.position, dest);
    while (!path.empty()) {
        Coord end = path.back();
        int pathCost = shortestPathCost(ant.position, end, world);
        if (pathCost < ant.energy) {
            ant.move(world.terrainMap, end, world.foodMap);
            return ant.position;
        } else {
            path.pop_back(); // Shorten path to fit within current energy budget
        }
    }
    return ant.position;
}

/**
 * @brief Determines if an ant can complete a round-trip to dest and back home without dying.
 */
bool roundTrip(Ant& ant, Coord dest, AntWorld& world) {
    int tripCost = shortestPathCost(ant.position, dest, world) + shortestPathCost(dest, ant.homeCoord, world);
    if (tripCost < ant.energy) {
        safeMove(ant, dest, world);
        safeMove(ant, ant.homeCoord, world);
        return true;
    }
    return false;
}

// ============================================================================
// SECTION 2: GEOMETRIC TERRITORY PARTITIONING
// ============================================================================

inline constexpr double PI = 3.14159265358979323846;

struct Territory {
    Coord home{0, 0};
    double start{0.0};
    double end{0.0};
    double mid{0.0};
    int scoutIndex{0};
    int totalScouts{1};
    const MapTemplate* map{nullptr};

    Territory() = default;

    Territory(Coord homeCoord, int scoutIdx, int numScouts, const MapTemplate& terrainMap)
        : home(homeCoord), scoutIndex(scoutIdx), totalScouts(numScouts), map(&terrainMap) {
        assignSector();
    }

    void assignSector() {
        int rows = static_cast<int>(map->size());
        int cols = static_cast<int>((*map)[0].size());

        // Count reachable non-home cells by angle
        std::vector<double> angles;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (r == home.first && c == home.second) continue;
                double a = std::atan2(c - home.second, r - home.first);
                if (a < 0.0) a += 2.0 * PI;
                angles.push_back(a);
            }
        }
        std::sort(angles.begin(), angles.end());

        // Partition angles equally among scouts
        int totalCells = static_cast<int>(angles.size());
        int startIdx = (scoutIndex * totalCells) / totalScouts;
        int endIdx = ((scoutIndex + 1) * totalCells) / totalScouts;

        start = angles[startIdx];
        end = (scoutIndex + 1 == totalScouts) ? (angles.front() + 2.0 * PI) : angles[endIdx];
        mid = (start + end) / 2.0;
    }

    bool within(Coord coord) const {
        if (coord.first == home.first && coord.second == home.second) return true;

        double a = std::atan2(coord.second - home.second, coord.first - home.first);
        if (a < 0.0) a += 2.0 * PI;

        if (start < end) {
            return (a >= start && a < end);
        } else {
            // Wraps around 2*PI boundary
            return (a >= start || a < end);
        }
    }

    Coord startPoint(int foodRadius) const {
        int rows = static_cast<int>(map->size());
        int cols = static_cast<int>((*map)[0].size());

        double step = std::max(1.0, foodRadius * 0.75);
        double maxDist = std::hypot(rows, cols);

        Coord best = home;
        for (double d = step; d < maxDist; d += step) {
            int r = static_cast<int>(std::round(home.first + d * std::cos(start)));
            int c = static_cast<int>(std::round(home.second + d * std::sin(start)));

            if (r < 0 || r >= rows || c < 0 || c >= cols) break;

            Coord candidate(r, c);
            if (candidate != home) {
                best = candidate;
                break;
            }
        }

        if (best == home) {
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = home.first + dr;
                    int nc = home.second + dc;
                    if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {
                        return Coord(nr, nc);
                    }
                }
            }
        }
        return best;
    }

    Coord startHeading(int foodRadius) {
        Coord startCoord = startPoint(foodRadius);
        int rows = static_cast<int>(map->size());
        int cols = static_cast<int>((*map)[0].size());

        Coord homeToStart = startCoord - home;
        double radialDist = std::hypot(homeToStart.first, homeToStart.second);

        Coord targetOnEndRay(
            static_cast<int>(std::round(home.first + radialDist * std::cos(end))),
            static_cast<int>(std::round(home.second + radialDist * std::sin(end)))
        );

        Coord sweepVector = targetOnEndRay - startCoord;
        if (sweepVector == Coord(0, 0)) {
            sweepVector = Coord(-homeToStart.second, homeToStart.first);
        }

        double sweepLength = std::hypot(sweepVector.first, sweepVector.second);
        Coord heading(0, 0);
        if (sweepLength > 1e-4) {
            heading = Coord(
                static_cast<int>(std::round((sweepVector.first / sweepLength) * foodRadius)),
                static_cast<int>(std::round((sweepVector.second / sweepLength) * foodRadius))
            );
        }

        Coord nextCoord = startCoord + heading;
        Coord clampedCoord(
            std::clamp(nextCoord.first, 0, rows - 1),
            std::clamp(nextCoord.second, 0, cols - 1)
        );
        heading = clampedCoord - startCoord;

        // Ensure non-zero heading scaled by foodRadius
        if (heading == Coord(0, 0)) {
            if (startCoord.first + 1 < rows) heading = Coord(1, 0);
            else if (startCoord.first - 1 >= 0) heading = Coord(-1, 0);
            else if (startCoord.second + 1 < cols) heading = Coord(0, 1);
            else heading = Coord(0, -1);

            Coord fallbackNext = startCoord + heading * foodRadius;
            Coord clampedFallback(
                std::clamp(fallbackNext.first, 0, rows - 1),
                std::clamp(fallbackNext.second, 0, cols - 1)
            );
            heading = clampedFallback - startCoord;
        }

        return heading;
    }

    Coord nextVantage(Coord currentVantage, Coord& heading) {
        int rows = static_cast<int>(map->size());
        int cols = static_cast<int>((*map)[0].size());

        // Step 1: Try stepping straight along heading
        Coord straight = currentVantage + heading;
        if (straight.first >= 0 && straight.first < rows &&
            straight.second >= 0 && straight.second < cols &&
            within(straight)) {
            return straight;
        }

        // Step 2: Hit territory or map boundary; reverse heading
        heading = heading * -1;
        Coord reversed = currentVantage + heading;
        if (reversed.first >= 0 && reversed.first < rows &&
            reversed.second >= 0 && reversed.second < cols &&
            within(reversed)) {
            return reversed;
        }

        // Step 3: Shift radially outward along sector bisector
        int foodRadius = static_cast<int>(std::round(std::hypot(heading.first, heading.second)));
        if (foodRadius < 1) foodRadius = 1;

        Coord outward(
            static_cast<int>(std::round(currentVantage.first + foodRadius * std::cos(mid))),
            static_cast<int>(std::round(currentVantage.second + foodRadius * std::sin(mid)))
        );
        if (outward.first >= 0 && outward.first < rows &&
            outward.second >= 0 && outward.second < cols &&
            within(outward)) {
            heading = startHeading(foodRadius);
            return outward;
        }

        // Step 4: Fallback to any valid neighbor within territory
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr == 0 && dc == 0) continue;
                Coord candidate(currentVantage.first + dr, currentVantage.second + dc);
                if (candidate.first >= 0 && candidate.first < rows &&
                    candidate.second >= 0 && candidate.second < cols &&
                    within(candidate) && candidate != home) {
                    return candidate;
                }
            }
        }

        // Step 5: Move toward map center if trapped
        Coord center(rows / 2, cols / 2);
        int stepR = (center.first > currentVantage.first) ? 1 : ((center.first < currentVantage.first) ? -1 : 0);
        int stepC = (center.second > currentVantage.second) ? 1 : ((center.second < currentVantage.second) ? -1 : 0);
        Coord centerStep(currentVantage.first + stepR, currentVantage.second + stepC);
        if (centerStep.first >= 0 && centerStep.first < rows &&
            centerStep.second >= 0 && centerStep.second < cols) {
            return centerStep;
        }

        return currentVantage;
    }
};

// ============================================================================
// SECTION 3: SIMPLEANT FINITE STATE MACHINE & FORAGING CONTROLLER
// ============================================================================

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
    Coord returnCoord{0, 0};
    Coord heading{0, 0};
    State state{EXPLORING};
    bool dead{false};
    AntWorld* world{nullptr};

    SimpleAnt() = default;

    SimpleAnt(Ant* antPtr, const Territory& terr, AntWorld& antWorld)
        : ant(antPtr), territory(terr), world(&antWorld) {
        this->currentVantage = territory.startPoint(ant->foodRadius);
        this->targetVantage = this->currentVantage;
        this->heading = territory.startHeading(ant->foodRadius);
        this->returnCoord = this->currentVantage;
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
            } else {
                state = RETURNING_HOME;
            }
            return;
        }

        // 1. Scan for visible food at current position
        std::vector<Coord> visibleFood = ant->foodScan(world->foodMap);
        if (grabAndDeliverFood(visibleFood)) {
            return;
        }

        // 2. Advance toward current target vantage point
        if (ant->position != targetVantage) {
            Coord prevPos = ant->position;
            safeMove(*ant, targetVantage, *world);

            if (ant->position == prevPos) {
                // Stuck: retreat to home to prevent starvation
                safeMove(*ant, ant->homeCoord, *world);
                state = WAIT_FOR_SCORE;
                return;
            }

            visibleFood = ant->foodScan(world->foodMap);
            if (grabAndDeliverFood(visibleFood)) {
                return;
            }
        }

        // 3. Reached vantage: scan and advance to next vantage in patrol pattern
        if (ant->position == targetVantage) {
            currentVantage = targetVantage;
            visibleFood = ant->foodScan(world->foodMap);
            if (grabAndDeliverFood(visibleFood)) {
                return;
            }
            targetVantage = territory.nextVantage(currentVantage, heading);
        }
    }

    bool grabAndDeliverFood(std::vector<Coord>& visibleFood) {
        if (visibleFood.empty()) return false;

        sortByDistance(visibleFood.front(), visibleFood.back(), ant->position, *world);
        Coord bestFood = visibleFood.front();

        // Feasibility check: can the ant reach food and return home safely?
        int tripCost = shortestPathCost(ant->position, bestFood, *world) +
                       shortestPathCost(bestFood, ant->homeCoord, *world);

        if (tripCost < ant->energy) {
            returnCoord = ant->position;
            safeMove(*ant, bestFood, *world);
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
        // Wait a turn for food to be scored in updateWorld().
        // On next turn, begin journey back to where ant left off.
        state = RETURNING_TO_STORED;
    }

    void returnToStored() {
        if (ant->energy <= 1) {
            die();
            return;
        }

        // Safety: ensure ant has enough energy to reach storedCoord AND return home
        int tripCost = shortestPathCost(ant->position, returnCoord, *world) +
                       shortestPathCost(returnCoord, ant->homeCoord, *world);

        if (tripCost + 2 >= ant->energy) {
            state = EXPLORING;
            explore();
            return;
        }

        safeMove(*ant, returnCoord, *world);
        if (ant->position == returnCoord) {
            state = EXPLORING;
        }
    }

    void die() {
        dead = true;
        if (ant->energy > 0) {
            ant->energy = 1;
        }
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

void simpleAnt(AntWorld& world) {
    static AntWorld* currentWorld = nullptr;
    static std::vector<SimpleAnt> ants;
    if (currentWorld != &world || ants.size() != world.ants.size()) {
        ants.clear();
        currentWorld = &world;
        int numAnts = static_cast<int>(world.ants.size());
        for (int i = 0; i < numAnts; ++i) {
            Territory territory(world.ants[i].homeCoord, i, numAnts, world.terrainMap);
            ants.emplace_back(&world.ants[i], territory, world);
        }
    }

    for (auto& sa : ants) {
        sa.forage();
    }

    bool allExhausted = std::all_of(world.ants.begin(), world.ants.end(), [](const Ant& ant) {
        return ant.energy <= 2;
    });
    if (allExhausted) {
        for (auto& ant : world.ants) {
            ant.energy = 0;
        }
    }
}

} // namespace

// ============================================================================
// SECTION 4: EDGE-CASE UNIT TESTING SUITE
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

// --- SimpleAnt Edge Case Tests ---

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

static bool testExploreSafetyRetreat() {
    SimpleAntFixture f({3, 0}, 5); // homeCost is 3; energy 5 <= 3 + 2 forces retreat
    f.sa.explore();
    assert(f.sa.ant->position == f.world.homeCoordinates);
    assert(f.sa.state == SimpleAnt::WAIT_FOR_SCORE);
    return true;
}

static bool testExploreStuckDetection() {
    SimpleAntFixture f({2, 2}, 10);
    f.world.homeCoordinates = {2, 2};
    f.world.ants[0].homeCoord = {2, 2};
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

static bool testReturnToStoredEnergyEdgeCase() {
    SimpleAntFixture f({0, 0}, 10);
    f.sa.returnCoord = {4, 4}; // tripCost is 8 + 8 = 16 > energy 10
    f.sa.state = SimpleAnt::RETURNING_TO_STORED;
    f.sa.returnToStored();
    assert(f.sa.state != SimpleAnt::RETURNING_TO_STORED);
    return true;
}

static bool testDeathAndPassivity() {
    SimpleAntFixture f({0, 0}, 1);
    f.sa.forage();
    assert(f.sa.dead);
    assert(f.sa.ant->energy == 1);
    f.sa.forage(); // No-op when dead
    assert(f.sa.dead);
    return true;
}

// --- Territory Edge Case Tests ---

static bool testSingleScoutFullCircle() {
    MapTemplate map(10, std::vector<int>(10, 0));
    Territory t(Coord(5, 5), 0, 1, map);
    assert(t.start == 0.0);
    assert(std::abs(t.end - 2.0 * PI) < 1e-4);
    assert(t.within(Coord(0, 0)));
    assert(t.within(Coord(9, 9)));
    assert(t.within(Coord(5, 5)));
    return true;
}

static bool testCornerScoutAngles() {
    MapTemplate map(10, std::vector<int>(10, 0));
    Territory t(Coord(0, 0), 0, 1, map);
    assert(t.within(Coord(0, 0)));
    assert(t.within(Coord(9, 9)));
    assert(t.within(Coord(0, 9)));
    assert(t.within(Coord(9, 0)));
    return true;
}

static bool testMapBoundaryBounce() {
    MapTemplate map(10, std::vector<int>(10, 0));
    Territory t(Coord(5, 5), 0, 1, map);
    Coord heading(0, 3);
    Coord v(5, 8); // One step forward would land at (5, 11) - out of bounds
    Coord next = t.nextVantage(v, heading);
    assert(next.first >= 0 && next.first < 10);
    assert(next.second >= 0 && next.second < 10);
    assert(heading.second < 0); // Heading reversed upon collision
    return true;
}

static bool testSectorBoundaryBounce() {
    MapTemplate map(10, std::vector<int>(10, 0));
    Territory t(Coord(5, 5), 0, 4, map);
    Coord sp = t.startPoint(2);
    Coord heading = t.startHeading(2);
    Coord v = sp;
    for (int step = 0; step < 10; ++step) {
        v = t.nextVantage(v, heading);
        assert(v.first >= 0 && v.first < 10);
        assert(v.second >= 0 && v.second < 10);
        assert(t.within(v));
    }
    return true;
}

static bool testTinyMapEdgeCase() {
    MapTemplate map(2, std::vector<int>(2, 0));
    Territory t(Coord(0, 0), 0, 1, map);
    Coord sp = t.startPoint(5);
    assert(sp.first >= 0 && sp.first < 2);
    assert(sp.second >= 0 && sp.second < 2);
    Coord heading = t.startHeading(5);
    Coord next = sp + heading;
    assert(next.first >= 0 && next.first < 2);
    assert(next.second >= 0 && next.second < 2);
    return true;
}

} // namespace

// State machine end-to-end transition test
bool testSimpleSearchTransitions() {
    AntWorld world(0, 10, 10, 1);
    world.homeCoordinates = {0, 0};
    world.terrainMap = MapTemplate(10, std::vector<int>(10, 0));
    world.foodMap = MapTemplate(10, std::vector<int>(10, 0));
    world.ants[0].position = {0, 0};
    world.ants[0].homeCoord = {0, 0};
    world.ants[0].energy = 150;

    Territory territory(world.homeCoordinates, 0, 1, world.terrainMap);
    SimpleAnt antWrapper(&world.ants[0], territory, world);

    assert(antWrapper.state == SimpleAnt::EXPLORING);

    // Step 1: Advance ant along its patrol route away from home
    antWrapper.forage();
    Coord patrolPos = antWrapper.ant->position;
    assert(patrolPos != world.homeCoordinates);

    // Step 2: Grab adjacent food and return home
    Coord foodPos(std::min(9, patrolPos.first + 1), patrolPos.second);
    world.foodMap[foodPos.first][foodPos.second] = 1;
    antWrapper.forage();
    assert(antWrapper.ant->position == world.homeCoordinates);
    assert(antWrapper.state == SimpleAnt::WAIT_FOR_SCORE);
    assert(antWrapper.returnCoord == patrolPos);

    // Step 3: Wait at home for scoring cycle
    world.updateWorld();
    antWrapper.forage();
    assert(antWrapper.ant->position == world.homeCoordinates);
    assert(antWrapper.state == SimpleAnt::RETURNING_TO_STORED);

    // Step 4: Travel back to returnCoord, transitioning back to EXPLORING
    antWrapper.forage();
    assert(antWrapper.state == SimpleAnt::EXPLORING);

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

void territoryTests() {
    std::cout << "\n--- Running Territory Edge-Case Unit Tests ---\n";
    assert(testSingleScoutFullCircle());
    std::cout << "  PASS: Single scout 360-degree sector coverage\n";

    assert(testCornerScoutAngles());
    std::cout << "  PASS: Corner home coordinate angle boundaries\n";

    assert(testMapBoundaryBounce());
    std::cout << "  PASS: Map boundary collision heading reflection\n";

    assert(testSectorBoundaryBounce());
    std::cout << "  PASS: Sector boundary constraint adherence\n";

    assert(testTinyMapEdgeCase());
    std::cout << "  PASS: Tiny grid bounds clamping with large foodRadius\n";

    std::cout << "All Territory edge-case tests PASSED!\n\n";
}
