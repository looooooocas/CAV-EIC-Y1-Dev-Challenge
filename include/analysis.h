#pragma once
#include "antworld.h"

using namespace std;

class Analysis{
public:
    Analysis(const AntWorld &world);

    double greedyOptimum(std::vector<int> antEnergies, std::vector<int> foodAtDist);

    double naiveOptimum(std::vector<int> antEnergies, std::vector<int> foodAtDist);

    double results();
    
    double efficiency();
    
    int incompletes(); 
    
    // int lostEnergy();

    AntWorld world;

private: 
    std::vector<int> getFoodDists(const AntWorld &world);

    int totalFood;
    std::vector<int> antEnergies; 
    MapTemplate initFoodMap;
};