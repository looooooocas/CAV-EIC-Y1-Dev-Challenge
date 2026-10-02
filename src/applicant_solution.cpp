//
// Created by dusan on 9/15/26.
//

#include "../include/antworld.h"
#include "utility_functions.h"
#include <memory>
#include <utility>
#include "../include/signalSearch.h"
#include "../include/simpleSearch.h"
#include <iostream>
#include <string>
#include <vector>

/** @brief this is where you as the applicant will make use of the above functions to develop your solution.
 * here are some existing examples of how calling these functions works to help get you started!
 */
void AntWorld::forage() {

    simpleSearch(*this);

    // Lightweight map display
    int rows = static_cast<int>(foodMap.size());
    int cols = static_cast<int>(foodMap[0].size());

    std::vector<std::string> grid(rows, std::string(cols, ' '));

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (foodMap[r][c] > 0) {
                grid[r][c] = '*';
            }
        }
    }

    for (const auto& ant : ants) {
        if (ant.position.first >= 0 && ant.position.first < rows &&
            ant.position.second >= 0 && ant.position.second < cols) {
            grid[ant.position.first][ant.position.second] = 'a';
        }
    }

    if (!ants.empty()) {
        grid[ants[0].homeCoord.first][ants[0].homeCoord.second] = 'H';
    }

    std::string out = "\033[2J\033[H"; // Clear terminal and move cursor to top-left
    out += '+' + std::string(cols, '-') + "+\n";
    for (int r = 0; r < rows; ++r) {
        out += '|' + grid[r] + "|\n";
    }
    out += '+' + std::string(cols, '-') + "+\n";

    std::cout << out << std::flush;
}

/** You may insert any custom functions below **/


