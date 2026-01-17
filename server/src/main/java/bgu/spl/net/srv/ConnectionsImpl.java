package bgu.spl.net.srv;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;

public class ConnectionsImpl<T>implements Connections<T> {
    //use to send messages to specific clients.
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> activeConnections = new ConcurrentHashMap<>();
    //use this to broadcast messages to a topic.
    private final ConcurrentHashMap<String, ConcurrentHashMap<Integer, Integer>> channelSubscribers = new ConcurrentHashMap<>();
    //This is a helper map to efficiently handle UNSUBSCRIBE requests.
    private final ConcurrentHashMap<Integer, ConcurrentHashMap<Integer, String>> clientSubscriptions = new ConcurrentHashMap<>();

    @Override
    public boolean send(int connectionId, T msg) {
        System.out.println("DEBUG CONNECTIONS: Attempting to send to ID: " + connectionId);
        ConnectionHandler<T> handler = activeConnections.get(connectionId);
        if (handler != null) {
            System.out.println("DEBUG CONNECTIONS: Found handler for ID: " + connectionId + ", sending message.");
            handler.send(msg);
            return true;
        }
        System.out.println("DEBUG CONNECTIONS: No handler found for ID: " + connectionId + ", message not sent.");
        return false;
    }

    @Override
    public void send(String channel, T msg) {
        ConcurrentHashMap<Integer, Integer> subscribers = channelSubscribers.get(channel);
        if (subscribers!=null) {    
            for(Integer connectionID:subscribers.keySet()){
                ConnectionHandler<T> handler = activeConnections.get(connectionID);
                if (handler != null) {
                    Integer subscriptionId = subscribers.get(connectionID);

                    String originalMsg = (String) msg;
                    String modifiedMsg = originalMsg.replace("subscription:0", "subscription:" + subscriptionId);
                    handler.send((T) modifiedMsg);
                    //send( (int)connectionID, (T) modifiedMsg); //why vs code is stupid
                }
            }
        }
    }

    @Override
    public void disconnect(int connectionId) {
        System.out.println("DEBUG CONNECTIONS: Disconnecting connection ID: " + connectionId);
        activeConnections.remove(connectionId); //remove active

        ConcurrentHashMap<Integer, String> subscriptions = clientSubscriptions.remove(connectionId);
        if (subscriptions != null) {
            for (String channel : subscriptions.values()) {
                ConcurrentHashMap<Integer, Integer> subscribers = channelSubscribers.get(channel);
                if (subscribers!=null) {
                    subscribers.remove(connectionId);
                }
            }
        }
    }

    //helper methods
    public void addConnection(int connectionId, ConnectionHandler<T> handler) {
        System.out.println("DEBUG CONNECTIONS: Adding connection ID: " + connectionId);
        activeConnections.put(connectionId, handler);
    }

    //needed???
    public void subscribe(int connectionId, String channel, int subscriptionId) {
        channelSubscribers.computeIfAbsent(channel, k -> new ConcurrentHashMap<>()).put(connectionId, subscriptionId);
        clientSubscriptions.computeIfAbsent(connectionId, k -> new ConcurrentHashMap<>()).put(subscriptionId, channel);
        
        /*channelSubscribers.putIfAbsent(channel, new ConcurrentHashMap<>());
        channelSubscribers.get(channel).put(connectionId, subscriptionId);  
        
        clientSubscriptions.putIfAbsent(connectionId, new ConcurrentHashMap<>());
        clientSubscriptions.get(connectionId).put(subscriptionId, channel);  
        */  
    }

    public void unsubscribe(int subscriptionId, int connectionId) {
        ConcurrentHashMap<Integer, String> userSubs = clientSubscriptions.get(connectionId);
        if (userSubs != null) {
            String channel = userSubs.remove(subscriptionId);
            if (channel != null) {
                ConcurrentHashMap<Integer, Integer> subscribers = channelSubscribers.get(channel);
                if (subscribers != null) {
                    subscribers.remove(connectionId);
                }
            }
        }
    }
}
