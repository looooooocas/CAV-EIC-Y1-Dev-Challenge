#include "antworld.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <algorithm>
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

int shortestPathCost(Coord start, Coord dest, AntWorld& world){
    return calculatePathCost(world.terrainMap, shortestPath(world.terrainMap, start, dest));
}

bool sortByDistance(Coord a, Coord b, Coord referencePoint, AntWorld& world){
    return shortestPathCost(a, referencePoint, world) < shortestPathCost(b, referencePoint, world);
}

void outputEnergies(std::vector<Ant> ants){ //TODO: rm
    for (int i = 0; i < ants.size(); i++){
        printf("Ant %d: %d\n", i, ants[i].energy);
    }
}

bool roundTrip(Ant& ant, Coord dest, AntWorld& world){
    int roundTripCost = calculatePathCost(world.terrainMap, shortestPath(world.terrainMap, ant.position, dest)) * 2;
    if (roundTripCost < ant.energy){ // round trip possible with spare energy
        safeMove(ant, dest, world);
        safeMove(ant, ant.homeCoord, world); 
        return true;
    }
    // round trip not possible
    return false;
}

// Territory methods:

bool Territory::within(Coord p) const {
    return angle(p) < end && angle(p) > start;
}

double Territory::angle(Coord p) const {
    return atan2(p.second - home.second, p.first - home.first);
}

bool Territory::crosses45(Coord a, Coord b) const {
    double angleA = std::atan2(std::abs(a.second - home.second), std::abs(a.first - home.first));
    double angleB = std::atan2(std::abs(b.second - home.second), std::abs(b.first - home.first));
    return (angleA < PI / 4 && angleB > PI / 4) || (angleA > PI / 4 && angleB < PI / 4);
}

Coord safeMove(Ant& ant, Coord dest, AntWorld& world){
    // Ant determines reachability: reject out-of-bounds destinations before
    // touching shortestPath (move() only guards >= size, not negatives).
    int rows = (int)world.terrainMap.size();
    int cols = (int)world.terrainMap[0].size();
    if (dest.first < 0 || dest.first >= rows || dest.second < 0 || dest.second >= cols){
        return ant.position;
    }
    // move without letting energy = 0; energy >= 1 always
    std::vector<Coord> path = shortestPath(world.terrainMap, ant.position, dest);
    while(!path.empty()){
        Coord end = path.back();
        int pathCost = shortestPathCost(ant.position, end, world);
        if (pathCost < ant.energy){ // if we can make it with >= 1 energy left
            ant.move(world.terrainMap, end, world.foodMap);
            return ant.position;
        } else { // shorten the path to dest (we cant make it)
            path.pop_back();
        }
    }
    return ant.position;
}

/*
Preconditions: Heading only has 1 nonzero component, and its value is foodRadius
*/
Coord Territory::nextVantage(Coord currentVantage, Coord &heading) {
    int rows = (int)map->size();
    int cols = (int)(*map)[0].size();

    Coord straightShot = currentVantage + heading;
    bool straightShotInBounds = straightShot.first >= 0 && straightShot.first < rows &&
                                 straightShot.second >= 0 && straightShot.second < cols;

    if (straightShotInBounds && within(straightShot)) {
        // Normal step within territory and map bounds
        if (crosses45(currentVantage, straightShot)){
            Coord curve = currentVantage + Coord(-heading.second, -heading.first);
            return curve;
        } else {
            return straightShot;
        }
    } else if (!straightShotInBounds) {
        // Step perpendicular to heading along the map edge
        Coord slideStep(-heading.second, -heading.first);
        Coord slide = currentVantage + slideStep;

        bool slideInBounds = slide.first >= 0 && slide.first < rows &&
                             slide.second >= 0 && slide.second < cols;

        // Hug the wall if the slide position is valid and inside territory
        if (slideInBounds && within(slide)) {
            return slide;
        }

        // Corner / edge intersection: cannot continue along this wall.
        // Step to the next layer and reverse scanning direction.
        Coord jumpUp = currentVantage + slideStep;
        
        // Clamp fallback coordinates strictly within the grid
        jumpUp.first  = std::max(0, std::min(rows - 1, jumpUp.first));
        jumpUp.second = std::max(0, std::min(cols - 1, jumpUp.second));
        
        heading = heading * -1;
        return jumpUp;
    } else {
        // Hit the territory's angular boundary — jump to the next layer outward
        // and reverse heading to scan back across.
        Coord jumpUp = currentVantage + Coord(-heading.second, -heading.first) * 2;
        heading = heading * -1;
        return jumpUp;
    }
}



Coord Territory::startPoint(int foodRadius) {
    int rows = static_cast<int>(map->size());
    int cols = static_cast<int>((*map)[0].size());

    // 2*foodRadius from home, in direction opposite of start heading
    int offsetX = static_cast<int>(std::lround(2 * foodRadius * std::cos(start)));
    int offsetY = static_cast<int>(std::lround(2 * foodRadius * std::sin(start)));
    Coord startPoint = home + Coord(offsetX, offsetY);

    startPoint.first  = std::max(0, std::min(rows - 1, startPoint.first));
    startPoint.second = std::max(0, std::min(cols - 1, startPoint.second));

    return startPoint;
}

Coord Territory::startHeading(int foodRadius) { //TODO: should foodRadius be saved by the struct?
    // generate x and y headings and determine which points from start to end
    Coord heading = Coord(foodRadius, 0);
    for (int i = 0; i < 4; i++){
        Coord testPoint = startPoint(foodRadius) + heading;
        if (within(testPoint)){ // TODO: is within sufficient? (it should since within is strictly > start), if not look for a shrinking angle
            return heading;
        }
        heading = Coord(-heading.second, -heading.first);
    }
    return Coord(0,0);
}
