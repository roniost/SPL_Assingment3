#include <thread>
#include <vector>
#include "../include/ConnectionHandler.h"
#include "../include/StompProtocol.h"

void listenPort(ConnectionHandler& connection, StompProtocol& protocol) {
    while(!protocol.getTerminate()) {
        std::string frame;
        if (!connection.getFrameAscii(frame, '\0')) {
            if(protocol.getConnected()) protocol.setTerminate(true);
            protocol.notifyResponse(0, true);
            break;
        }
        if(frame == "\0") continue;
        StompProtocol::Frame framed = protocol.frameToFrame(frame);
        if (framed.type == "ERROR") {
            protocol.setTerminate(true);
            protocol.notifyResponse(std::stoi(framed.frameID), true);
            break;
        }
        else if (framed.type == "RECEIPT") {
            protocol.notifyResponse(std::stoi(framed.frameID));
            if(std::stoi(framed.frameID) == protocol.getLogoutReceiptID()) break;
            continue;
        }
        else if (framed.type == "MESSAGE") {
            protocol.prossesFrame(frame);
            continue;
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
            std::cout << "not connected to server, login first" << std::endl;
            continue;
        }
        int i = args[1].find(':');
        std::string host = args[1].substr(0, i);
        short port = static_cast<short>(std::stoi(args[1].substr(i+1)));
        ConnectionHandler connection(host, port);

        if (!connection.connect()) {
            std::cerr << "Could not connect to server" << std::endl;
            return 1;
        }
        connection.sendFrameAscii(protocol.buildConnectionFrame(host, args[2], args[3]), '\0');
        std::string answer;
        if(connection.getFrameAscii(answer, '\0')) {
            if (answer.find("CONNECTED") != std::string::npos) {
                std::cout << "Login successfu" << std::endl;
                protocol.Login(args[2]);
            }
            else if(answer.find("ERROR") != std::string::npos) {
                std::cout << "Wrong password" << std::endl;
                continue;
            }
        }
        else {
            std::cout << "Could not connect to server" << std::endl;
            continue;
        }

        // got connection approved
        reciver = std::thread(listenPort, std::ref(connection), std::ref(protocol));
        while(!protocol.getTerminate()) {
            std::string command;
            std::getline(std::cin, command);
            std::vector<std::string> args = protocol.splitFrame(command, ' ');
            if(args[0] == "login") {
                std::cout << "The client is already logged in, log out before trying again" << std::endl;
                continue;
            }
            else if(args[0] == "join") {
                int subID = protocol.getNewSubID();
                int reciptID = protocol.getNewReciptID();
                connection.sendFrameAscii(protocol.buildSubscribeFrame(args[1], subID, reciptID), '\0');
                protocol.waitForResponse(reciptID);
                if(protocol.getTerminate()) break;
                protocol.Join(args[1], subID);
                std::cout << "Joined channel " << args[1] << std::endl;
                continue;
            }
            else if (args[0] == "exit") {
                int subID = protocol.gameToSubID(args[1]);
                int reciptID = protocol.getNewReciptID();
                connection.sendFrameAscii(protocol.buildUnsubscribeFrame(subID, reciptID), '\0');
                protocol.waitForResponse(reciptID);
                if(protocol.getTerminate()) break;
                protocol.Exit(subID);
                std::cout << "Exited channel " << args[1] << std::endl;
                continue;
            }
            else if (args[0] == "report") {
                std::vector<std::string> reports = protocol.Report(args[1]);
                for (std::string report : reports) {
                    if (protocol.getTerminate() || !protocol.getConnected()) { //should not happen unless report fails
                        std::cout << "Connection lost or Error received. Stopping report." << std::endl;
                        break; 
                    }
                    if(!connection.sendFrameAscii(report, '\0')) { // only if connection fails
                        std::cout << "Disconnected. Exiting...\n";
                        protocol.setTerminate(true);
                        reciver.join();
                        return 0;
                    }
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
                protocol.setLogoutReceiptID(reciptID);
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
            reciver.join(); //just to make sure
            std::cout << "Terminating...\n";
            return 0;
        }
    }
}