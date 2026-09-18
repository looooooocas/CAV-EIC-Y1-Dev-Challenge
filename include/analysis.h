#pragma once
#include "antworld.h"

using namespace std;

class Analysis{
public:
    Analysis(const AntWorld &world);

    double optimal();

    double results();
    
    double efficiency();
    
    int incompletes(); 
    
    //subOptimals(); investigates how frequently we wasted energy e.g. could have take a more direct path if we had waited for a scout, TBD

    AntWorld world;

private: 
    std::vector<int> getFoodDists(const AntWorld &world);

    double greedyOptimum(std::vector<int> antEnergies, std::vector<int> foodAtDist);

    double perfectOptimum(std::vector<int> antEnergies, std::vector<int> foodAtDist);

    int totalFood;
    std::vector<int> antEnergies; 
    MapTemplate initFoodMap;
};