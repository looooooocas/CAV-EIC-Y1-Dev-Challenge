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
    int rows = static_cast<int>(world.terrainMap.size());
    int cols = static_cast<int>(world.terrainMap[0].size());
    if (start.first < 0 || start.first >= rows || start.second < 0 || start.second >= cols ||
        dest.first < 0 || dest.first >= rows || dest.second < 0 || dest.second >= cols) {
        return 999999;
    }
    return calculatePathCost(world.terrainMap, shortestPath(world.terrainMap, start, dest));
}

bool sortByDistance(Coord a, Coord b, Coord referencePoint, AntWorld& world){
    return shortestPathCost(a, referencePoint, world) < shortestPathCost(b, referencePoint, world);
}

bool roundTrip(Ant& ant, Coord dest, AntWorld& world){
    int tripCost = shortestPathCost(ant.position, dest, world) + shortestPathCost(dest, ant.homeCoord, world);
    if (tripCost < ant.energy){ // round trip possible with spare energy
        safeMove(ant, dest, world);
        safeMove(ant, ant.homeCoord, world); 
        return true;
    }
    // round trip not possible
    return false;
}

// allows the ant to move without risking being removed from ants (this would cause pointer issues for my structs)
Coord safeMove(Ant& ant, Coord dest, AntWorld& world){
    // Ant determines reachability: reject out-of-bounds destinations before
    // touching shortestPath (move() only guards >= size, not negatives).
    int rows = static_cast<int>(world.terrainMap.size());
    int cols = static_cast<int>(world.terrainMap[0].size());
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

// Territory methods:

Territory::Territory(Coord home, int scoutIndex, int numScouts, const MapTemplate& map)
    : home(home), map(&map) {
    int rows = static_cast<int>(map.size());
    int cols = static_cast<int>(map[0].size());

    // 1. Collect all grid cell angles relative to home in [0, 2*PI)
    std::vector<double> angles;
    angles.reserve(rows * cols - 1);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            Coord cell(r, c);
            if (cell == home) continue;
            angles.push_back(angle(cell));
        }
    }
    std::sort(angles.begin(), angles.end());

    // 2. Evenly partition the sorted cells among the scouts
    int totalCells = static_cast<int>(angles.size());
    int startIndex = (scoutIndex * totalCells) / numScouts;
    int endIndex   = ((scoutIndex + 1) * totalCells) / numScouts;

    // Scout 0 begins at angle 0.0; subsequent scouts begin at the prior scout's end boundary
    start = (scoutIndex == 0) ? 0.0 : angles[startIndex - 1];

    // Final scout covers up to 2*PI; others cover up to their partition's last cell angle
    end = (scoutIndex + 1 == numScouts) ? 2.0 * PI : angles[endIndex - 1];
}

// returns whether point p is within the bounds of theterritory
bool Territory::within(Coord p) const {
    double a = angle(p);
    return a >= start && a <= end;
}

//returns the angle of point p relative to home
double Territory::angle(Coord p) const {
    double a = std::atan2(p.second - home.second, p.first - home.first);
    if (a < 0.0) a += 2.0 * PI;
    return a;
}

Coord Territory::nextVantage(Coord currentVantage, Coord& heading) {
    int rows = static_cast<int>(map->size());
    int cols = static_cast<int>((*map)[0].size());

    // 1. Continue along current heading if in bounds and within territory
    Coord straight = currentVantage + heading;
    if (straight.first >= 0 && straight.first < rows &&
        straight.second >= 0 && straight.second < cols &&
        within(straight)) {
        return straight;
    }

    // 2. Reached boundary: reverse heading for the return sweep
    heading = heading * -1;

    // 3. Jump outward ~2*foodRadius away from home
    Coord homeToVantage = currentVantage - home;
    double radialDist = std::hypot(homeToVantage.first, homeToVantage.second);
    double jumpDist = std::max(2.0, std::hypot(heading.first, heading.second) * 2.0);

    Coord radialStep(0, 0);
    if (radialDist > 0.0) {
        radialStep = Coord(
            static_cast<int>(std::round((homeToVantage.first / radialDist) * jumpDist)),
            static_cast<int>(std::round((homeToVantage.second / radialDist) * jumpDist))
        );
    }

    Coord projectedVantage = currentVantage + radialStep;
    Coord jumpUp(
        std::clamp(projectedVantage.first, 0, rows - 1),
        std::clamp(projectedVantage.second, 0, cols - 1)
    );

    if (jumpUp != currentVantage) {
        return jumpUp;
    }

    // 4. If blocked by a wall, advance along the wall away from home
    Coord wallStep(0, 0);
    int stepDist = static_cast<int>(jumpDist);

    if (currentVantage.second == 0 || currentVantage.second == cols - 1) {
        // Vertical wall: advance along rows away from home
        int dir = (currentVantage.first >= home.first) ? 1 : -1;
        wallStep = Coord(dir * stepDist, 0);
    } else if (currentVantage.first == 0 || currentVantage.first == rows - 1) {
        // Horizontal wall: advance along columns away from home
        int dir = (currentVantage.second >= home.second) ? 1 : -1;
        wallStep = Coord(0, dir * stepDist);
    }

    Coord projectedWallVantage = currentVantage + wallStep;
    Coord wallJump(
        std::clamp(projectedWallVantage.first, 0, rows - 1),
        std::clamp(projectedWallVantage.second, 0, cols - 1)
    );

    if (wallJump != currentVantage && within(wallJump)) {
        return wallJump;
    }

    // 5. Try stepping along the reversed heading at map boundary
    Coord returnStep = currentVantage + heading;
    if (returnStep.first >= 0 && returnStep.first < rows &&
        returnStep.second >= 0 && returnStep.second < cols &&
        within(returnStep)) {
        return returnStep;
    }

    // 6. Entire sector traversed to map edge: restart patrol
    int foodRadius = static_cast<int>(std::round(std::hypot(heading.first, heading.second)));
    heading = startHeading(foodRadius);
    return startPoint(foodRadius);
}

// finds a point near the starting angle close to 2*foodRadius away from home
Coord Territory::startPoint(int foodRadius) {
    int maxDist = 2 * foodRadius;
    int rows = static_cast<int>(map->size());
    int cols = static_cast<int>((*map)[0].size());

    // Search window within 2 * foodRadius around home
    int minRow = std::max(0, home.first - maxDist);
    int maxRow = std::min(rows - 1, home.first + maxDist);
    int minCol = std::max(0, home.second - maxDist);
    int maxCol = std::min(cols - 1, home.second + maxDist);

    Coord bestCoord = home;
    double bestScore = 1e9;

    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            Coord p(row, col);
            if (p == home || !within(p)) continue;

            Coord offset = p - home;
            double dist = std::hypot(offset.first, offset.second);
            if (dist <= maxDist + 0.5) {
                // Angular distance from territory start
                double a = angle(p);
                double angleDiff = std::abs(a - start);
                if (angleDiff > PI) angleDiff = 2.0 * PI - angleDiff;

                // Prefer coords near 2*foodRadius distance and closest to start angle
                double distDiff = std::abs(dist - maxDist);
                double score = distDiff * 2.0 + angleDiff;
                if (score < bestScore) {
                    bestScore = score;
                    bestCoord = p;
                }
            }
        }
    }
    return bestCoord;
}

Coord Territory::startHeading(int foodRadius) {
    Coord startCoord = startPoint(foodRadius);
    int rows = static_cast<int>(map->size());
    int cols = static_cast<int>((*map)[0].size());

    // Vector from home to startCoord and its radial distance
    Coord homeToStart = startCoord - home;
    double radialDist = std::hypot(homeToStart.first, homeToStart.second);

    // Target point on the end ray at the same distance from home (chord across the sector)
    Coord targetOnEndRay(
        static_cast<int>(std::round(home.first + radialDist * std::cos(end))),
        static_cast<int>(std::round(home.second + radialDist * std::sin(end)))
    );

    // Vector connecting startCoord across the sector to the end ray
    Coord sweepVector = targetOnEndRay - startCoord;
    if (sweepVector == Coord(0, 0)) {
        // Fallback to tangent if startCoord is already at the end ray
        sweepVector = Coord(-homeToStart.second, homeToStart.first);
    }

    double sweepLength = std::hypot(sweepVector.first, sweepVector.second);
    Coord heading(0, 0);
    if (sweepLength > 1e-4) {
        heading = Coord(
            static_cast<int>(std::round((sweepVector.first / sweepLength) * foodRadius)),
            static_cast<int>(std::round((sweepVector.second / sweepLength) * foodRadius))
        );
    }

    // Clamp next step to stay strictly in map bounds
    Coord nextCoord = startCoord + heading;
    Coord clampedCoord(
        std::clamp(nextCoord.first, 0, rows - 1),
        std::clamp(nextCoord.second, 0, cols - 1)
    );
    heading = clampedCoord - startCoord;

    // Ensure non-zero heading
    if (heading == Coord(0, 0)) {
        if (startCoord.first + 1 < rows) heading = Coord(1, 0);
        else if (startCoord.first - 1 >= 0) heading = Coord(-1, 0);
        else if (startCoord.second + 1 < cols) heading = Coord(0, 1);
        else heading = Coord(0, -1);
    }

    return heading;
}
