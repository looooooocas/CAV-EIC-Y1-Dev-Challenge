//
// Created by dusan on 9/15/26.
//

#include "../include/antworld.h"


bool onFood(Ant ant, std::vector<Coord> visibleFood){
    for(Coord c : visibleFood){
        if(c == ant.position){
            return true;
        }
    }
    return false;
}

bool seesFood(Ant ant, AntWorld world){
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

void outputEnergies(std::vector<Ant> ants){
    for (int i = 0; i < ants.size(); i++){
        printf("Ant %d: %d\n", i, ants[i].energy);
    }
}

/** @brief this is where you as the applicant will make use of the above functions to develop your solution.
 * here are some existing examples of how calling these functions works to help get you started!
 */
void AntWorld::forage() {

    for(Ant& ant : this->ants){
        outputEnergies(this->ants);
        dumbSearch(ant, *this);
    }

    // std::vector<Coord> visibleFood = this->ants[0].foodScan(this->foodMap);
    //
    // Coord desiredDestination = Coord(5, 5);
    // Coord finalPos = this->ants[0].move(this->terrainMap, desiredDestination, this->foodMap);
    // bool destCheck = (desiredDestination == finalPos);
    //
    // this->ants[0].dropPheromone(this->pheromoneMap);
    //
    // this->ants[0].erasePheromone(this->pheromoneMap);
    //
    // this->ants[0].returnHome(this->terrainMap, this->foodMap);
}

/** You may insert any custom functions below **/





