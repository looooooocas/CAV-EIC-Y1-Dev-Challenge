//
// Created by dusan on 9/15/26.
//

#include "../include/antworld.h"
#include "utility_functions.h"
#include <memory>
#include <utility>


inline constexpr double PI = 3.14159265358979323846;

void dumbSearch(Ant& ant, AntWorld& world);
void signallingAnts(AntWorld& world);
/** @brief this is where you as the applicant will make use of the above functions to develop your solution.
 * here are some existing examples of how calling these functions works to help get you started!
 */
void AntWorld::forage() {

    signallingAnts(*this);

    // for(Ant& ant : this->ants){
    //     dumbSearch(ant, *this);
    // }
}

/** You may insert any custom functions below **/

// --------------------------------------------------------------------------------------------



// Ant Roles -------------------------------------------------------------------------------------------


