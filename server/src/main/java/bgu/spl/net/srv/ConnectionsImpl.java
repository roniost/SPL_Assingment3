package bgu.spl.net.srv;

import java.util.concurrent.ConcurrentHashMap;

public class ConnectionsImpl<T>implements Connections<T> {
    //use to send messages to specific clients.
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> activeConnections = new ConcurrentHashMap<>();
    //use this to broadcast messages to a topic.
    private final ConcurrentHashMap<String, ConcurrentHashMap<Integer, Integer>> channelSubscribers = new ConcurrentHashMap<>();
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
        ConcurrentHashMap<Integer, Integer> subscribers = channelSubscribers.get(channel);
        if (subscribers!=null) {    
            for(Integer connectionID:subscribers.keySet()){
                ConnectionHandler<T> handler = activeConnections.get(connectionID);
                if (handler != null) {
                    Integer subscriptionId = subscribers.get(connectionID);

                    String originalMsg = (String) msg;
                    String modifiedMsg = originalMsg.replace("subscription:0", "subscription:" + subscriptionId);
                    handler.send((T) modifiedMsg);
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
                ConcurrentHashMap<Integer, Integer> subscribers = channelSubscribers.get(channel);
                if (subscribers!=null) {
                    subscribers.remove(connectionId);
                }
            }
        }
    }

    @Override
    public boolean isSubscribed(int connectionId, String channel) {
        ConcurrentHashMap<Integer, Integer> subscribers = channelSubscribers.get(channel);
        return subscribers != null && subscribers.containsKey(connectionId);
    }

    //helper methods
    public void addConnection(int connectionId, ConnectionHandler<T> handler) { 
        activeConnections.put(connectionId, handler);
    }

    public void subscribe(int connectionId, String channel, int subscriptionId) {

        channelSubscribers.putIfAbsent(channel, new ConcurrentHashMap<>());
        channelSubscribers.get(channel).put(connectionId, subscriptionId);  
                
        clientSubscriptions.putIfAbsent(connectionId, new ConcurrentHashMap<>());
        clientSubscriptions.get(connectionId).put(subscriptionId, channel);  
        
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
