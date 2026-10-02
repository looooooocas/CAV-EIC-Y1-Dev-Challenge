#include <../include/simpleSearch.h>
#include <../include/antworld.h>
#include <../include/utilities.h>
#include <iostream>
#include <cassert>
#include <vector>

namespace {
    int checks = 0;
    int failures = 0;

    void check(bool condition, const std::string& description) {
        ++checks;
        if (!condition) {
            ++failures;
            std::cerr << "FAIL: " << description << '\n';
        } else {
            std::cout << "  PASS: " << description << '\n';
        }
    }

    // 1. Tests that each ant is assigned and explores its own Territory sector
    void testTerritorySectors() {
        std::cout << "\n--- Test 1: Territory Assignment & Sector Exploration ---\n";
        resetSimpleSearch();

        AntWorld world(0, 15, 15, 4);
        world.homeCoordinates = {7, 7};
        world.terrainMap = MapTemplate(15, std::vector<int>(15, 0));
        world.foodMap = MapTemplate(15, std::vector<int>(15, 0));

        for (int i = 0; i < 4; ++i) {
            world.ants[i].position = {7, 7};
            world.ants[i].homeCoord = {7, 7};
            world.ants[i].energy = 100;
        }

        // Each ant takes a step in simpleSearch
        simpleSearch(world);

        // Verify each ant left home and moved into a distinct sector
        std::vector<Coord> positions;
        for (int i = 0; i < 4; ++i) {
            positions.push_back(world.ants[i].position);
            check(world.ants[i].position != Coord(7, 7), 
                  "Ant " + std::to_string(i) + " moved away from home towards its territory");
        }

        // Verify ants do not all collapse to the same initial coordinate
        bool allDistinct = true;
        for (size_t i = 0; i < positions.size(); ++i) {
            for (size_t j = i + 1; j < positions.size(); ++j) {
                if (positions[i] == positions[j]) allDistinct = false;
            }
        }
        check(allDistinct, "All 4 ants moved toward distinct territory start points");
    }

    // 2. Tests that an ant selects the closest food item when multiple are visible
    void testClosestFoodSelection() {
        std::cout << "\n--- Test 2: Closest Food Selection Priority ---\n";
        resetSimpleSearch();

        AntWorld world(0, 10, 10, 1);
        world.homeCoordinates = {0, 0};
        world.terrainMap = MapTemplate(10, std::vector<int>(10, 0));
        world.foodMap = MapTemplate(10, std::vector<int>(10, 0));
        world.ants[0].position = {0, 0};
        world.ants[0].homeCoord = {0, 0};
        world.ants[0].energy = 100;

        // Place closer food at (0, 1) and further food at (0, 3)
        world.foodMap[0][1] = 1;
        world.foodMap[0][3] = 1;

        simpleSearch(world);

        // Ant should have targeted and grabbed the closer food at (0, 1)
        check(world.foodMap[0][1] == 0, "Closer food at (0, 1) was grabbed");
        check(world.foodMap[0][3] == 1, "Further food at (0, 3) was left for next trip");
        check(world.ants[0].position == Coord(0, 0), "Ant returned home with closer food");
        check(world.ants[0].carryingFood == true, "Ant arrived home carrying the food");
    }

    // 3. Tests strict turn-by-turn timing: grab -> deliver -> score -> wait a turn -> return to stored
    void testStrictTimingAndReturnToStored() {
        std::cout << "\n--- Test 3: Strict Timing (Grab -> Deliver -> Score -> Wait -> Return) ---\n";
        resetSimpleSearch();

        AntWorld world(0, 12, 12, 1);
        world.homeCoordinates = {0, 0};
        world.terrainMap = MapTemplate(12, std::vector<int>(12, 0));
        world.foodMap = MapTemplate(12, std::vector<int>(12, 0));
        world.ants[0].position = {0, 0};
        world.ants[0].homeCoord = {0, 0};
        world.ants[0].energy = 200;

        // Place food at (5, 0). Not visible from home (0,0) with foodRadius 3,
        // but directly along the ant's path toward startPoint (6, 0).
        world.foodMap[5][0] = 1;

        // Step through until food is detected and ant arrives home with food
        int stepsToDeliver = 0;
        bool reachedHomeWithFood = false;
        while (stepsToDeliver < 20 && !world.isGameOver()) {
            simpleSearch(world);
            if (world.ants[0].carryingFood && world.ants[0].position == Coord(0, 0)) {
                reachedHomeWithFood = true;
                break;
            }
            world.updateWorld();
            stepsToDeliver++;
        }
        check(reachedHomeWithFood, "Delivery Step: Ant reached home with food");

        // T: Scoring occurs at end of delivery step via updateWorld()
        world.updateWorld();
        check(world.score == 1, "T: Food scored by updateWorld");
        check(world.ants[0].carryingFood == false, "T: carryingFood reset to false after scoring");
        check(world.ants[0].position == Coord(0, 0), "T: Ant currently at home base");

        // T+1: Ant MUST wait a turn at home
        simpleSearch(world);
        check(world.ants[0].position == Coord(0, 0), "T+1: Ant waited turn at home for scoring cycle");
        world.updateWorld();

        // T+2: Ant departs home, traveling back toward where it left off
        simpleSearch(world);
        check(world.ants[0].position != Coord(0, 0), 
              "T+2: Ant departed home back towards stored position (now at: (" + 
              std::to_string(world.ants[0].position.first) + ", " + 
              std::to_string(world.ants[0].position.second) + "))");
    }

    // 4. Tests consecutive harvesting trips from the same stored location
    void testConsecutiveHarvests() {
        std::cout << "\n--- Test 4: Consecutive Harvest Trips from Same Location ---\n";
        resetSimpleSearch();

        AntWorld world(0, 10, 10, 1);
        world.homeCoordinates = {0, 0};
        world.terrainMap = MapTemplate(10, std::vector<int>(10, 0));
        world.foodMap = MapTemplate(10, std::vector<int>(10, 0));
        world.ants[0].position = {0, 0};
        world.ants[0].homeCoord = {0, 0};
        world.ants[0].energy = 300;

        // Two food items at (0, 2) and (1, 2)
        world.foodMap[0][2] = 1;
        world.foodMap[1][2] = 1;

        // Trip 1: Pick up first food item and deliver home
        simpleSearch(world);
        check(world.ants[0].carryingFood && world.ants[0].position == Coord(0, 0), 
              "Trip 1: Delivered 1st food item to home");
        world.updateWorld();
        check(world.score == 1, "Trip 1: Score incremented to 1");

        // Wait turn at home
        simpleSearch(world);
        check(world.ants[0].position == Coord(0, 0), "Trip 1: Waited turn at home");
        world.updateWorld();

        // Trip 2: Return to stored coordinate and pick up the 2nd food item
        simpleSearch(world);
        check(world.ants[0].carryingFood && world.ants[0].position == Coord(0, 0), 
              "Trip 2: Returned to field, grabbed 2nd food, and delivered home");
        world.updateWorld();
        check(world.score == 2, "Trip 2: Score incremented to 2");
    }

    // 5. Tests safe navigation over steep terrain (elevation differences)
    void testSteepTerrainSafety() {
        std::cout << "\n--- Test 5: Steep Terrain Safety & Elevation Handling ---\n";
        resetSimpleSearch();

        AntWorld world(0, 6, 6, 1);
        world.homeCoordinates = {0, 0};
        world.terrainMap = MapTemplate(6, std::vector<int>(6, 0));
        world.foodMap = MapTemplate(6, std::vector<int>(6, 0));
        world.ants[0].position = {0, 0};
        world.ants[0].homeCoord = {0, 0};
        world.ants[0].energy = 80;

        // Create a steep ridge between home and food
        world.terrainMap[0][1] = 3;
        world.terrainMap[0][2] = 6;
        world.foodMap[0][2] = 1;

        simpleSearch(world);
        check(world.ants[0].position == Coord(0, 0), "Ant crossed ridge and returned safely to home");
        check(world.ants[0].energy > 0, "Ant maintained positive energy throughout the mountain trip");
        world.updateWorld();
        check(world.score == 1, "Food retrieved over steep ridge successfully scored");
    }

    // 6. Multi-Ant Full Simulation Benchmark across multiple seeds
    void testFullSimulationBenchmark() {
        std::cout << "\n--- Test 6: Full Multi-Ant Simulation Benchmark ---\n";
        std::vector<uint32_t> testSeeds = {42, 12345, 0};

        for (uint32_t seed : testSeeds) {
            resetSimpleSearch();
            AntWorld world(seed, 15, 15, 8);
            int initialFood = 0;
            for (const auto& row : world.foodMap) {
                for (int cell : row) initialFood += cell;
            }

            int steps = 0;
            while (!world.isGameOver() && steps < 200) {
                simpleSearch(world);
                world.updateWorld();
                steps++;
            }

            check(world.score >= 20, 
                  "Seed " + std::to_string(seed) + ": Scored " + 
                  std::to_string(world.score) + " / " + std::to_string(initialFood) + 
                  " in " + std::to_string(steps) + " steps");

            // Verify no ants dropped food (no incomplete returns)
            int droppedFood = 0;
            for (const auto& ant : world.ants) {
                if (ant.carryingFood && ant.position != world.homeCoordinates) {
                    droppedFood++;
                }
            }
            check(droppedFood == 0, "Seed " + std::to_string(seed) + ": 0 food lost in transit");
        }
    }
}

int main() {
    std::cout << "========================================\n";
    std::cout << "        RUNNING SIMPLE SEARCH TESTS     \n";
    std::cout << "========================================\n";

    testTerritorySectors();
    testClosestFoodSelection();
    testStrictTimingAndReturnToStored();
    testConsecutiveHarvests();
    testSteepTerrainSafety();
    testFullSimulationBenchmark();

    tests();
    check(testSimpleSearchTransitions(), "SimpleAnt State Machine Transitions");

    std::cout << "\n========================================\n";
    std::cout << "Simple Search Test Results: " << checks << " checks, "
              << failures << " failures.\n";
    std::cout << "========================================\n";

    return (failures == 0) ? 0 : 1;
}
