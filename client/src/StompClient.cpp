#include <stdlib.h>
#include <thread>
#include <functional>
#include <iostream>
#include <sstream>
#include "../include/ConnectionHandler.h"
#include "../include/StompProtocol.h"

void listenPort(ConnectionHandler& connection, StompProtocol& protocol) {
    std::cout << "[DEBUG] Listener thread started." << std::endl;
    while(!protocol.getTerminate()) {
        std::string frame;
        if (!connection.getFrameAscii(frame, '\0')) {
            std::cout << "[DEBUG] Listener: Connection closed or getFrameAscii failed." << std::endl;
            if(protocol.getConnected()) protocol.setTerminate(true);
            protocol.notifyResponse(0, true);
            break;
        }
        if(frame == "\0") continue;
        std::cout << "[DEBUG] Listener received frame:\n" << frame << "\n----------------" << std::endl;
        StompProtocol::Frame framed = protocol.frameToFrame(frame);
        std::cout << "[DEBUG] Listener processing frame type: " << framed.type << std::endl;
        if (framed.type == "ERROR") {
            std::cout << "[DEBUG] Listener: ERROR frame received. Terminating." << std::endl;
            protocol.setTerminate(true);
            protocol.notifyResponse(std::stoi(framed.frameID), true);
            break;
        }
        else if (framed.type == "RECIPT") {
            std::cout << "[DEBUG] Listener: RECIPT frame received. ID: " << framed.frameID << std::endl;
            protocol.notifyResponse(std::stoi(framed.frameID));
            continue;
        }
        else if (framed.type == "MESSAGE") {
            std::cout << "[DEBUG] Listener: MESSAGE frame received. Processing..." << std::endl;
            protocol.prossesFrame(frame);
        }
        std::cout << "[DEBUG] Listener thread exiting." << std::endl;
    }
}

int main(int argc, char *argv[]) {
	// TODO: implement the STOMP client
	
    StompProtocol protocol;
    std::thread reciver;

    while(1) {
        std::string command;
        std::getline(std::cin, command);
        std::cout << "[DEBUG] Input received: " << command << std::endl;
        std::vector<std::string> args = protocol.splitFrame(command, ' ');
        if(args[0]!="login") {
            std::cout << "not connected to server" << std::endl;
            continue;
        }
        int i = args[1].find(':');
        std::string host = args[1].substr(0, i);
        short port = static_cast<short>(std::stoi(args[1].substr(i+1)));
        
        std::cout << "[DEBUG] Connecting to " << host << ":" << port << std::endl;
        ConnectionHandler connection(host, port);

        if (!connection.connect()) {
            std::cerr << "Cannot connect to " << host << ":" << port << std::endl;
            return 1;
        }
        std::cout << "[DEBUG] Connected. Sending CONNECT frame..." << std::endl;
        connection.sendFrameAscii(protocol.buildConnectionFrame(host, args[2], args[3]), '\0');
        std::string answer;
        if(connection.getFrameAscii(answer, '\0')) {
            std::cout << "[DEBUG] Server response during login: " << answer << std::endl;
            if (answer.find("CONNECTED") != std::string::npos) {
                std::cout << "[DEBUG] login successful" << std::endl;
            }
            else if(answer.find("ERROR") != std::string::npos) {
                std::cout << "wrong password\n";
                continue;
            }
        }
        else {
            std::cout << "couldn't connect. Exiting...\n";
            return 0;
        }

        // got connection approved
        std::cout << "[DEBUG] Login approved. Starting listener thread..." << std::endl;
        reciver = std::thread(listenPort, std::ref(connection), std::ref(protocol));
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
                std::cout << "[DEBUG] Joining " << args[1] << " (SubID: " << subID << ", ReciptID: " << reciptID << ")" << std::endl;
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
                std::cout << "[DEBUG] Exiting " << args[1] << " (SubID: " << subID << ", ReciptID: " << reciptID << ")" << std::endl;
                connection.sendFrameAscii(protocol.buildUnsubscribeFrame(subID, reciptID), '\0');
                protocol.waitForResponse(reciptID);
                if(protocol.getTerminate())
                    break;
                protocol.Exit(subID);
                continue;
            }
            else if (args[0] == "report") {
                std::cout << "[DEBUG] Generating report from file: " << args[1] << std::endl;
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
                std::cout << "[DEBUG] Requesting summary..." << std::endl;
                protocol.Summery(args[1], args[2], args[3]);
                continue;
            }
            else if (args[0] == "logout") {
                int reciptID = protocol.getNewReciptID();
                std::cout << "[DEBUG] Logging out. ReciptID: " << reciptID << std::endl;
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
            std::cout << "[DEBUG] Terminate flag set. Cleaning up..." << std::endl;
            connection.close();
            protocol.Logout();
            std::cout << "Terminating...\n";
            return 0;
        }
    }
}