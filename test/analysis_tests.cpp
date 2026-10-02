#include <../include/analysis.h>
#include <../include/antworld.h>
#include <../include/utilities.h>
#include <iostream>
#include <algorithm>
#include <numeric>
#include <vector>

namespace {
    int failures = 0;
    int checks = 0;

    bool approx(double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    }

    void check(bool condition, const std::string &description) {
        ++checks;
        if (!condition) {
            ++failures;
            std::cerr << "FAIL: " << description << '\n';
        }
    }

    // Clears food map and sets food at the given positions
    void putFoodAt(AntWorld& world, const std::vector<Coord>& positions) {
        int x = world.foodMap.size();
        int y = world.foodMap[0].size();

        MapTemplate map(x, std::vector<int>(y, 0));
        for (const auto& pos : positions) {
            map[pos.first][pos.second] = 1;
        }
        world.foodMap = map;
    }

    // Sets ant count and energies in the world
    void setAnts(AntWorld& world, const std::vector<int>& antEnergies) {
        if (world.ants.size() != antEnergies.size()) {
            world.ants.clear();
            for (size_t i = 0; i < antEnergies.size(); ++i) {
                world.ants.emplace_back(antEnergies[i], world.homeCoordinates);
            }
        } else {
            for (size_t i = 0; i < antEnergies.size(); ++i) {
                world.ants[i].energy = antEnergies[i];
            }
        }
    }

    // Calculates actual one-way Dijkstra travel energy from world.homeCoordinates to target
    int getTravelCost(const AntWorld& world, Coord target) {
        std::vector<Coord> path = shortestPath(world.terrainMap, world.homeCoordinates, target);
        int travelEnergy = 0;
        for (size_t i = 1; i < path.size(); ++i) {
            auto [r1, c1] = path[i - 1];
            auto [r2, c2] = path[i];
            travelEnergy += 1 + std::abs(world.terrainMap[r1][c1] - world.terrainMap[r2][c2]);
        }
        return travelEnergy;
    }

    // Calculates actual round-trip energy (one-way * 2)
    int getRoundTripCost(const AntWorld& world, Coord target) {
        return 2 * getTravelCost(world, target);
    }

    void testFoodAt() {
        AntWorld world(0, 5, 5, 2);
        std::vector<Coord> foodPositions = {{0, 0}, {1, 1}, {2, 2}};
        putFoodAt(world, foodPositions);
        std::vector<Coord> actualFoodPositions;

        for (size_t x = 0; x < world.foodMap.size(); x++) {
            for (size_t y = 0; y < world.foodMap[x].size(); y++) {
                if (world.foodMap[x][y] == 1) {
                    actualFoodPositions.push_back({static_cast<int>(x), static_cast<int>(y)});
                }
            }
        }
        check(actualFoodPositions == foodPositions, "Food positions set correctly");
        check(actualFoodPositions.size() == foodPositions.size(), "Food amount correct");

        foodPositions = {};
        actualFoodPositions.clear();
        putFoodAt(world, foodPositions);

        for (size_t x = 0; x < world.foodMap.size(); x++) {
            for (size_t y = 0; y < world.foodMap[x].size(); y++) {
                if (world.foodMap[x][y] == 1) {
                    actualFoodPositions.push_back({static_cast<int>(x), static_cast<int>(y)});
                }
            }
        }
        check(actualFoodPositions.empty(), "Empty food map correct");
    }

    void testSetAnts() {
        AntWorld world(0, 5, 5, 6);
        std::vector<int> antEnergies = {15, 7, 8, 2, 0, -1};
        setAnts(world, antEnergies);
        for (size_t i = 0; i < antEnergies.size(); i++) {
            check(world.ants[i].energy == antEnergies[i], "Ant energy set correctly");
        }

        // Test resizing when energy vector size differs
        std::vector<int> fewerEnergies = {20, 10};
        setAnts(world, fewerEnergies);
        check(world.ants.size() == 2, "Ant count adjusted to match energies");
        check(world.ants[0].energy == 20 && world.ants[1].energy == 10, "Adjusted ant energies correct");
    }

    void testNaiveOptimumFlatTerrain() {
        // Controlled flat terrain (elevation 0 everywhere, home at 0,0)
        AntWorld world(0, 12, 12, 2);
        world.homeCoordinates = {0, 0};
        world.terrainMap = MapTemplate(12, std::vector<int>(12, 0));

        // Foods at Manhattan distances 2, 3, 5 -> round trips: 4, 6, 10
        std::vector<Coord> foodPositions = {{0, 2}, {0, 3}, {5, 0}};
        putFoodAt(world, foodPositions);

        // 1. Easy case: total energy 20 >= 4 + 6 + 10 -> all 3 collected
        std::vector<int> antEnergies = {10, 10};
        setAnts(world, antEnergies);
        Analysis analysis(world);
        check(approx(analysis.naiveOptimum(antEnergies), 1.0), "Naive optimum flat: easy full collection");

        // 2. Insufficient energy: total energy 10 -> covers 4 + 6, cannot afford 10 -> 2/3
        antEnergies = {5, 5};
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(approx(analysis.naiveOptimum(antEnergies), 2.0 / 3.0), "Naive optimum flat: insufficient energy (2/3)");

        // 3. Fail case: total energy 3 < 4 -> 0 collected
        antEnergies = {1, 2};
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(approx(analysis.naiveOptimum(antEnergies), 0.0), "Naive optimum flat: impossible trip (0/3)");
    }

    void testNaiveOptimumRealTerrain() {
        // Real procedural world with terrain elevation and random home coordinates
        AntWorld world(0, 12, 12, 2);

        // Pick 3 distinct coordinates that are not home
        std::vector<Coord> candidates = {{1, 1}, {6, 2}, {11, 11}};
        for (auto& c : candidates) {
            if (c == world.homeCoordinates) {
                c = {(c.first + 3) % 12, (c.second + 3) % 12};
            }
        }

        // Measure actual terrain travel costs from world.homeCoordinates
        struct FoodCost {
            Coord coord;
            int oneWay;
            int roundTrip;
        };
        std::vector<FoodCost> items;
        for (const auto& c : candidates) {
            int ow = getTravelCost(world, c);
            items.push_back({c, ow, 2 * ow});
        }
        std::sort(items.begin(), items.end(), [](const FoodCost& a, const FoodCost& b) {
            return a.roundTrip < b.roundTrip;
        });

        std::vector<Coord> foodPositions = {items[0].coord, items[1].coord, items[2].coord};
        putFoodAt(world, foodPositions);

        int cost1 = items[0].roundTrip;
        int cost2 = items[1].roundTrip;
        int cost3 = items[2].roundTrip;

        // 1. Full coverage on real terrain
        int totalNeeded = cost1 + cost2 + cost3;
        std::vector<int> antEnergies = {totalNeeded / 2, totalNeeded - (totalNeeded / 2)};
        setAnts(world, antEnergies);
        Analysis analysis(world);
        check(approx(analysis.naiveOptimum(antEnergies), 1.0), "Naive optimum real terrain: full collection");

        // 2. Partial coverage on real terrain (exact budget for cheapest 2 items)
        int budgetTwo = cost1 + cost2;
        antEnergies = {budgetTwo / 2, budgetTwo - (budgetTwo / 2)};
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(approx(analysis.naiveOptimum(antEnergies), 2.0 / 3.0), "Naive optimum real terrain: partial collection (2/3)");

        // 3. Zero coverage on real terrain
        antEnergies = {cost1 / 2, (cost1 > 0 ? (cost1 - 1) / 2 : 0)};
        if (std::accumulate(antEnergies.begin(), antEnergies.end(), 0) >= cost1) {
            antEnergies = {0, 0};
        }
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(approx(analysis.naiveOptimum(antEnergies), 0.0), "Naive optimum real terrain: insufficient energy for any item");

        // 4. Energy pooling / fungibility verification:
        // 4 ants with low individual energy, each insufficient for cost1, but summing to cost1
        if (cost1 >= 4) {
            std::vector<int> pooledAnts = {cost1 / 4, cost1 / 4, cost1 / 4, cost1 - 3 * (cost1 / 4)};
            setAnts(world, pooledAnts);
            analysis = Analysis(world);
            // Naive pools energy so it can collect the first food item (1 out of 3)
            check(approx(analysis.naiveOptimum(pooledAnts), 1.0 / 3.0), "Naive optimum real terrain: energy pooling");
        }
    }

    void testTerrainElevationImpact() {
        // Demonstrates explicitly why accounting for terrain matters:
        // Target is 3 steps away from home.
        // On flat terrain: one-way cost = 3, round-trip = 6.
        // On steep terrain: one-way cost = 3 + 3*2 = 9, round-trip = 18.
        AntWorld world(0, 5, 5, 1);
        world.homeCoordinates = {0, 0};
        Coord target = {0, 3};
        putFoodAt(world, {target});

        // Flat terrain setup
        world.terrainMap = MapTemplate(5, std::vector<int>(5, 0));
        std::vector<int> antEnergy = {6};
        setAnts(world, antEnergy);
        Analysis analysisFlat(world);
        check(approx(analysisFlat.naiveOptimum(antEnergy), 1.0), "Elevation impact: flat terrain collects food with RT energy 6");

        // Mountain ridge setup (elevations 0 -> 2 -> 4 -> 6)
        world.terrainMap = MapTemplate(5, std::vector<int>(5, 0));
        world.terrainMap[0][1] = 2;
        world.terrainMap[0][2] = 4;
        world.terrainMap[0][3] = 6;
        // Step costs: (1+|2-0|) + (1+|4-2|) + (1+|6-4|) = 3 + 3 + 3 = 9. Round-trip = 18.
        check(getRoundTripCost(world, target) == 18, "Elevation impact: mountain path cost verified as 18");

        setAnts(world, antEnergy); // ant energy still 6
        Analysis analysisMountain(world);
        check(approx(analysisMountain.naiveOptimum(antEnergy), 0.0), "Elevation impact: mountain terrain fails with same energy 6");

        // Now provide the true mountain round-trip energy (18)
        antEnergy = {18};
        setAnts(world, antEnergy);
        Analysis analysisMountainSufficient(world);
        check(approx(analysisMountainSufficient.naiveOptimum(antEnergy), 1.0), "Elevation impact: mountain terrain succeeds with energy 18");
    }

    void testGreedyOptimumFlatTerrain() {
        // Controlled flat terrain
        AntWorld world(0, 12, 12, 2);
        world.homeCoordinates = {0, 0};
        world.terrainMap = MapTemplate(12, std::vector<int>(12, 0));

        // Foods at distances 2, 3 -> round trips: 4, 6
        std::vector<Coord> foodPositions = {{0, 2}, {0, 3}};
        putFoodAt(world, foodPositions);

        // Both ants can make round trips
        std::vector<int> antEnergies = {10, 10};
        setAnts(world, antEnergies);
        Analysis analysis(world);
        check(approx(analysis.greedyOptimum(antEnergies), 1.0), "Greedy optimum flat: easy full collection");

        // Ant 0 has 4 (can get food 1), Ant 1 has 5 (cannot get food 2 at RT 6, nor relay)
        antEnergies = {4, 2};
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(approx(analysis.greedyOptimum(antEnergies), 1.0 / 2.0), "Greedy optimum flat: partial collection");

        // Impossible trips
        antEnergies = {1, 1};
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(approx(analysis.greedyOptimum(antEnergies), 0.0), "Greedy optimum flat: impossible trip");
    }

    void testGreedyOptimumRealTerrain() {
        // Real procedural world
        AntWorld world(0, 12, 12, 3);

        std::vector<Coord> candidates = {{2, 2}, {5, 1}, {10, 10}};
        for (auto& c : candidates) {
            if (c == world.homeCoordinates) {
                c = {(c.first + 2) % 12, (c.second + 2) % 12};
            }
        }

        struct FoodCost {
            Coord coord;
            int oneWay;
            int roundTrip;
        };
        std::vector<FoodCost> items;
        for (const auto& c : candidates) {
            int ow = getTravelCost(world, c);
            items.push_back({c, ow, 2 * ow});
        }
        std::sort(items.begin(), items.end(), [](const FoodCost& a, const FoodCost& b) {
            return a.roundTrip < b.roundTrip;
        });

        std::vector<Coord> foodPositions = {items[0].coord, items[1].coord, items[2].coord};
        putFoodAt(world, foodPositions);

        int cost1 = items[0].roundTrip;
        int cost2 = items[1].roundTrip;
        int cost3 = items[2].roundTrip;

        // 1. Easy case: each ant has sufficient energy for its respective food
        std::vector<int> antEnergies = {cost1, cost2, cost3};
        setAnts(world, antEnergies);
        Analysis analysis(world);
        check(approx(analysis.greedyOptimum(antEnergies), 1.0), "Greedy optimum real terrain: all ants satisfy RT costs");

        // 2. Insufficient case: ants can afford cost1 and cost2, but 3rd food is out of reach
        antEnergies = {cost1, cost2, items[2].oneWay - 1};
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(approx(analysis.greedyOptimum(antEnergies), 2.0 / 3.0), "Greedy optimum real terrain: exactly 2 items affordable");

        // 3. Fail case: no ant can even reach distance of closest food
        antEnergies = {items[0].oneWay - 1, items[0].oneWay - 1, items[0].oneWay - 1};
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(approx(analysis.greedyOptimum(antEnergies), 0.0), "Greedy optimum real terrain: no ant can reach food");
    }

    void testGreedyRelayMechanism() {
        // Tests greedyOptimum's ant-relay feature on terrain:
        // When an ant has energy E with dist < E < 2*dist, it drops food at:
        // droppedDist = dist - (E - dist) = 2*dist - E.
        // A second ant with energy >= 2*droppedDist can then retrieve it.
        AntWorld world(0, 12, 12, 2);
        world.homeCoordinates = {0, 0};
        world.terrainMap = MapTemplate(12, std::vector<int>(12, 0));

        // Food at distance 6 (one-way 6, round trip 12)
        putFoodAt(world, {{6, 0}});

        // Ant 1 has energy 8: reaches dist 6, carries food 2 units back, drops at dist 4 (RT 8)
        // Ant 2 has energy 8: reaches dist 4 and brings food home (RT 8 <= 8)
        std::vector<int> antEnergies = {8, 8};
        setAnts(world, antEnergies);
        Analysis analysisSuccess(world);
        check(approx(analysisSuccess.greedyOptimum(antEnergies), 1.0), "Greedy relay: successfully relays food to home");

        // Ant 1 has energy 8 (drops at dist 4, RT 8), but Ant 2 only has energy 4 (cannot afford RT 8 nor reach dist 4)
        antEnergies = {8, 4};
        setAnts(world, antEnergies);
        Analysis analysisFail(world);
        check(approx(analysisFail.greedyOptimum(antEnergies), 0.0), "Greedy relay: second ant lacks energy to finish relay");
    }

    void testNaiveVsGreedyComparison() {
        // Highlights the distinction between naive optimum (energy fungibility)
        // and greedy optimum (individual ant constraints):
        AntWorld world(0, 12, 12, 2);
        world.homeCoordinates = {0, 0};
        world.terrainMap = MapTemplate(12, std::vector<int>(12, 0));

        // Food at distance 5 (round trip 10)
        putFoodAt(world, {{5, 0}});

        // Two ants with energy 5 each (total 10)
        std::vector<int> antEnergies = {5, 5};
        setAnts(world, antEnergies);
        Analysis analysis(world);

        // Naive pools energy: 10 >= 10 -> collects food (1.0)
        check(approx(analysis.naiveOptimum(antEnergies), 1.0), "Comparison: naive pools energy (1.0)");

        // Greedy checks individual ants: neither ant has > 5 energy, so neither can reach and bring it back -> 0.0
        check(approx(analysis.greedyOptimum(antEnergies), 0.0), "Comparison: greedy rejects insufficient individual ants (0.0)");
    }

    void testEfficiency() {
        AntWorld world(0, 5, 5, 2);
        std::vector<int> antEnergies = {10, 10};
        setAnts(world, antEnergies);
        world.score = 2;

        Analysis analysis(world);
        // Efficiency = score / totalEnergy = 2 / 20 = 0.1
        check(approx(analysis.efficiency(), 2.0 / 20.0), "Efficiency calculation (score / totalEnergy)");

        // Score 0 gives efficiency 0 without division by zero
        world.score = 0;
        Analysis analysisZeroScore(world);
        check(approx(analysisZeroScore.efficiency(), 0.0), "Efficiency with score 0 is 0.0");

        // Energy 0 gives efficiency 0
        setAnts(world, {0, 0});
        world.score = 2;
        Analysis analysisZeroEnergy(world);
        check(approx(analysisZeroEnergy.efficiency(), 0.0), "Efficiency with energy 0 is 0.0");
    }

    void testIncompletes() {
        AntWorld world(0, 5, 5, 2);
        putFoodAt(world, {{0, 1}, {1, 0}});
        setAnts(world, {10, 10});

        Analysis analysis(world);
        // At initialization, no dead ant has dropped food at a new location
        check(analysis.incompletes() == 0, "Incompletes: initially zero");

        // Simulate an ant dying and dropping food at a new position
        analysis.world.foodMap[4][4] = 1;
        check(analysis.incompletes() == 1, "Incompletes: 1 dropped food at new location");

        // Second incomplete
        analysis.world.foodMap[3][3] = 1;
        check(analysis.incompletes() == 2, "Incompletes: 2 dropped food items");

        // Consuming an initial food does not count as an incomplete
        analysis.world.foodMap[0][1] = 0;
        check(analysis.incompletes() == 2, "Incompletes: collected food does not affect count");
    }

    void testResults() {
        AntWorld world(0, 5, 5, 2);
        putFoodAt(world, {{0, 0}, {0, 1}, {1, 0}, {1, 1}}); // 4 total food

        world.score = 0;
        Analysis analysis(world);
        check(approx(analysis.results(), 0.0), "Results: score 0 gives 0.0");

        world.score = 2;
        Analysis analysisHalf(world);
        check(approx(analysisHalf.results(), 0.5), "Results: 2/4 food gives 0.5");

        world.score = 4;
        Analysis analysisFull(world);
        check(approx(analysisFull.results(), 1.0), "Results: 4/4 food gives 1.0");

        // Empty food map returns 0.0
        putFoodAt(world, {});
        world.score = 0;
        Analysis analysisEmpty(world);
        check(approx(analysisEmpty.results(), 0.0), "Results: empty food map gives 0.0");
    }
}

int main() {
    testFoodAt();
    testSetAnts();
    testNaiveOptimumFlatTerrain();
    testNaiveOptimumRealTerrain();
    testTerrainElevationImpact();
    testGreedyOptimumFlatTerrain();
    testGreedyOptimumRealTerrain();
    testGreedyRelayMechanism();
    testNaiveVsGreedyComparison();
    testEfficiency();
    testIncompletes();
    testResults();

    std::cout << "\n========================================\n";
    std::cout << "Analysis Test Results: " << checks << " checks, "
              << failures << " failures.\n";
    std::cout << "========================================\n";

    return (failures == 0) ? 0 : 1;
}
