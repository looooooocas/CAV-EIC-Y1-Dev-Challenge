#include "antworld.h"
#include "utility_functions.h"

bool seesFood(Ant& ant, AntWorld& world){
    return !ant.foodScan(world.foodMap).empty();
}

void wander(Ant& ant, AntWorld& world){
    int xOrY = rand() % 2;
    int positive = rand() % 2;
    int moveAmt = positive ? 1 : -1;
    Coord nextStep = Coord(ant.position.first + (xOrY * moveAmt), ant.position.second+ ((1 - xOrY) * moveAmt));
    ant.move(world.terrainMap, nextStep, world.foodMap);
}

void dumbSearch(Ant& ant, AntWorld& world){
    /*
    hasFood -> go home
    seesFood -> go to it
    wander
    */
    if(ant.carryingFood){
        Coord initPos = ant.position;
        Coord finalPos = ant.returnHome(world.terrainMap, world.foodMap);
        if (finalPos == initPos){ // just to use the last energy, obviously not smart
            wander(ant, world);
        }
    } else if (seesFood(ant, world)){
        Coord initPos = ant.position;
        Coord finalPos = ant.move(world.terrainMap, ant.foodScan(world.foodMap)[0], world.foodMap);
        if (finalPos == initPos){ // just to use the last energy, obviously not smart. It's actually possible to become completely stuck because of height diff.
            wander(ant, world);
        }
    } else {
        wander(ant, world);
    }
    return;
}