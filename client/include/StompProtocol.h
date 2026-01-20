#pragma once

#include "../include/ConnectionHandler.h"
#include "../include/event.h"
#include <map>
#include <vector>
#include <mutex>
#include <set>

class StompProtocol
{
    
    struct gameState {
        std::string teamA = "";
        std::string teamB = "";
        std::map<std::string, std::string> generalStats = {};
        std::map<std::string, std::string> team_a_stats = {};
        std::map<std::string, std::string> team_b_stats = {};
        std::set<Event> events = {};
    };

private:

    std::string username;
    bool isConnected;
    int subIDCounter;
    int reciptIDCounter;
    int waitingReciptID;
    mutable std::mutex mtx;
    std::condition_variable cv;
    bool responseReceived;
    bool shouldTerminate;

    // subscriptionID <-> game
    std::map<std::string, int> gameToSubId;
    std::map<int, std::string> subIdToGame;

    // reciptID -> Action
    //std::map<int, std::string> reciptAction;

    // game -> user -> events
    std::map<std::string, std::map<std::string, gameState>> gameData;

    //std::vector<std::string> splitFrame(const std::string& frame, char delimiter);
    //Event frameToEvent(std::string frame);
public:

    StompProtocol():username(""),isConnected(false),subIDCounter(0),reciptIDCounter(0),waitingReciptID(0),mtx(),cv(),responseReceived(false),shouldTerminate(false),gameToSubId({}),subIdToGame({}),gameData({}){}

    struct Frame {
        std::string type = "";
        std::string frameID = "";
        std::string msg = "";
    };

    void waitForResponse(int reciptID = 0);
    void notifyResponse(int reciptID = 0, bool instent = false);

    bool Login(std::string username);
    std::string buildConnectionFrame(std::string host, std::string username, std::string password);
    bool Logout();
    std::string buildDisconnectFrame(int reciptID);
    bool Join(std::string gameName, int subID);
    std::string buildSubscribeFrame(std::string gameName, int subID, int reciptID);
    //bool Exit(std::string gameName);
    bool Exit(int subId);
    std::string buildUnsubscribeFrame(int subID, int reciptID);
    std::string Report(std::string filePath);

    //client->self
    void Summery(std::string gameName, std::string user, std::string filePath);
    bool prossesEvent(Event event, std::string& user);

    //server->client
    bool prossesFrame(std::string frame);
    std::vector<std::string> splitFrame(const std::string& frame, char delimiter);
    Frame parseFrame(std::string input);
    Event frameToEvent(std::string frame);


    // geters / setters
    bool getConnected() {return isConnected;}
    bool isSubTo(std::string gameName);
    bool isSubTo(int subId);
    int gameToSubID(std::string gameName) {std::lock_guard<std::mutex> lock(mtx); return gameToSubId[gameName];}
    //bool getAction(int reciptId);
    int getNewSubID() {return ++subIDCounter;}
    int getNewReciptID() {return ++reciptIDCounter;}
    bool getTerminate() {return shouldTerminate;}
    void setTerminate(bool val) {shouldTerminate = val;}
    int getCurrentlyWaiting() {return waitingReciptID;}

    Frame frameToFrame(std::string input);
};
