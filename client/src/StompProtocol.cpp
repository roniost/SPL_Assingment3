#pragma once

#include "../include/StompProtocol.h"
//#include "../include/ConnectionHandler.h"
#include "../include/event.h"
#include <map>
#include <vector>
#include <mutex>
#include <sstream>

StompProtocol::StompProtocol():username(""),isConnected(false) {}

// client requests
bool StompProtocol::Login(std::string username) {
    if(isConnected)
        //throw std::runtime_error("already connected");
        return false;
    this->username = username;
    this->isConnected = true;
    return true;
}

bool StompProtocol::Logout() {
    if(!isConnected)
        //throw std::runtime_error("not connected");
        return false;
    this->isConnected = false;
    this->username = "";
    return true;
}

bool StompProtocol::Join(std::string gameName, int subId, int reciptId) {
    if(!isConnected)
        throw std::runtime_error("not connected to server");
    else if (isSubTo(gameName))
        return false;
    
    subIdToGame[subId] = gameName;
    gameToSubId[gameName] = subId;
    return true;
}

bool StompProtocol::Exit(std::string gameName) {
    if(!isSubTo(gameName))
        return false;
    
    int subId = gameToSubId[gameName];
    gameToSubId.erase(gameName);
    subIdToGame.erase(subId);
    gameData.erase(gameName);
    return true;
}

bool StompProtocol::Exit(int subId) {
    if(!isSubTo(subId))
        return false;
    
    std::string gameName = subIdToGame[subId];
    gameToSubId.erase(gameName);
    subIdToGame.erase(subId);
    gameData.erase(gameName);
    return true;
}

// server -> client

// support function for prossesing frames
std::vector<std::string> splitFrame(const std::string& frame, char delimiter) {
    std::vector<std::string> split;
    std::string line;
    std::stringstream ss(frame);
    while (std::getline(ss, line, delimiter)) {
        split.push_back(line);
    }
    return split;
}

Event StompProtocol::frameToEvent(std::string frame) {
    // headers and fields for event creation
    std::string teamA, teamB, eventName, discription;
    int eventTime = 0;
    std::map<std::string, std::string> gameUpdates, teamAUpdates, teamBUpdates;

    std::vector<std::string> lines = splitFrame(frame, '\n');

    // frame will always contain the fields in a spesific order
    // find first field
    size_t i = 0;
    while(!lines[i].compare(0, 6, "team a"))
        i++;
    
        std::map<std::string, std::string>* currentUpdateMap = &gameUpdates;

    for (; i < lines.size(); i++) {
        std::string line = lines[i];
        
        // Find the split point between Key and Value
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue; // Skip empty lines

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1); // skips space after colon

        if (key == "team a") {
            teamA = value;
        } 
        else if (key == "team b") {
            teamB = value;
        } 
        else if (key == "event name") {
            eventName = value;
        } 
        else if (key == "time") {
            if(value.empty())
                eventTime = 0;
            else
                eventTime = std::stoi(value);
        } 
        else if (key == "description") {
            discription = value;
        }
        else if (key == "general game updates") {
            currentUpdateMap = &gameUpdates;
        }
        else if (key == "team a updates") {
            currentUpdateMap = &teamAUpdates;
        }
        else if (key == "team b updates") {
            currentUpdateMap = &teamBUpdates;
        }
        else {
            // If it's none of the main headers it belongs to the currently active map
            (*currentUpdateMap)[key] = value;
        }
    }
    currentUpdateMap = nullptr;
    return Event(teamA, teamB, eventName, eventTime, gameUpdates, teamAUpdates, teamBUpdates, discription);
}

bool StompProtocol::prossesFrame(std::string frame) {
    Event newEvent = frameToEvent(frame);
    std::string user;
    std::vector<std::string> lines = splitFrame(frame, '\n');
    //find user who submitted this event
    for (size_t i = 0; i < lines.size(); i++) {
        std::string line = lines[i];
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue; // Skip empty lines
        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        if(key=="user") {
            user = value;
            break;
        }
    }

    std::string gameName = newEvent.get_team_a_name() + "_" + newEvent.get_team_b_name();
    if(!gameToSubId.count(gameName)) {
        return false; // not subscribed to game
    }
    
    if(!gameData.count(gameName) || !gameData[gameName].count(user)) {
        gameState state;
        state.teamA = newEvent.get_team_a_name();
        state.teamB = newEvent.get_team_b_name();
        state.generalStats = newEvent.get_game_updates();
        state.team_a_stats = newEvent.get_team_a_updates();
        state.team_b_stats = newEvent.get_team_b_updates(); 
        state.events.push_back(newEvent);
        if(!gameData.count(gameName)) {
            std::map<std::string, gameState> newMap;
            newMap[user] = state;
            gameData[gameName] = newMap;
        }
        else {
            gameData[gameName][user] = state;
        }
        return true;
    }
    else {
        for(auto pair : newEvent.get_game_updates()) {
            std::string key = pair.first;
            gameData[gameName][user].generalStats[key] = pair.second;
        }
        for(auto pair : newEvent.get_team_a_updates()) {
            std::string key = pair.first;
            gameData[gameName][user].team_a_stats[key] = pair.second;
        }
        for(auto pair : newEvent.get_team_b_updates()) {
            std::string key = pair.first;
            gameData[gameName][user].team_b_stats[key] = pair.second;
        }
        gameData[gameName][user].events.push_back(newEvent);
        return true;
    }
}

// getters
bool StompProtocol::isSubTo(std::string gameName) {
    return !!gameToSubId.count(gameName);
}

bool StompProtocol::isSubTo(int subId) {
    return !!subIdToGame.count(subId);
}
