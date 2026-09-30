
#include "antworld.h"
#include "utility_functions.h"
#include "utilities.h"

class Carrier{ // wrapper for carrier ant; allows us to track other info for carrier
    public:
    Ant* ant;
    int state = 0;
    Coord broadcastPoint;
    Coord vantagePoint;
    Coord heading;
    int signalCount = 0;
    bool signalBuffer[6] = {0};
    AntWorld* world;

    Carrier() : ant(nullptr), vantagePoint(-1, -1), world(nullptr) {}

    Carrier(Ant* ant, AntWorld& world){
        this->ant = ant;
        this->vantagePoint = ant->homeCoord; // outdated
        this->world = &world;
    }

    void assumeVantage(){
        // determine vantagePoint: closest point to homeCoord within pheromoneRange of broadcastPoint
        ant->move(world->terrainMap, vantagePoint, world->foodMap);
        //TODO: insufficient energy?
    }

    void receiveSignal(){
        // receive signal
        signalBuffer[signalCount] = world->pheromoneMap[broadcastPoint.first][broadcastPoint.second];
        signalCount++;
        if (signalCount == 6){ // whole signal received
            int relativeFirst = signalBuffer[0] * 4 + signalBuffer[1] * 2 + signalBuffer[2];
            int relativeSecond = signalBuffer[3] * 4 + signalBuffer[4] * 2 + signalBuffer[5];
            Coord foodCoord = Coord(relativeFirst, relativeSecond) - Coord(ant->foodRadius, ant->foodRadius) + broadcastPoint;

            // Do these comparisons work? its just a pair of ints
            if (foodCoord == Coord(0,0)){ // no signal, assumed dead scout
                state = 3; // kills self TODO: suicide is never the optimal solution
            }
            else if (foodCoord == Coord(7, 7)){ // "over" signal
                Coord temp = broadcastPoint;
                broadcastPoint = nextVantage();
                vantagePoint = temp;
                state = 2; // receiveVantageSignal
            }
            else if(shortestPathLength(ant->position, foodCoord, *world) * 2 <= ant->energy // round trip possible //TODO: reorg the conditions
                && world->pheromoneMap[ant->position.first][ant->position.second] == 0){ // signal unclaimed by another carrier
                ant->dropPheromone(world->pheromoneMap); // mark that the signal has been answered (for other carriers to know to skip) TODO: erase this somewhere
                roundTrip(*ant, foodCoord, *world);
                assumeVantage(); // only needed if we are using a dynamic scouting path
            }
            signalCount = 0; // reset signal count and buffer
            std::fill(signalBuffer, signalBuffer + 6, 0);
        }
    }

    void receiveVantageSignal(){ // UNIMPLEMENTED 
        // receive signal
        signalBuffer[signalCount] = world->pheromoneMap[broadcastPoint.first][broadcastPoint.second];
        signalCount++;
        if (signalCount == 6){ // whole signal received
            int relativeFirst = signalBuffer[0] * 4 + signalBuffer[1] * 2 + signalBuffer[2];
            int relativeSecond = signalBuffer[3] * 4 + signalBuffer[4] * 2 + signalBuffer[5];
            broadcastPoint = Coord(relativeFirst - ant->foodRadius + broadcastPoint.first, relativeSecond - ant->foodRadius + broadcastPoint.second);
            if(shortestPathLength(ant->position, broadcastPoint, *world) * 2 <= ant->energy){ // round trip possible. TODO: reorg the conditions
                assumeVantage(); // only needed if we are using a dynamic scouting path
            }
            signalCount = 0; // reset signal count and buffer
            std::fill(signalBuffer, signalBuffer + 6, 0);
        }
    }

    void forage(){
        switch(state){
            case 0: assumeVantage();
            break;
            case 1: receiveSignal();
            break;
            case 2: 
            break;
            case 3: ant->energy = 0; // kills itself
            break;
        }
    }
};

class Scout{ // wrapper for scout ant; allows us to track other info for scout
    public: 
    Ant* ant;
    int state = 0;
    int signalCount = 0;
    Coord vantagePoint;
    AntWorld* world;

    Scout(Ant* ant, AntWorld& world){
        this->ant = ant;
        this->vantagePoint = ant->homeCoord;
        this->world = &world;
    }

    void assumeVantage(){
        // find next vantage

        ant->move(world->terrainMap, vantagePoint, world->foodMap);
        state++;
    }

    //Breaks if the food coord changes before the signal is complete
    void signalCoord(Coord foodCoord){
        ant->erasePheromone(world->pheromoneMap);
        if (signalCount < 3){ // x coord
            int relativeFirst = foodCoord.first - ant->position.first + ant->foodRadius; // gives us a number from 0 - foodRadius
            unsigned bit = (relativeFirst >> (signalCount - 3)) & 1;
            if (bit){
                ant->dropPheromone(world->foodMap);
            }
            signalCount++;
        } else if (signalCount < 6 ){ // y coord
            int relativeSecond = foodCoord.second - ant->position.second + 2*ant->foodRadius;
            unsigned bit = (relativeSecond >> (signalCount - 6)) & 1;
            if (bit){
                ant->dropPheromone(world->foodMap);
            }
            signalCount++;
        }
        if (signalCount >= 6){ // this signal is over, check if food was removed, reset for the next
            signalCount = 0;
            if(world->foodMap[foodCoord.first][foodCoord.second] == 1){ // food wasn't picked up, do it its self
                ant->move(world->terrainMap, foodCoord, world->foodMap);
                ant->returnHome(world->terrainMap, world->foodMap);
                state = 3; //kills self, TODO: hould be solo explore 
            }
        }
    }

    bool signalOver(){ // TODO: add signal next vantage coord (different format?)
        if (signalCount < 6){
            ant->dropPheromone(world->pheromoneMap);
            signalCount++;
            return true;
        }
        else {
            ant->erasePheromone(world->pheromoneMap);
            signalCount = 0;
            state = 0;
            vantagePoint = nextVP(vantagePoint, *world);
            return false;
        }
    }

    void signalFood(){ // if sees food, signals it; if not, tells us to change state
        std::vector<Coord> visibleFood = ant->foodScan(world->foodMap);
        if (visibleFood.empty()){
            state++;
            return;
        }
        std::sort(visibleFood.begin(), visibleFood.end(), [this](Coord a, Coord b){
            return sortByDistance(a, b, ant->homeCoord, *world);
        });
        signalCoord(visibleFood[0]);
    }

    void forage(){
        switch(state){
            case 0: // getVantage
            assumeVantage();
            break;
            case 1: // signalFood
            signalFood();
            break;
            case 2: // signalOver
            signalOver();
            break;
            case 3: // scouting over: no more carriers OR scout out of energy
            ant->energy = 0; // kills itself
            break;
        }
    }
};

// Organization
class ScoutTeam{
    public: 
    Scout scout;
    Territory territory;
    std::vector<Carrier> carriers;

    ScoutTeam(Ant* scoutAnt, Territory territory, AntWorld& world)
        : scout(scoutAnt, world) {

        this->terrainMap = &world.terrainMap;
        this->foodMap = &world.foodMap;
        this->pheromoneMap = &world.pheromoneMap;
        this->territory = territory;

        for (auto& carrier : carriers){
            carrier.broadcastPoint = scout.vantagePoint;
        }
    }

    void signalCoord(Coord c, int signalCount, AntWorld& world);
    
    void assumeVantages(){ // just pass it along TODO? just handle it here
        scout.assumeVantage();
        for (auto& carrier : carriers){
            carrier.assumeVantage();
        }
    }

    private:
    unsigned int signalCounter = 0;
    MapTemplate* terrainMap;
    MapTemplate* foodMap;
    MapTemplate* pheromoneMap;
};

std::vector<ScoutTeam> createTeams(std::vector<Ant> ants, AntWorld& world, int numTeams = 4){
    std::vector<ScoutTeam> teams;
    teams.clear();
    if (ants.empty()) return teams;

    std::sort(ants.begin(), ants.end(), [](Ant a, Ant b) { return a.energy > b.energy; });
    size_t i = 0;
    while(i < numTeams && i < ants.size()){
        Territory territory = Territory(ants[i].homeCoord, (i) * PI/numTeams, (i + 1) * PI/numTeams); // angular slice of the map (from home) that the team is responsible for searching
        teams.emplace_back(&ants[i], territory, world);
        i++;
    }
    while (i < ants.size()){
        teams[i % teams.size()].carriers.emplace_back(&ants[i], world);
        i++;
    }
    return teams;
}

void clearImmediateArea(std::vector<Ant>& ants, AntWorld& world){
    std::sort(ants.begin(), ants.end(), [](const Ant& a, const Ant& b){return a.energy < b.energy;}); // sort ants lowest energy to highest
    std::vector<Coord> visibleFood = ants[0].foodScan(world.foodMap); // see food
    for (auto food : visibleFood){ // gather each food with the lowest energy ant that can make the round trip
        for (auto& ant : ants){
            if (roundTrip(ant, food, world)){
                break;
            }
        }
    }
    // TODO: if no ant can make the round trip (shouldn't really happen)
}

void signallingAnts(AntWorld& world){ // handles first stage control, team creation, and team organization
    static int phase = 0;
    static std::vector<ScoutTeam> teams;
    switch (phase){
        case 0: 
            clearImmediateArea(world.ants, world);
            teams = createTeams(world.ants, world);
            phase = 1;
            break;
        case 1: // allow the ants to determine their own behaviour
            for (auto team : teams){
                team.scout.forage();
                for (auto carrier : team.carriers){
                    carrier.forage();
                }
            }       
    }
}