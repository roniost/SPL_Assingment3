#include <stdlib.h>
#include <iostream>
#include <sstream>
#include "../include/ConnectionHandler.h"
#include "../include/StompProtocol.h"

void listen(ConnectionHandler& connection, StompProtocol& protocol) {
    while(!protocol.getTerminate()) {
        std::string frame;
        if (!connection.getFrameAscii(frame, '\0')) {
            if(protocol.getConnected()) protocol.setTerminate(true);
            protocol.notifyResponse(0, true);
            break;
        }
        StompProtocol::Frame framed = protocol.frameToFrame(frame);
        if (framed.type == "ERROR") {
            protocol.setTerminate(true);
            protocol.notifyResponse(std::stoi(framed.frameID), true);
            break;
        }
        else if (framed.type == "RECIPT") {
            protocol.notifyResponse(std::stoi(framed.frameID));
            continue;
        }
        else if (framed.type == "MESSAGE") {
            protocol.prossesFrame(frame);
        }
    }
}

int main(int argc, char *argv[]) {
	// TODO: implement the STOMP client
	
    StompProtocol protocol;
    std::thread reciver;
    
    while(1) {
        std::string command;
        std::getline(std::cin, command);
        std::vector<std::string> args = protocol.splitFrame(command, ' ');
        if(args[0]!="login") {
            std::cout << "not connected to server" << std::endl;
            continue;
        }
        int i = args[1].find(':');
        std::string host = args[1].substr(0, i);
        short port = static_cast<short>(std::stoi(args[1].substr(i+1)));
        ConnectionHandler connection(host, port);

        if (!connection.connect()) {
            std::cerr << "Cannot connect to " << host << ":" << port << std::endl;
            return 1;
        }
        connection.sendFrameAscii(protocol.buildConnectionFrame(host, args[2], args[3]), '\0');
        std::string answer;
        if(connection.getLine(answer)) {
            if(answer == "ERROR") {
                std::cout << "wrong password\n";
                continue;
            }
        }
        else {
            std::cout << "couldn't connect. Exiting...\n";
            return 0;
        }

        // got connection approved
        while(!protocol.getTerminate()) {
            std::string command;
            std::getline(std::cin, command);
            std::vector<std::string> args = protocol.splitFrame(command, ' ');
            if(args[0] == "login") {
                std::cout << "already logged in\n";
                continue;
            }
            else if(args[0] == "join") {
                int subID = protocol.getNewSubID();
                int reciptID = protocol.getNewReciptID();
                connection.sendFrameAscii(protocol.buildSubscribeFrame(args[1], subID, reciptID), '\0');
                protocol.waitForResponse(reciptID);
                if(protocol.getTerminate())
                    break;
                protocol.Join(args[1], subID);
                continue;
            }
            else if (args[0] == "exit") {
                int subID = protocol.gameToSubID(args[1]);
                int reciptID = protocol.getNewReciptID();
                connection.sendFrameAscii(protocol.buildUnsubscribeFrame(subID, reciptID), '\0');
                protocol.waitForResponse(reciptID);
                if(protocol.getTerminate())
                    break;
                protocol.Exit(subID);
                continue;
            }
            else if (args[0] == "report") {
                std::string report = protocol.Report(args[1]);
                if(!(report == "") || !connection.sendFrameAscii(report, '\0')) {
                    std::cout << "Disconnected. Exiting...\n";
                    protocol.setTerminate(true);
                    reciver.join();
                    return 0;
                }
                if(protocol.getTerminate()) break;
                continue;
            }
            else if (args[0] == "summary") {
                protocol.Summery(args[1], args[2], args[3]);
                continue;
            }
            else if (args[0] == "logout") {
                int reciptID = protocol.getNewReciptID();
                connection.sendFrameAscii(protocol.buildDisconnectFrame(reciptID), '\0');
                protocol.waitForResponse(reciptID);
                if(protocol.getTerminate()) break;
                protocol.Logout();
                connection.close();
                reciver.join();
                break;
            }
        }
        if(protocol.getTerminate()) {
            connection.close();
            protocol.Logout();
            std::cout << "Terminating...\n";
            return 0;
        }
    }
}