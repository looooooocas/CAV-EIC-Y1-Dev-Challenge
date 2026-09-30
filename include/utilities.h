#ifndef DEV_CHALLENGE_UTILITY_H
#define DEV_CHALLENGE_UTILITY_H

#include "antworld.h"
#include <vector>
#include <cmath>

inline constexpr double PI = 3.14159265358979323846;

// Coordinate operator overloads
Coord operator+(Coord a, Coord b);
Coord operator-(Coord a, Coord b);
Coord operator*(Coord a, int scalar);

// Territory representation
struct Territory {
    Coord home;
    double start;
    double end;

    Territory() = default;
    Territory(Coord home, double start, double end)
        : home(home), start(start), end(end) {}

    bool within(Coord p) const;
    Coord nextVantage(Coord currentVantage);
};

// Pathfinding & Movement helpers
int shortestPathLength(Coord start, Coord dest, AntWorld& world);
bool sortByDistance(Coord a, Coord b, Coord referencePoint, AntWorld& world);
bool roundTrip(Ant& ant, Coord dest, AntWorld& world);

// Debugging / Logging
void outputEnergies(std::vector<Ant> ants);

#endif // DEV_CHALLENGE_UTILITY_H
