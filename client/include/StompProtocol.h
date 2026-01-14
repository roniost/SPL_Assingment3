#pragma once

#include "../include/ConnectionHandler.h"
#include "../include/event.h"
#include <map>
#include <vector>
#include <mutex>

// TODO: implement the STOMP protocol
class StompProtocol
{

    struct gameState {
        std::string teamA;
        std::string teamB;
        std::map<std::string, std::string> generalStats;
        std::map<std::string, std::string> team_a_stats;
        std::map<std::string, std::string> team_b_stats;
        std::vector<Event> events;

        gameState():teamA(""), teamB(""), generalStats({}), team_a_stats({}), team_b_stats({}), events() {}
    };
private:
    std::string username;
    bool isConnected;

    // subscriptionID <-> game
    std::map<std::string, int> gameToSubId;
    std::map<int, std::string> subIdToGame;
    // reciptID -> Action
    //std::map<int, std::string> reciptAction;
    // game -> user -> events
    std::map<std::string, std::map<std::string, gameState>> gameData;

    std::vector<std::string> splitFrame(const std::string& frame, char delimiter);
public:

    // consturcotr
    StompProtocol();

    // client->server
    bool Login(std::string username);
    bool Logout();
    bool Join(std::string gameName, int subId, int reciptId);
    bool Exit(std::string gameName);
    bool Exit(int subId);
    std::vector<std::string> Report(std::string filePath);

    //client->self
    bool Summery(std::string gameName, std::string user, std::string filePath);

    //server->client
    Event frameToEvent(std::string frame);
    bool prossesFrame(std::string frame);

    // geters (no need for setters)
    bool getConnected(){return isConnected;}
    bool isSubTo(std::string gameName);
    bool isSubTo(int subId);
    //bool getAction(int reciptId);
};
