#ifndef signalSearch
#define signalSearch

#include <vector>
#include  <random>
#include "utility_functions.h"
#include "antworld.h"

#include "utilities.h"

void signallingAnts(AntWorld& world, int numScouts = 4);
void resetSignalSearch();

#endif