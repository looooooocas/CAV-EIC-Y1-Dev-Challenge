#include <../include/analysis.h>
#include <../include/antworld.h>
#include <iostream>

namespace {
    int failures = 0;
    int checks = 0;

    bool approx(double a, double b, double epsilon = 1e-10) {
        return std::abs(a - b) < epsilon;
    }

    void check(bool condition, const std::string &description) {
        ++checks;
            if (!condition) {
                ++failures;
                std::cerr << "FAIL: " << description << '\n';
            }
        }

    void putFoodAt(AntWorld& world, vector<Coord> positions) { //Not taking into account ant origin
        int x = world.foodMap.size();
        int y = world.foodMap[0].size();

        MapTemplate map(x, std::vector<int>(y, 0));
        for (auto pos : positions) {
            map[pos.first][pos.second] = 1;
        }
        world.foodMap = map;
    }

    void testFoodAt(){
        AntWorld world(0, 5, 5, 2);
        vector<Coord> foodPositions = {{0, 0}, {1, 1}, {2, 2}};
        putFoodAt(world, foodPositions);
        vector<Coord> actualFoodPositions;
        
        for (int x = 0; x < world.foodMap.size(); x++) {
            for (int y = 0; y < world.foodMap[x].size(); y++) {
                if (world.foodMap[x][y] == 1) {
                    actualFoodPositions.push_back({x, y});
                }
            }
        }
        check(actualFoodPositions == foodPositions, "Food positions correct");
        check(actualFoodPositions.size() == foodPositions.size(), "Food amount correct");

        foodPositions = {};
        actualFoodPositions.clear();
        putFoodAt(world, foodPositions);

        for (int x = 0; x < world.foodMap.size(); x++) {
            for (int y = 0; y < world.foodMap[x].size(); y++) {
                if (world.foodMap[x][y] == 1) {
                    actualFoodPositions.push_back({x, y});
                }
            }
        }
        check(actualFoodPositions == foodPositions, "Food positions correct");
        check(actualFoodPositions.size() == foodPositions.size(), "Food amount correct");
    }

    void setAnts(AntWorld& world, vector<int> antEnergies) {
        check(antEnergies.size() == world.ants.size(), "Number of ant energies must match number of ants");
        for (int i = 0; i < antEnergies.size(); i++) {
            world.ants[i].energy = antEnergies[i];
        }
    }

    void testSetAnts(){
        AntWorld world(0, 5, 5, 6);
        vector<int> antEnergies = {15, 7, 8, 2 ,0 , -1};
        setAnts(world, antEnergies);
        for (int i = 0; i < antEnergies.size(); i++) {
            check(world.ants[i].energy == antEnergies[i], "Ant energy set correctly");
        }
    }

    void testNaiveOptimum(){ // TODO: actually look at the world / pick a world seed and redo energy levels / outcomes
        // easy case
        AntWorld world(0, 12, 12, 2);
        vector<Coord> foodPositions = {{5, 0}, {0, 5}};
        vector<int> antEnergies = {20, 20};
        putFoodAt(world, foodPositions);
        setAnts(world, antEnergies);
        Analysis analysis(world);
        check(approx(analysis.naiveOptimum(antEnergies), 1.0), "Naive optimum easy");
        // Insufficient energy case
        world = AntWorld(0, 12, 12, 2);
        foodPositions = {{10, 0}, {0, 3}, {0, 2}};
        antEnergies = {13, 13};
        putFoodAt(world, foodPositions);
        setAnts(world, antEnergies); // number of ants gets reduced to 0 each run.
        analysis = Analysis(world);
        check(approx(analysis.naiveOptimum(antEnergies), 2.0/3.0), "Naive optimum insuf energy");
        // Many Options case
        world = AntWorld(0, 12, 12, 2);
        foodPositions = {{5, 0}, {4, 0}, {0, 3}, {0, 2}};
        antEnergies = {13, 13};
        putFoodAt(world, foodPositions);
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(approx(analysis.naiveOptimum(antEnergies), 3.0/4.0), "Naive optimum many options");
        // Fail case
        world = AntWorld(0, 12, 12, 2);
        foodPositions = {{11, 0}};
        antEnergies = {10, 10};
        putFoodAt(world, foodPositions);
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(approx(analysis.naiveOptimum(antEnergies), 0.0), "Naive optimum impossible trip");
    }

    void testGreedyOptimum(){
        // Simple case
        AntWorld world(0, 12, 12, 2);
        vector<Coord> foodPositions = {{5, 0}, {0, 5}};
        vector<int> antEnergies = {13, 13};
        putFoodAt(world, foodPositions);
        setAnts(world, antEnergies);
        Analysis analysis(world);
        check(approx(analysis.greedyOptimum(antEnergies), 1.0), "Greedy optimum easy");
        // Insufficient energy case
        world = AntWorld(0, 12, 12, 2);
        foodPositions = {{6, 0}, {0, 3}, {0, 2}};
        antEnergies = {12, 12};
        putFoodAt(world, foodPositions);
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(approx(analysis.greedyOptimum(antEnergies), 2.0/3.0), "Greedy optimum insuf energy");
        // Many Options case
        world = AntWorld(0, 6, 6, 2);
        foodPositions = {{5, 0}, {4, 0}, {0, 3}, {0, 2}};
        antEnergies = {13, 13};
        putFoodAt(world, foodPositions);
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(approx(analysis.greedyOptimum(antEnergies), 3.0/4.0), "Greedy optimum many options");
        // Fail case
        world = AntWorld(0, 12, 12, 2);
        foodPositions = {{11, 0}};
        antEnergies = {10, 10};
        putFoodAt(world, foodPositions);
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(approx(analysis.greedyOptimum(antEnergies), 0.0), "Greedy optimum impossible trip");
    }

    void testEfficiency(){
        // just testing the math
        AntWorld world(0, 5, 5, 2);
        vector<int> antEnergies = {10, 10};
        world.score = 2;
        Analysis analysis(world);
        check(approx(analysis.efficiency(), 2.0/20.0), "Efficiency");
    }

    void testIncompletes(){
        // 0 case
        AntWorld world(0, 5, 5, 2);
        vector<Coord> foodPositions = {{4, 0}, {0, 4}};
        vector<int> antEnergies = {10, 10};
        putFoodAt(world, foodPositions);
        setAnts(world, antEnergies);
        Analysis analysis(world);
        check(analysis.incompletes() == 0, "Incompletes zero");
        // non-zero case
        foodPositions = {{4, 0}};
        antEnergies = {7};
        putFoodAt(world, foodPositions);
        setAnts(world, antEnergies);
        analysis = Analysis(world);
        check(analysis.incompletes() == 1, "Incompletes non-zero");
    }
}

int main() { // TODO: float comparisons
    testFoodAt();
    testSetAnts();
    testNaiveOptimum();
    testGreedyOptimum();
    testEfficiency();
    testIncompletes();
    return 0;
}
