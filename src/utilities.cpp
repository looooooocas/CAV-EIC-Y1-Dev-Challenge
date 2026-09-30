#include "antworld.h"
#include <vector>
#include <cmath>
#include "utilities.h"



// Utilities
Coord operator+(Coord a, Coord b){
    return Coord(a.first + b.first, a.second + b.second);
}

Coord operator-(Coord a, Coord b){
    return Coord(a.first - b.first, a.second - b.second);
}

Coord operator*(Coord a, int scalar){
    return Coord(a.first * scalar, a.second * scalar);
}

struct Territory{
    Coord home;
    double start;
    double end;

    bool within(Coord p){
        return angle(p) < end && angle(p) > start;
    }

    double angle(Coord p){
        return atan2(p.second - home.second, p.first - home.first);
    }

    bool crosses45(Coord a, Coord b){
        if (angle(a) < PI / 4 && angle(b) > PI / 4){

        }
    }

    Coord nextVantage(Coord currentVantage, Coord heading){
        Coord straightShot = currentVantage + heading;
        if (within(straightShot)){
            if ()
        } else {
            // go to nearest edge point
        }
    }

};

int shortestPathLength(Coord start, Coord dest, AntWorld& world){
    return calculatePathCost(world.terrainMap, shortestPath(world.terrainMap, start, dest));
}

bool sortByDistance(Coord a, Coord b, Coord referencePoint, AntWorld& world){
    return shortestPathLength(a, referencePoint, world) < shortestPathLength(b, referencePoint, world);
}

void outputEnergies(std::vector<Ant> ants){
    for (int i = 0; i < ants.size(); i++){
        printf("Ant %d: %d\n", i, ants[i].energy);
    }
}

bool roundTrip(Ant& ant, Coord dest, AntWorld& world){
    if (calculatePathCost(world.terrainMap, shortestPath(world.terrainMap, ant.position, dest)) * 2 <= ant.energy){ // round trip possible
        ant.move(world.terrainMap, dest, world.foodMap);
        ant.returnHome(world.terrainMap, world.foodMap);
        return true;
    }
    // round trip not possible
    return false;
}