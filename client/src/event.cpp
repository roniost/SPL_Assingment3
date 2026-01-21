#include "../include/event.h"
#include "../include/json.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <sstream>
using json = nlohmann::json;

Event::Event(std::string team_a_name, std::string team_b_name, std::string name, int time,
             std::map<std::string, std::string> game_updates, std::map<std::string, std::string> team_a_updates,
             std::map<std::string, std::string> team_b_updates, std::string discription)
    : team_a_name(team_a_name), team_b_name(team_b_name), name(name),
      time(time), game_updates(game_updates), team_a_updates(team_a_updates),
      team_b_updates(team_b_updates), description(discription)
{
}

Event::~Event()
{
}

const std::string &Event::get_team_a_name() const
{
    return this->team_a_name;
}

const std::string &Event::get_team_b_name() const
{
    return this->team_b_name;
}

const std::string &Event::get_name() const
{
    return this->name;
}

int Event::get_time() const
{
    return this->time;
}

const std::map<std::string, std::string> &Event::get_game_updates() const
{
    return this->game_updates;
}

const std::map<std::string, std::string> &Event::get_team_a_updates() const
{
    return this->team_a_updates;
}

const std::map<std::string, std::string> &Event::get_team_b_updates() const
{
    return this->team_b_updates;
}

const std::string &Event::get_discription() const
{
    return this->description;
}

Event::Event(const std::string &frame_body) : team_a_name(""), team_b_name(""), name(""), time(0), game_updates(), team_a_updates(), team_b_updates(), description("")
{
}

// comparator for set in stomp protocol
bool Event::operator<(const Event& other) const{
    bool thisBeforeHT = this->game_updates.at("before halftime")=="true";
    bool otherBeforeHT = other.game_updates.at("before halftime")=="true";
    if(thisBeforeHT!=otherBeforeHT)
        return thisBeforeHT>otherBeforeHT;
    return this->get_time()<other.get_time();
}

// to string for report
std::string Event::toString() const{
    std::string out = "";
    out.append("team a:" + this->get_team_a_name() + "\n");
    out.append("team b:" + this->get_team_b_name() + "\n");
    out.append("event name:" + this->get_name() + "\n");
    out.append("time:" + std::to_string(this->get_time()) + "\n");
    out.append("general game updates:\n");
    for(auto pair : this->get_game_updates()) {
        out.append("    " + pair.first + ":" + pair.second + "\n");
    }
    out.append("team a updates:\n");
    for(auto pair : this->get_team_a_updates()) {
        out.append("    " + pair.first + ":" + pair.second + "\n");
    }
    out.append("team b updates:\n");
    for(auto pair : this->get_team_b_updates()) {
        out.append("    " + pair.first + ":" + pair.second + "\n");
    }
    out.append("description:\n" + this->get_discription() + "\n");
    return out;
}

names_and_events parseEventsFile(std::string json_path)
{
    std::ifstream f(json_path);
    json data = json::parse(f);

    std::string beforeHT = "true";

    std::string team_a_name = data["team a"];
    std::string team_b_name = data["team b"];

    // run over all the events and convert them to Event objects
    std::vector<Event> events;
    for (auto &event : data["events"])
    {
        std::string name = event["event name"];
        int time = event["time"];
        std::string description = event["description"];
        std::map<std::string, std::string> game_updates;
        std::map<std::string, std::string> team_a_updates;
        std::map<std::string, std::string> team_b_updates;
        for (auto &update : event["general game updates"].items())
        {
            // check of current time compare to halftime
            if(update.key()=="before halftime") {
                if(update.value().is_string())
                    beforeHT = update.value();
                else
                    beforeHT = update.value().dump();
            }
            if (update.value().is_string())
                game_updates[update.key()] = update.value();
            else
                game_updates[update.key()] = update.value().dump();
        }

        //force index before halftime to any event
        if(!game_updates.count("before halftime"))
            game_updates["before halftime"] = beforeHT;

        for (auto &update : event["team a updates"].items())
        {
            if (update.value().is_string())
                team_a_updates[update.key()] = update.value();
            else
                team_a_updates[update.key()] = update.value().dump();
        }

        for (auto &update : event["team b updates"].items())
        {
            if (update.value().is_string())
                team_b_updates[update.key()] = update.value();
            else
                team_b_updates[update.key()] = update.value().dump();
        }
        
        events.push_back(Event(team_a_name, team_b_name, name, time, game_updates, team_a_updates, team_b_updates, description));
    }
    names_and_events events_and_names{team_a_name, team_b_name, events};

    return events_and_names;
}