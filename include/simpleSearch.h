#ifndef SIMPLE_SEARCH_H
#define SIMPLE_SEARCH_H

#include "antworld.h"
#include "utilities.h"
#include <vector>

void simpleSearch(AntWorld& world);
void simpleSearch(Ant& ant, AntWorld& world);
void resetSimpleSearch();

void tests();
bool testSimpleSearchTransitions();

#endif // SIMPLE_SEARCH_H
