#pragma once

#include "../include/StompProtocol.h"
//#include "../include/ConnectionHandler.h"
#include "../include/event.h"
#include <map>
#include <vector>
#include <mutex>
#include <sstream>

StompProtocol::StompProtocol():username(""),isConnected(false) {}

// client -> server
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

std::vector<std::string> StompProtocol::Report(std::string filePath) {
    if(!isConnected)
        throw std::runtime_error("not connected to server");
    names_and_events input = parseEventsFile(filePath);
    std::string gameName = input.team_a_name + "_" + input.team_b_name;
    std::vector<std::string> out;

    for(Event event : input.events) {
        std::string msg = "";
        msg.append("SEND\n").append("destination:/" + gameName + "\n\n");
        msg.append("user: " + username + "\n");
        msg.append(event.toString() + "\n\0");
        out.push_back(msg);
    }

    return out;
}

// client -> self
std::string StompProtocol::Summery(std::string gameName, std::string user) {
    if(!isSubTo(gameName))
        throw std::invalid_argument("not subscribed to game");
    gameState game = gameData[gameName][user];
    if(game.events.size()==0)
        throw std::invalid_argument("no existing reports from this user");
    
    std::string out = "";
    out.append(game.teamA + " vs " + game.teamB + "\n");
    out.append("Game stats:\n");
    
    out.append("General stats:\n");
    for(auto pair : game.generalStats)
        out.append(pair.first + ": " + pair.second + "\n");

    out.append("\n" + game.teamA + " stats:\n");
    for(auto pair : game.team_a_stats)
        out.append(pair.first + ": " + pair.second + "\n");

    out.append("\n" + game.teamB + " stats:\n");
    for(auto pair : game.team_b_stats)
        out.append(pair.first + ": " + pair.second + "\n");
    
    out.append("\nGame event reports:");
    for(Event event : game.events)
        out.append("\n" + std::to_string(event.get_time()) + " - " + event.get_name() + ":\n\n" + event.get_discription() + "\n\n");

    return out;
}

bool StompProtocol::prossesEvent(Event event, std::string& user) {
    std::string gameName = event.get_team_a_name() + "_" + event.get_team_b_name();
    if(!gameToSubId.count(gameName)) {
        return false; // not subscribed to game
    }

    if(!gameData.count(gameName) || !gameData[gameName].count(user)) {
        gameState state;
        state.teamA = event.get_team_a_name();
        state.teamB = event.get_team_b_name();
        state.generalStats = event.get_game_updates();
        state.team_a_stats = event.get_team_a_updates();
        state.team_b_stats = event.get_team_b_updates(); 
        state.events.insert(event);
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
        for(auto pair : event.get_game_updates()) {
            std::string key = pair.first;
            gameData[gameName][user].generalStats[key] = pair.second;
        }
        for(auto pair : event.get_team_a_updates()) {
            std::string key = pair.first;
            gameData[gameName][user].team_a_stats[key] = pair.second;
        }
        for(auto pair : event.get_team_b_updates()) {
            std::string key = pair.first;
            gameData[gameName][user].team_b_stats[key] = pair.second;
        }
        gameData[gameName][user].events.insert(event);
        return true;
    }
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
            while(key[0]==' ')
                key.substr(1);
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
    this->prossesEvent(newEvent, user);
}

// getters
bool StompProtocol::isSubTo(std::string gameName) const{
    return !!gameToSubId.count(gameName);
}

bool StompProtocol::isSubTo(int subId) const{
    return !!subIdToGame.count(subId);
}
