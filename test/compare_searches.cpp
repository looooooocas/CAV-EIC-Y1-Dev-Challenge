#include <iostream>
#include <iomanip>
#include <vector>
#include <numeric>
#include <cmath>
#include <string>
#include "../include/antworld.h"
#include "../include/analysis.h"
#include "../include/simpleSearch.h"
#include "../include/signalSearch.h"

struct ExperimentResult {
    int param; // ant count OR map dimension
    int totalFood;
    int naiveFood;
    int greedyFood;
    int simpleScore;
    int signalScore;
    int numScouts;
};

struct SeedExperiments {
    std::string label;
    uint32_t seed;
    std::vector<ExperimentResult> antScaling;
    std::vector<ExperimentResult> sizeScaling;
};

int main() {
    // 3 Curated Seeds:
    // 1. Closest Match (where Signal was most competitive)
    // 2. Biggest Blowout (where Simple had the largest lead)
    // 3. High-Energy Terrain (robust terrain test)
    std::vector<std::pair<std::string, uint32_t>> chosenSeeds = {
        {"Closest Match",    638747925},
        {"Biggest Blowout", 3882826974},
        {"High-Energy Map", 4219272983}
    };

    const std::vector<int> antCounts = {8, 10, 12, 14, 16};
    const std::vector<int> mapSizes  = {15, 18, 21, 24, 27, 30};

    std::vector<SeedExperiments> allResults;

    for (const auto& [label, seed] : chosenSeeds) {
        SeedExperiments se;
        se.label = label;
        se.seed = seed;

        // -------------------------------------------------------------
        // Experiment 1: Ant Scaling (8 to 16 ants, fixed 15x15 map)
        // -------------------------------------------------------------
        for (int ants : antCounts) {
            // Sweet spot: 25% scouts
            int scouts = std::max(1, static_cast<int>(std::round(ants * 0.25)));

            // Baseline & Analysis
            AntWorld baseWorld(seed, 15, 15, ants);
            int totalFood = 0;
            for (const auto& row : baseWorld.foodMap) {
                for (int cell : row) totalFood += cell;
            }
            std::vector<int> energies;
            for (const auto& a : baseWorld.ants) energies.push_back(a.energy);

            Analysis analysis(baseWorld);
            int naiveFood  = static_cast<int>(std::round(analysis.naiveOptimum(energies) * totalFood));
            int greedyFood = static_cast<int>(std::round(analysis.greedyOptimum(energies) * totalFood));

            // Simple Search
            AntWorld worldSimple(seed, 15, 15, ants);
            resetSimpleSearch();
            int stepsSimple = 0;
            while (!worldSimple.isGameOver() && stepsSimple < 500) {
                simpleSearch(worldSimple);
                worldSimple.updateWorld();
                stepsSimple++;
            }

            // Signal Search
            AntWorld worldSignal(seed, 15, 15, ants);
            resetSignalSearch();
            int stepsSignal = 0;
            while (!worldSignal.isGameOver() && stepsSignal < 500) {
                signallingAnts(worldSignal, scouts);
                worldSignal.updateWorld();
                stepsSignal++;
            }

            se.antScaling.push_back({
                ants,
                totalFood,
                naiveFood,
                greedyFood,
                worldSimple.score,
                worldSignal.score,
                scouts
            });
        }

        // -------------------------------------------------------------
        // Experiment 2: Map Size Scaling (15x15 to 30x30, fixed 8 ants)
        // -------------------------------------------------------------
        for (int size : mapSizes) {
            int ants = 8;
            int scouts = 2; // 25% sweet spot for 8 ants

            AntWorld baseWorld(seed, size, size, ants);
            int totalFood = 0;
            for (const auto& row : baseWorld.foodMap) {
                for (int cell : row) totalFood += cell;
            }
            std::vector<int> energies;
            for (const auto& a : baseWorld.ants) energies.push_back(a.energy);

            Analysis analysis(baseWorld);
            int naiveFood  = static_cast<int>(std::round(analysis.naiveOptimum(energies) * totalFood));
            int greedyFood = static_cast<int>(std::round(analysis.greedyOptimum(energies) * totalFood));

            // Simple Search
            AntWorld worldSimple(seed, size, size, ants);
            resetSimpleSearch();
            int stepsSimple = 0;
            while (!worldSimple.isGameOver() && stepsSimple < 500) {
                simpleSearch(worldSimple);
                worldSimple.updateWorld();
                stepsSimple++;
            }

            // Signal Search
            AntWorld worldSignal(seed, size, size, ants);
            resetSignalSearch();
            int stepsSignal = 0;
            while (!worldSignal.isGameOver() && stepsSignal < 500) {
                signallingAnts(worldSignal, scouts);
                worldSignal.updateWorld();
                stepsSignal++;
            }

            se.sizeScaling.push_back({
                size,
                totalFood,
                naiveFood,
                greedyFood,
                worldSimple.score,
                worldSignal.score,
                scouts
            });
        }

        allResults.push_back(se);
    }

    // =========================================================================
    // PRINT EXPERIMENT 1: ANT SCALING
    // =========================================================================
    std::cout << "\n";
    std::cout << "==================================================================================================================================\n";
    std::cout << "                        PART 1: SCALING ANT COUNT (8 TO 16 ANTS ON FIXED 15x15 MAP, ~25% SCOUTS)                  \n";
    std::cout << "==================================================================================================================================\n";
    std::cout << std::left
              << std::setw(18) << "Seed Type"
              << std::setw(12) << "Seed"
              << std::setw(6)  << "Ants"
              << std::setw(10) << "Config"
              << std::setw(8)  << "Food"
              << std::setw(8)  << "Naive"
              << std::setw(8)  << "Greedy"
              << std::setw(16) << "Simple Search"
              << std::setw(16) << "Signal Search"
              << std::setw(14) << "Winner"
              << "\n";
    std::cout << "----------------------------------------------------------------------------------------------------------------------------------\n";

    for (const auto& se : allResults) {
        for (const auto& r : se.antScaling) {
            double simplePct = r.greedyFood > 0 ? (100.0 * r.simpleScore / r.greedyFood) : 0.0;
            double signalPct = r.greedyFood > 0 ? (100.0 * r.signalScore / r.greedyFood) : 0.0;

            std::string configStr = std::to_string(r.numScouts) + "S / " + std::to_string(r.param - r.numScouts) + "C";
            std::string simpleStr = std::to_string(r.simpleScore) + " (" + std::to_string(static_cast<int>(std::round(simplePct))) + "%)";
            std::string signalStr = std::to_string(r.signalScore) + " (" + std::to_string(static_cast<int>(std::round(signalPct))) + "%)";

            std::string winner;
            if (r.simpleScore > r.signalScore) {
                winner = "Simple (+" + std::to_string(r.simpleScore - r.signalScore) + ")";
            } else if (r.signalScore > r.simpleScore) {
                winner = "Signal (+" + std::to_string(r.signalScore - r.simpleScore) + ")";
            } else {
                winner = "Tie";
            }

            std::cout << std::left
                      << std::setw(18) << se.label
                      << std::setw(12) << se.seed
                      << std::setw(6)  << r.param
                      << std::setw(10) << configStr
                      << std::setw(8)  << r.totalFood
                      << std::setw(8)  << r.naiveFood
                      << std::setw(8)  << r.greedyFood
                      << std::setw(16) << simpleStr
                      << std::setw(16) << signalStr
                      << std::setw(14) << winner
                      << "\n";
        }
        std::cout << "----------------------------------------------------------------------------------------------------------------------------------\n";
    }

    // Average per ant count across the 3 seeds
    std::cout << "AVERAGE BY ANT COUNT (Across 3 Seeds):\n";
    for (size_t a = 0; a < antCounts.size(); ++a) {
        double avgNaive = 0, avgGreedy = 0, avgSimple = 0, avgSignal = 0;
        int scouts = 0;
        for (const auto& se : allResults) {
            avgNaive += se.antScaling[a].naiveFood;
            avgGreedy += se.antScaling[a].greedyFood;
            avgSimple += se.antScaling[a].simpleScore;
            avgSignal += se.antScaling[a].signalScore;
            scouts = se.antScaling[a].numScouts;
        }
        avgNaive /= allResults.size();
        avgGreedy /= allResults.size();
        avgSimple /= allResults.size();
        avgSignal /= allResults.size();

        std::cout << "  Ants: " << std::setw(2) << antCounts[a] 
                  << " (" << scouts << "S/" << (antCounts[a] - scouts) << "C) | "
                  << "Naive: " << std::fixed << std::setprecision(1) << avgNaive << " | "
                  << "Greedy: " << std::fixed << std::setprecision(1) << avgGreedy << " | "
                  << "Simple: " << std::setw(4) << avgSimple << " (" << (avgSimple / avgGreedy * 100.0) << "% Gr, " << (avgSimple / avgNaive * 100.0) << "% Nv) | "
                  << "Signal: " << std::setw(4) << avgSignal << " (" << (avgSignal / avgGreedy * 100.0) << "% Gr, " << (avgSignal / avgNaive * 100.0) << "% Nv) | "
                  << "Simple Lead: +" << (avgSimple - avgSignal) << "\n";
    }

    // =========================================================================
    // PRINT EXPERIMENT 2: MAP SIZE SCALING
    // =========================================================================
    std::cout << "\n";
    std::cout << "==================================================================================================================================\n";
    std::cout << "                        PART 2: SCALING MAP SIZE (15x15 TO 30x30 ON FIXED 8 ANTS, 2S/6C)                          \n";
    std::cout << "==================================================================================================================================\n";
    std::cout << std::left
              << std::setw(18) << "Seed Type"
              << std::setw(12) << "Seed"
              << std::setw(10) << "Map Size"
              << std::setw(8)  << "Food"
              << std::setw(8)  << "Naive"
              << std::setw(8)  << "Greedy"
              << std::setw(16) << "Simple Search"
              << std::setw(16) << "Signal Search"
              << std::setw(14) << "Winner"
              << "\n";
    std::cout << "----------------------------------------------------------------------------------------------------------------------------------\n";

    for (const auto& se : allResults) {
        for (const auto& r : se.sizeScaling) {
            double simplePct = r.greedyFood > 0 ? (100.0 * r.simpleScore / r.greedyFood) : 0.0;
            double signalPct = r.greedyFood > 0 ? (100.0 * r.signalScore / r.greedyFood) : 0.0;

            std::string sizeStr = std::to_string(r.param) + "x" + std::to_string(r.param);
            std::string simpleStr = std::to_string(r.simpleScore) + " (" + std::to_string(static_cast<int>(std::round(simplePct))) + "%)";
            std::string signalStr = std::to_string(r.signalScore) + " (" + std::to_string(static_cast<int>(std::round(signalPct))) + "%)";

            std::string winner;
            if (r.simpleScore > r.signalScore) {
                winner = "Simple (+" + std::to_string(r.simpleScore - r.signalScore) + ")";
            } else if (r.signalScore > r.simpleScore) {
                winner = "Signal (+" + std::to_string(r.signalScore - r.simpleScore) + ")";
            } else {
                winner = "Tie";
            }

            std::cout << std::left
                      << std::setw(18) << se.label
                      << std::setw(12) << se.seed
                      << std::setw(10) << sizeStr
                      << std::setw(8)  << r.totalFood
                      << std::setw(8)  << r.naiveFood
                      << std::setw(8)  << r.greedyFood
                      << std::setw(16) << simpleStr
                      << std::setw(16) << signalStr
                      << std::setw(14) << winner
                      << "\n";
        }
        std::cout << "----------------------------------------------------------------------------------------------------------------------------------\n";
    }

    // Average per map size across the 3 seeds
    std::cout << "AVERAGE BY MAP SIZE (Across 3 Seeds):\n";
    for (size_t s = 0; s < mapSizes.size(); ++s) {
        double avgNaive = 0, avgGreedy = 0, avgSimple = 0, avgSignal = 0;
        int food = 0;
        for (const auto& se : allResults) {
            avgNaive += se.sizeScaling[s].naiveFood;
            avgGreedy += se.sizeScaling[s].greedyFood;
            avgSimple += se.sizeScaling[s].simpleScore;
            avgSignal += se.sizeScaling[s].signalScore;
            food = se.sizeScaling[s].totalFood;
        }
        avgNaive /= allResults.size();
        avgGreedy /= allResults.size();
        avgSimple /= allResults.size();
        avgSignal /= allResults.size();

        std::string sizeStr = std::to_string(mapSizes[s]) + "x" + std::to_string(mapSizes[s]);
        std::cout << "  Size: " << std::setw(5) << sizeStr 
                  << " (" << std::setw(3) << food << " food) | "
                  << "Naive: " << std::fixed << std::setprecision(1) << std::setw(5) << avgNaive << " | "
                  << "Greedy: " << std::fixed << std::setprecision(1) << std::setw(5) << avgGreedy << " | "
                  << "Simple: " << std::setw(4) << avgSimple << " (" << (avgSimple / avgGreedy * 100.0) << "% Gr, " << (avgSimple / avgNaive * 100.0) << "% Nv) | "
                  << "Signal: " << std::setw(4) << avgSignal << " (" << (avgSignal / avgGreedy * 100.0) << "% Gr, " << (avgSignal / avgNaive * 100.0) << "% Nv) | "
                  << "Simple Lead: +" << (avgSimple - avgSignal) << "\n";
    }
    std::cout << "==================================================================================================================================\n\n";

    // -------------------------------------------------------------
    // PART 3: 10 Benchmark Seeds Head-to-Head Comparison (15x15, 8 ants)
    // -------------------------------------------------------------
    std::cout << "==================================================================================================================================\n";
    std::cout << "                 PART 3: HEAD-TO-HEAD BENCHMARK ACROSS 10 SEEDS (15x15, 8 ANTS, 2S/6C SIGNAL)                     \n";
    std::cout << "==================================================================================================================================\n";
    std::cout << std::left
              << std::setw(4)  << "#"
              << std::setw(12) << "Seed"
              << std::setw(18) << "Type"
              << std::setw(8)  << "Food"
              << std::setw(8)  << "Naive"
              << std::setw(8)  << "Greedy"
              << std::setw(18) << "Simple Search"
              << std::setw(18) << "Signal Search"
              << std::setw(14) << "Winner"
              << "\n";
    std::cout << "----------------------------------------------------------------------------------------------------------------------------------\n";

    std::vector<std::pair<std::string, uint32_t>> tenSeeds = {
        {"Other Seed #1",   3205851784},
        {"Other Seed #2",   2467874039},
        {"Other Seed #3",   1342359732},
        {"Other Seed #4",   4155908089},
        {"Other Seed #5",   2866877852},
        {"Closest Match",   638747925},
        {"Biggest Blowout", 3882826974},
        {"High-Energy Map", 4219272983},
        {"Seed #9",         1854518068},
        {"Seed #10",        1790332065}
    };

    struct Row10 {
        int food;
        int naive;
        int greedy;
        int simpleScore;
        int signalScore;
    };
    std::vector<Row10> rows;

    for (size_t i = 0; i < tenSeeds.size(); ++i) {
        uint32_t seed = tenSeeds[i].second;
        std::string label = tenSeeds[i].first;

        // Baseline Analysis
        AntWorld baseWorld(seed, 15, 15, 8);
        int totalFood = 0;
        for (const auto& r : baseWorld.foodMap) for (int c : r) totalFood += c;
        std::vector<int> energies;
        for (const auto& a : baseWorld.ants) energies.push_back(a.energy);
        Analysis analysis(baseWorld);
        int naiveFood  = static_cast<int>(std::round(analysis.naiveOptimum(energies) * totalFood));
        int greedyFood = static_cast<int>(std::round(analysis.greedyOptimum(energies) * totalFood));

        // Simple Search
        AntWorld wSim(seed, 15, 15, 8);
        resetSimpleSearch();
        int stepsSim = 0;
        while (!wSim.isGameOver() && stepsSim < 500) {
            simpleSearch(wSim);
            wSim.updateWorld();
            stepsSim++;
        }
        int scoreSimple = wSim.score;

        // Signal (2 scouts)
        AntWorld wSig(seed, 15, 15, 8);
        resetSignalSearch();
        int stepsSig = 0;
        while (!wSig.isGameOver() && stepsSig < 500) {
            signallingAnts(wSig, 2);
            wSig.updateWorld();
            stepsSig++;
        }
        int scoreSig = wSig.score;

        rows.push_back({totalFood, naiveFood, greedyFood, scoreSimple, scoreSig});

        std::string simpleStr = std::to_string(scoreSimple) + " (" + 
                                std::to_string(static_cast<int>(std::round(scoreSimple * 100.0 / greedyFood))) + "%)";
        std::string signalStr = std::to_string(scoreSig) + " (" + 
                                std::to_string(static_cast<int>(std::round(scoreSig * 100.0 / greedyFood))) + "%)";
        std::string winner = (scoreSimple > scoreSig) ? "Simple (+" + std::to_string(scoreSimple - scoreSig) + ")" :
                             (scoreSig > scoreSimple) ? "Signal (+" + std::to_string(scoreSig - scoreSimple) + ")" : "Tie";

        std::cout << std::left
                  << std::setw(4)  << (i + 1)
                  << std::setw(12) << seed
                  << std::setw(18) << label
                  << std::setw(8)  << totalFood
                  << std::setw(8)  << naiveFood
                  << std::setw(8)  << greedyFood
                  << std::setw(18) << simpleStr
                  << std::setw(18) << signalStr
                  << std::setw(14) << winner
                  << "\n";
    }

    std::cout << "----------------------------------------------------------------------------------------------------------------------------------\n";
    // Average for first 5 ("other 5")
    double n5 = 0, g5 = 0, sim5 = 0, sig5 = 0;
    for (int i = 0; i < 5; ++i) {
        n5 += rows[i].naive; g5 += rows[i].greedy; sim5 += rows[i].simpleScore; sig5 += rows[i].signalScore;
    }
    std::cout << std::left
              << std::setw(34) << "AVERAGE OTHER 5 SEEDS (#1-#5):"
              << std::setw(8)  << "90"
              << std::fixed << std::setprecision(1)
              << std::setw(8)  << (n5 / 5.0)
              << std::setw(8)  << (g5 / 5.0)
              << std::setw(18) << (sim5 / 5.0)
              << std::setw(18) << (sig5 / 5.0)
              << std::setw(14) << ("Simple (+" + std::to_string(static_cast<int>(std::round((sim5 - sig5) / 5.0))) + ")")
              << "\n";

    // Average for all 10
    double n10 = 0, g10 = 0, sim10 = 0, sig10 = 0;
    for (size_t i = 0; i < 10; ++i) {
        n10 += rows[i].naive; g10 += rows[i].greedy; sim10 += rows[i].simpleScore; sig10 += rows[i].signalScore;
    }
    std::cout << std::left
              << std::setw(34) << "AVERAGE ALL 10 SEEDS:"
              << std::setw(8)  << "90"
              << std::fixed << std::setprecision(1)
              << std::setw(8)  << (n10 / 10.0)
              << std::setw(8)  << (g10 / 10.0)
              << std::setw(18) << (sim10 / 10.0)
              << std::setw(18) << (sig10 / 10.0)
              << std::setw(14) << ("Simple (+" + std::to_string(static_cast<int>(std::round((sim10 - sig10) / 10.0))) + ")")
              << "\n";
    std::cout << "==================================================================================================================================\n\n";

    return 0;
}
