package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionsImpl;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;


public class StompMessagingProtocolImpl implements StompMessagingProtocol<String> {
    
    private int connectionID;
    private ConnectionsImpl<String> connections;
    private boolean shouldTerminate = false;

    private static final ConcurrentHashMap<String, String> userCredentials = new ConcurrentHashMap<>();
    private static final ConcurrentHashMap<String, Boolean> activeUsers = new ConcurrentHashMap<>();

    private String currentUser = null; //not logged in --------------------------------

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionID = connectionId;
        this.connections = (ConnectionsImpl<String>) connections;
    }

    @Override
    public String process(String message) {
        System.out.println("DEBUG PROTOCOL: Processing message:\n" + message); 
        Frame frame = parseFrame(message);
        if (frame==null) {
            sendError("Malformed frame", "Could not parse message");
            return null;
        }

        try{
            switch (frame.command) {
                case "CONNECT":
                    handleConnect(frame);
                    break;
                case "SUBSCRIBE":
                    handleSubscribe(frame);
                    break;
                case "UNSUBSCRIBE":
                    handleUnsubscribe(frame);
                    break;
                case "SEND":
                    handleSend(frame); 
                    break;
                case "DISCONNECT":
                    handleDisconnect(frame);
                    break;
                default:
                    sendError("Unknown command", "The command " + frame.command + " is not recognized");
                    break;
            }
        }catch (Exception e) {
            sendError("Processing Error", e.getMessage());
        }
        return null;
    }

    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }

    // --- Command Handlers ---
    private void handleConnect(Frame frame) {
        String login = frame.headers.get("login");
        String passcode = frame.headers.get("passcode");

        if (login == null || passcode == null) {
            sendError("Missing headers", "CONNECT frame must include login, passcode");
            shouldTerminate = true;
            return;
        }
        
        //boolean successfulLogin = connections.addConnection(connectionID, login);

        if(activeUsers.containsKey(login) && activeUsers.get(login)) {
            sendError("User already logged in", "User " + login + " is already logged in");
            return;
        }
        
        if (userCredentials.containsKey(login)) {
            if (!userCredentials.get(login).equals(passcode)) {
                sendError("Wrong password", "Password does not match");
                return;
            }
        } else {
            userCredentials.put(login, passcode);
        }
        
        activeUsers.put(login, true);
        currentUser = login;

        String response = "CONNECTED\n" +
                          "version:1.2\n" +
                          "\n" + 
                          "\u0000";
        connections.send(connectionID, response);
    }

    private void handleSubscribe(Frame frame) {
        verifyLoggedIn();
        String destination = frame.headers.get("destination");
        String id = frame.headers.get("id");

        if (destination==null || id==null) {
            sendError("Missing headers", "SUBSCRIBE frame must include destination and id");
            return;
        }
        int subscriptionId= Integer.parseInt(id);
        connections.subscribe(connectionID, destination, subscriptionId);

        handleReceipt(frame);
    }

    private void handleUnsubscribe(Frame frame) {
        verifyLoggedIn();
        String id = frame.headers.get("id");

        if (id==null) {
            sendError("Missing headers", "UNSUBSCRIBE frame must include id");
            return;
        }
        int subscriptionId= Integer.parseInt(id);
        connections.unsubscribe(subscriptionId, connectionID);

        handleReceipt(frame);
    }

    private void handleSend(Frame frame) {
        verifyLoggedIn();
        String destination = frame.headers.get("destination");

        if (destination==null) {
            sendError("Missing headers", "SEND frame must include destination");
            return;
        }

        String body = frame.body;
        String messageFrame = "MESSAGE\n" +
                              "destination:" + destination + "\n" +
                              "message-id:" + java.util.UUID.randomUUID().toString() + "\n" +
                              "subscription:0\n" +
                              // "subscription:???" -> This is the hard part with generic broadcast
                              "\n" + 
                              body + "\u0000";
        
        connections.send(destination, messageFrame);
        handleReceipt(frame);
    }
    
    private void handleDisconnect(Frame frame) {
        verifyLoggedIn();
        handleReceipt(frame);
        activeUsers.remove(currentUser);
        shouldTerminate = true;
        connections.disconnect(connectionID);
    } 

    // --- Helper Methods ---
    private void handleReceipt(Frame frame) {
        String receiptId = frame.headers.get("receipt");
        if (receiptId != null) {
            String response = "RECEIPT\n" +
                                "receipt-id:" + receiptId + "\n" + 
                                "\n"
                                + "\u0000";
            connections.send(connectionID,   response);
        }
    }

    private void sendError(String message, String details) {
        String errorFrame = "ERROR\n" +
                            "message:" + message + "\n" +
                            "\n" +
                            details + "\n" + "\u0000";
        connections.send(connectionID, errorFrame);
        shouldTerminate = true;
        connections.disconnect(connectionID);
        if (currentUser != null) {
            activeUsers.remove(currentUser);
        }
    }
    
    private void verifyLoggedIn() {
        if (currentUser == null) {
            throw new IllegalStateException("User not logged in");
        }
    }

    private Frame parseFrame(String message) {
        if (message==null || message.trim().isEmpty()) {
            return null;
        }
        String[] parts=message.split("\n", 2);
        String command=parts[0].trim();
        if (parts.length<2) {
            return new Frame(command, new HashMap<>(), "");
        }

        String rest=parts[1];
        Map<String, String> headers=new HashMap<>();
        String body="";

        int emptyLineIndex = rest.indexOf("\n\n");
        String headersSection;

        if (emptyLineIndex!=-1) {
            headersSection = rest.substring(0, emptyLineIndex);
            body = rest.substring(emptyLineIndex + 2);
        } else {
            headersSection = rest;
        }

        String[] headerLines = headersSection.split("\n");
        for (String line : headerLines) {
            int colonIndex = line.indexOf(':');
            if (colonIndex != -1) {
                String key = line.substring(0, colonIndex).trim();
                String value = line.substring(colonIndex + 1).trim();
                headers.put(key, value);
            }
        }
        return new Frame(command, headers, body);
    }
    // --- Frame Class ---

    public class Frame {
        String command;
        Map<String, String> headers;
        String body;

        Frame(String command, Map<String, String> headers, String body) {
            this.command = command;
            this.headers = headers;
            this.body = body;
        }
    }
}
