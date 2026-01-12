package bgu.spl.net.srv;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;

public class ConnectionsImpl<T> implements Connections<T> {
    //use to send messages to specific clients.
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> activeConnections = new ConcurrentHashMap<>();
    //use this to broadcast messages to a topic.
    private final ConcurrentHashMap<String, ConcurrentLinkedQueue<Integer>> channelSubscribers = new ConcurrentHashMap<>();
    //This is a helper map to efficiently handle UNSUBSCRIBE requests.
    private final ConcurrentHashMap<Integer, ConcurrentHashMap<Integer, String>> clientSubscriptions = new ConcurrentHashMap<>();

    @Override
    public boolean send(int connectionId, T msg) {
        ConnectionHandler<T> handler = activeConnections.get(connectionId);
        if (handler != null) {
            handler.send(msg);
            return true;
        }
        return false;
    }

    @Override
    public void send(String channel, T msg) {
        ConcurrentLinkedQueue<Integer> subscribers = channelSubscribers.get(channel);
        if (subscribers!=null) {    
            for(Integer connectionID:subscribers){
                if (activeConnections.containsKey(connectionID)) {
                    send(connectionID, msg);
                }
            }
        }
    }

    @Override
    public void disconnect(int connectionId) {
        activeConnections.remove(connectionId); //remove active

        ConcurrentHashMap<Integer, String> subscriptions = clientSubscriptions.remove(connectionId);
        if (subscriptions != null) {
            for (String channel : subscriptions.values()) {
                ConcurrentLinkedQueue<Integer> subscribers = channelSubscribers.get(channel);
                if (subscribers!=null) {
                    subscribers.remove(connectionId);
                }
            }
        }
    }

    //helper methods
    public void addConnection(int connectionId, ConnectionHandler<T> handler) {
        activeConnections.put(connectionId, handler);
    }

    //needed???
    public void subscribe(int connectionId, String channel, int subscriptionId) {
        channelSubscribers.computeIfAbsent(channel, k -> new ConcurrentLinkedQueue<>()).add(connectionId);
        clientSubscriptions.computeIfAbsent(connectionId, k -> new ConcurrentHashMap<>()).put(subscriptionId, channel);
    }

    public void unsubscribe(int subscriptionId, int connectionId) {
        ConcurrentHashMap<Integer, String> userSubs = clientSubscriptions.get(connectionId);
        if (userSubs != null) {
            String channel = userSubs.remove(subscriptionId);
            if (channel != null) {
                ConcurrentLinkedQueue<Integer> subscribers = channelSubscribers.get(channel);
                if (subscribers != null) {
                    subscribers.remove(connectionId);
                }
            }
        }
    }
}
