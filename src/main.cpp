#include <iostream>
#include <iomanip>
#include <numeric>
#include <vector>
#include <string>
#include "../include/antworld.h"
#include "../include/analysis.h"

/** @brief The main function that will run the game. If you are not using a IDE gui, this is the executable you want to target when you build
 */
int main(int argc, char* argv[]) {
    // Pick a deterministic seed (or allow command line override)
    std::random_device rd;
    uint32_t SEED = rd();

    const int MAX_SIMULATION_STEP_COUNT = 1000;
    AntWorld gameInstance = AntWorld(SEED);

    /*
    // Initial state before simulation starts
    std::vector<int> initialEnergies;
    for (const auto& ant : gameInstance.ants) {
        initialEnergies.push_back(ant.energy);
    }
    int totalInitialEnergy = std::accumulate(initialEnergies.begin(), initialEnergies.end(), 0);

    int totalFood = 0;
    for (const auto& row : gameInstance.foodMap) {
        for (int cell : row) {
            totalFood += cell;
        }
    }

    // Run theoretical analyses using the initial world state and terrain
    Analysis analysis(gameInstance);
    double naiveFrac = analysis.naiveOptimum(initialEnergies);
    double greedyFrac = analysis.greedyOptimum(initialEnergies);
    int naiveExpectedFood = static_cast<int>(std::round(naiveFrac * totalFood));
    int greedyExpectedFood = static_cast<int>(std::round(greedyFrac * totalFood));
    */

    // Run simulation
    bool gameOver = false;
    int stepCount = 1;
    while (!gameOver && stepCount <= MAX_SIMULATION_STEP_COUNT) {
        gameOver = gameInstance.worldStep();
        stepCount++;
    }

    if (gameOver) {
        printf("GAME OVER!! Total score: %d\n", gameInstance.score);
    } else if (stepCount >= MAX_SIMULATION_STEP_COUNT) {
        printf("Game not finished. Hit maxmimum simulation step count\n");
    } else {
        printf("Termination reached for unknown reason.\n");
    }

    /*
    // Update analysis world copy with final results for post-game metrics
    analysis.world.score = gameInstance.score;
    analysis.world.foodMap = gameInstance.foodMap;
    int incompletes = analysis.incompletes();
    double actualEfficiency = analysis.efficiency();
    double actualFrac = totalFood > 0 ? static_cast<double>(gameInstance.score) / totalFood : 0.0;

    std::cout << "\n=======================================================\n";
    std::cout << "                 SIGNAL SEARCH BENCHMARK               \n";
    std::cout << "=======================================================\n";
    std::cout << "Seed:                 " << SEED << "\n";
    std::cout << "Map Size:             " << gameInstance.foodMap.size() << " x " << gameInstance.foodMap[0].size() << "\n";
    std::cout << "Home Coordinates:     (" << gameInstance.homeCoordinates.first << ", " << gameInstance.homeCoordinates.second << ")\n";
    std::cout << "Ant Count:            " << initialEnergies.size() << " ants\n";
    std::cout << "Total Initial Energy: " << totalInitialEnergy << " units\n";
    std::cout << "Total Food on Map:    " << totalFood << " items\n";
    std::cout << "Simulation Steps:     " << stepCount << (gameOver ? " (Completed)" : " (Hit max step limit)") << "\n";
    std::cout << "-------------------------------------------------------\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Naive Optimum:        " << naiveExpectedFood << " / " << totalFood
              << " (" << (naiveFrac * 100.0) << "% of map food)\n";
    std::cout << "Greedy Optimum:       " << greedyExpectedFood << " / " << totalFood
              << " (" << (greedyFrac * 100.0) << "% of map food)\n";
    std::cout << "Actual Score:         " << gameInstance.score << " / " << totalFood
              << " (" << (actualFrac * 100.0) << "% of map food)\n";
    std::cout << "-------------------------------------------------------\n";
    std::cout << "COMPARISON & EFFICACY:\n";
    if (greedyExpectedFood > 0) {
        double greedyPerf = (static_cast<double>(gameInstance.score) / greedyExpectedFood) * 100.0;
        std::cout << "  vs Greedy Optimum:  " << greedyPerf << "% efficacy\n";
    }
    if (naiveExpectedFood > 0) {
        double naivePerf = (static_cast<double>(gameInstance.score) / naiveExpectedFood) * 100.0;
        std::cout << "  vs Naive Optimum:   " << naivePerf << "% efficacy\n";
    }
    std::cout << "Incomplete Returns:   " << incompletes << " (food dropped by dead ants)\n";
    std::cout << "Actual Efficiency:    " << std::setprecision(4) << actualEfficiency << " food/energy\n";
    std::cout << "=======================================================\n";
    */

    return 0;
}
