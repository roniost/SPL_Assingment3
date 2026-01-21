#include "../include/StompProtocol.h"
//#include "../include/ConnectionHandler.h"
#include "../include/event.h"
#include <map>
#include <vector>
#include <mutex>
#include <sstream>
#include <fstream>

void StompProtocol::waitForResponse(int reciptID) {
    std::cout << "[DEBUG] waitForResponse called. Waiting for ReciptID: " << reciptID << std::endl;
    std::unique_lock<std::mutex> lock(mtx);
    waitingReciptID = reciptID;
    cv.wait(lock, [this] { return responseReceived; });
    std::cout << "[DEBUG] waitForResponse finished for ReciptID: " << reciptID << std::endl;
    responseReceived = false; // Reset for next time
}

void StompProtocol::notifyResponse(int reciptID, bool instent) {
    std::cout << "[DEBUG] notifyResponse called. ReciptID: " << reciptID << ", Instant: " << instent << std::endl;
    if(!instent && waitingReciptID != reciptID) {
        //std::cout << "incorrect recipt id recived, continuing to wait\n";
        std::cout << "[DEBUG] notifyResponse: Incorrect recipt id received (Expected: " << waitingReciptID << ", Got: " << reciptID << "), continuing to wait\n";
        return;
    }
    std::lock_guard<std::mutex> lock(mtx);
    responseReceived = true;
    cv.notify_one();
    std::cout << "[DEBUG] notifyResponse: Notification sent." << std::endl;
}

// client -> server
bool StompProtocol::Login(std::string username) {
    std::cout << "[DEBUG] Login called for user: " << username << std::endl;
    std::lock_guard<std::mutex> lock(mtx);
    if(isConnected) {
        std::cout << "[DEBUG] Login failed: Already connected." << std::endl;
        return false;
    }
    this->username = username;
    this->isConnected = true;
    std::cout << "[DEBUG] Login successful." << std::endl;
    return true;
}

std::string StompProtocol::buildConnectionFrame(std::string host, std::string username, std::string password) {
    std::cout << "[DEBUG] Building CONNECTION frame for host: " << host << ", user: " << username << std::endl;
    std::string frame = "CONNECT\naccept -version :1.2\n";
    frame.append("host:" + host + "\n");
    frame.append("login:" + username + "\n");
    frame.append("passcode:" + password + "\n");
    frame.append("\n");
    return frame;
}

bool StompProtocol::Logout() {
    std::cout << "[DEBUG] Logout called." << std::endl;
    std::lock_guard<std::mutex> lock(mtx);
    if(!isConnected) {
        std::cout << "[DEBUG] Logout failed: Not connected." << std::endl;
        return false;
    }
    this->isConnected = false;
    this->username = "";
    std::cout << "[DEBUG] Logout successful." << std::endl;
    return true;
}

std::string StompProtocol::buildDisconnectFrame(int reciptID) {
    std::cout << "[DEBUG] Building DISCONNECT frame. ReciptID: " << reciptID << std::endl;
    std::string frame = "DISCONNECT\n";
    frame.append("receipt:" + std::to_string(reciptID) + "\n");
    frame.append("\n");
    return frame;
}

bool StompProtocol::Join(std::string gameName, int subId) {
    std::cout << "[DEBUG] Join called. Game: " << gameName << ", SubID: " << subId << std::endl;
    std::lock_guard<std::mutex> lock(mtx);
    if(!isConnected) {
        std::cout << "[DEBUG] Join failed: Not connected." << std::endl;
        return false;
    }
    else if (!isSubTo(gameName)) {
        std::cout << "[DEBUG] Join failed: Not subscribed logic check (Note: Logic seems inverted in original code?)" << std::endl;
        return false;
    }
    subIdToGame[subId] = gameName;
    gameToSubId[gameName] = subId;
    std::cout << "[DEBUG] Join successful. Mapped " << gameName << " to " << subId << std::endl;
    return true;
}

std::string StompProtocol::buildSubscribeFrame(std::string gameName, int subID, int reciptID) {
    std::cout << "[DEBUG] Building SUBSCRIBE frame. Game: " << gameName << ", SubID: " << subID << ", ReciptID: " << reciptID << std::endl;
    std::string frame = "SUBSCRIBE\n";
    frame.append("destination:/" + gameName);
    frame.append("id:" + std::to_string(subID) + "\n");
    frame.append("receipt:" + std::to_string(reciptID) + "\n");
    frame.append("\n");
    return frame;
}

/*bool StompProtocol::Exit(std::string gameName) {
    if(!isSubTo(gameName))
        return false;
    
    int subId = gameToSubId[gameName];
    gameToSubId.erase(gameName);
    subIdToGame.erase(subId);
    gameData.erase(gameName);
    return true;
}*/

bool StompProtocol::Exit(int subId) {
    std::cout << "[DEBUG] Exit called. SubID: " << subId << std::endl;
    std::lock_guard<std::mutex> lock(mtx);
    if(!isSubTo(subId)) {
        std::cout << "[DEBUG] Exit failed: Not subscribed to SubID " << subId << std::endl;
        return false;
    }
    std::string gameName = subIdToGame[subId];
    gameToSubId.erase(gameName);
    subIdToGame.erase(subId);
    gameData.erase(gameName);
    std::cout << "[DEBUG] Exit successful. Removed " << gameName << std::endl;
    return true;
}

std::string StompProtocol::buildUnsubscribeFrame(int subID, int reciptID) {
    std::cout << "[DEBUG] Building UNSUBSCRIBE frame. SubID: " << subID << ", ReciptID: " << reciptID << std::endl;
    std::string frame = "UNSUBSCRIBE\n";
    frame.append("id:" + std::to_string(subID) + "\n");
    frame.append("receipt:" + std::to_string(reciptID) + "\n");
    frame.append("\n");
    return frame;
}

std::string StompProtocol::Report(std::string filePath) {
    std::cout << "[DEBUG] Report called. FilePath: " << filePath << std::endl;
    std::lock_guard<std::mutex> lock(mtx);
    if(!isConnected) {
        std::cout << "[DEBUG] Report failed: Not connected." << std::endl;
        return "";
    }
    names_and_events input = parseEventsFile(filePath);
    std::string gameName = input.team_a_name + "_" + input.team_b_name;
    std::cout << "[DEBUG] Parsed events for game: " << gameName << ", Count: " << input.events.size() << std::endl;
    std::string out;

    for(Event event : input.events) {
        std::string msg = "";
        msg.append("SEND\n").append("destination:/" + gameName + "\n\n");
        msg.append("user: " + username + "\n");
        msg.append(event.toString() + "\n\0\n");
        out.append(msg);
    }

    return out.substr(0, out.length()-2);
}

// client -> self
void StompProtocol::Summery(std::string gameName, std::string user, std::string filePath) {
    std::cout << "[DEBUG] Summary called. Game: " << gameName << ", User: " << user << ", Path: " << filePath << std::endl;
    std::lock_guard<std::mutex> lock(mtx);

    if(!isSubTo(gameName)) {
        std::cout << "[DEBUG] Summary error: Not subscribed to game." << std::endl;
        throw std::invalid_argument("not subscribed to game");
    }
    gameState game = gameData[gameName][user];
    if(game.events.size()==0) {
        std::cout << "[DEBUG] Summary error: No events found for user." << std::endl;
        throw std::invalid_argument("no existing reports from this user");
    }
    
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

    std::ofstream file(filePath);
    file << out;
    file.close();
    std::cout << "[DEBUG] Summary written to file successfully." << std::endl;
}

bool StompProtocol::prossesEvent(Event event, std::string& user) {
    std::cout << "[DEBUG] prossesEvent called. User: " << user << std::endl;
    std::lock_guard<std::mutex> lock(mtx);
    std::string gameName = event.get_team_a_name() + "_" + event.get_team_b_name();
    if(!gameToSubId.count(gameName)) {
        return false; // not subscribed to game
    }

    if(!gameData.count(gameName) || !gameData[gameName].count(user)) {
        std::cout << "[DEBUG] prossesEvent: New entry for game/user." << std::endl;
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
        std::cout << "[DEBUG] prossesEvent: Updating existing entry." << std::endl;
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

// support functions for prossesing frames
std::vector<std::string> StompProtocol::splitFrame(const std::string& frame, char delimiter) {
    std::vector<std::string> split;
    std::string line;
    std::stringstream ss(frame);
    while (std::getline(ss, line, delimiter)) {
        split.push_back(line);
    }
    return split;
}

StompProtocol::Frame StompProtocol::parseFrame(std::string input) {
    StompProtocol::Frame frame;
    std::vector<std::string> lines = splitFrame(input, '\n');
    frame.type = lines[0];
    frame.msg = input;
    frame.frameID = lines[1].substr(lines[1].find_first_of(":"), lines[1].length() - 1);
    return frame;
}



Event StompProtocol::frameToEvent(std::string frame) {
    std::cout << "[DEBUG] frameToEvent called." << std::endl;
    // headers and fields for event creation
    std::string teamA, teamB, eventName, discription;
    int eventTime = 0;
    std::map<std::string, std::string> gameUpdates, teamAUpdates, teamBUpdates;

    std::vector<std::string> lines = splitFrame(frame, '\n');

    // frame will always contain the headers in a spesific order
    // find first field
    size_t i = 0;
    while(!lines[i].compare(0, 6, "team a")) i++;
    
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
            while(key[0]==' ') key = key.substr(1);
            // If it's none of the headers its part of the currently updating map
            (*currentUpdateMap)[key] = value;
        }
    }
    currentUpdateMap = nullptr; // del
    return Event(teamA, teamB, eventName, eventTime, gameUpdates, teamAUpdates, teamBUpdates, discription);
}

bool StompProtocol::prossesFrame(std::string frame) {
    std::cout << "[DEBUG] prossesFrame called. Raw Frame size: " << frame.length() << std::endl;
    std::lock_guard<std::mutex> lock(mtx);

    StompProtocol::Frame msg = parseFrame(frame);
    std::cout << "[DEBUG] prossesFrame: Parsed type: " << msg.type << std::endl;
    std::string command = frame;
        
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
    return this->prossesEvent(newEvent, user);
}

StompProtocol::Frame StompProtocol::frameToFrame(std::string input) {
    std::vector<std::string> lines = splitFrame(input, '\n');
    Frame frame;
    frame.type = lines[0];
    frame.frameID = lines[1].substr(lines[1].find_first_of(":") + 1, lines[1].length());
    frame.msg = input;
    return frame;
}

// getters
bool StompProtocol::isSubTo(std::string gameName) {
    std::lock_guard<std::mutex> lock(mtx);
    return !!gameToSubId.count(gameName);
}

bool StompProtocol::isSubTo(int subId) {
    std::lock_guard<std::mutex> lock(mtx);
    return !!subIdToGame.count(subId);
}
