#pragma once

#include "../include/ConnectionHandler.h"
#include "../include/event.h"
#include <map>
#include <vector>
#include <mutex>
#include <set>

// TODO: implement the STOMP protocol
class StompProtocol
{

    struct gameState {
        std::string teamA;
        std::string teamB;
        std::map<std::string, std::string> generalStats;
        std::map<std::string, std::string> team_a_stats;
        std::map<std::string, std::string> team_b_stats;
        std::set<Event> events;

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

    //std::vector<std::string> splitFrame(const std::string& frame, char delimiter);
    Event frameToEvent(std::string frame);
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
    std::string Summery(std::string gameName, std::string user);
    bool prossesEvent(Event event, std::string& user);

    //server->client
    bool prossesFrame(std::string frame);


    // geters (no need for setters)
    bool getConnected() const {return isConnected;}
    bool isSubTo(std::string gameName) const;
    bool isSubTo(int subId) const;
    //bool getAction(int reciptId);
};
