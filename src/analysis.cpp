#include <../include/analysis.h>

Analysis::Analysis(const AntWorld &world) : world(world){
    this->initFoodMap = world.foodMap;
    this->totalFood = 0;
    // count total food
    for (auto row : world.foodMap) 
        for (int cell : row)
            this->totalFood += cell;
    // store ant energies
    for (auto ant : world.ants){
        this->antEnergies.push_back(ant.energy);
    }
    std::sort(this->antEnergies.begin(), this->antEnergies.end());
}

double Analysis::results(){
    return static_cast<double>(this->world.score) / this->totalFood;
}

std::vector<int> Analysis::getFoodDists(const AntWorld &world){
    const int maxDist = (world.terrainMap.size() + world.terrainMap[0].size()) * 2;
    std::vector<int> foodAtDist(maxDist, 0);

    // count travel energy of each food source (foodAtDist)
    for (unsigned long row = 0; row < world.foodMap.size(); row++){
        for (unsigned long col = 0; col < world.foodMap[row].size(); col++){
            if(world.foodMap[row][col] == 1){
                std::vector<Coord> path = shortestPath(world.terrainMap, world.homeCoordinates, Coord(row, col));
                int travelEnergy = 0;
                for (int i = 1; i < path.size(); ++i){
                    auto [r1, c1] = path[i - 1];
                    auto [r2, c2] = path[i];

                    int cost = 1 + std::abs(world.terrainMap[r1][c1] - world.terrainMap[r2][c2]);
                    travelEnergy += cost;
                }
                foodAtDist[travelEnergy]++;
            }
        }
    }
    return foodAtDist;
}

double Analysis::greedyOptimum(std::vector<int> antEnergies, std::vector<int> foodAtDist){
    /*
    count sort round trip distances to each food source
    iterate through that list and subtract distances while incrementing food collected until we run out of energy
    divide by total food on the map
    The food selection is optimal but the ant assignment may be improvable
    */
    int optimal = 0;
    for (int dist = 0; dist < foodAtDist.size(); dist++ ){
        while(foodAtDist[dist] > 0){
            // find first suitable ant
            for (int a = 0; a < antEnergies.size(); a++){
                if (antEnergies[a] >= dist*2){
                    antEnergies[a] -= dist*2;
                    optimal++;
                    foodAtDist[dist]--;
                    break;
                }
            } // only exits if no single ant can make the trip
            if (!antEnergies.empty()){
                foodAtDist[dist] -= 1;
                foodAtDist[dist - antEnergies.back()] += 1;
                antEnergies.pop_back();
            }
            std::sort(antEnergies.begin(), antEnergies.end()); // re-sort for next iteration
        }
    }
    return (double)optimal / totalFood;
}

double Analysis::naiveOptimum(std::vector<int> antEnergies, std::vector<int> foodAtDist){ 
    // assumes no lost energy due to mismatches ie 10 ants with 1 energy are assumed to be able to get a food at dist 5 with round trip cost 10
    int energyTotal = std::accumulate(antEnergies.begin(), antEnergies.end(), 0);
    int foodPotential = 0;
    for (int dist = 0; dist < foodAtDist.size(); dist++){
        while (foodAtDist[dist] > 0){
            if (energyTotal >= dist*2){
                energyTotal -= dist*2;
                foodPotential++;
                foodAtDist[dist]--;
            } else {
                break;
            }
        }
    }
    return (double)foodPotential / totalFood;
}

double Analysis::efficiency(){ //
    int energyTotal = std::accumulate(antEnergies.begin(), antEnergies.end(), 0);
    return (double)energyTotal / world.score;
}

int Analysis::incompletes(){
    int count = 0;
    for (int i = 0; i < world.foodMap.size(); i++){
        for (int j = 0; j < world.foodMap[0].size(); j++){
            if (world.foodMap[i][j] == 1 && initFoodMap[i][j] == 0){ // food at a new location (dropped by a dead ant) counts as an incomplete return
                count++;
            }
        }
    }
    return count;
}



//Tests --------------------------------------



