package bgu.spl.net.impl.stomp;

import java.util.function.Supplier;

import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.Server;
import bgu.spl.net.api.MessageEncoderDecoder; 
import bgu.spl.net.api.MessagingProtocol;


public class StompServer {

    public static void main(String[] args) {
        // TODO: implement this

        if (args.length<2) {
            System.out.println("Usage: java StompServer <port> <frame-delimiter>");
            return;
        }
        int port = Integer.parseInt(args[0]);
        String serverType = args[1];
        
        System.out.println("Starting server " +serverType  + " on port:" + port);
        Supplier<MessagingProtocol<String>> protocolFactory = () -> new ProtocolAdapter(new StompMessagingProtocolImpl());
        Supplier<MessageEncoderDecoder<String>> encoderFactory = () -> new StompMessageEncoderDecoder();

        if (serverType.equals("tpc")) {
            Server.threadPerClient(port,protocolFactory,encoderFactory).serve();
        }
        else if (serverType.equals("reactor")) {
            Server.reactor(
                 Runtime.getRuntime().availableProcessors(),
                 port,protocolFactory,encoderFactory).serve();          
        }
        else {
            System.out.println("Unknown server type: " + serverType);
        }
    }

    //helper

    private static class ProtocolAdapter implements MessagingProtocol<String> {
        private final StompMessagingProtocolImpl impl;

        public ProtocolAdapter(StompMessagingProtocolImpl impl) {
            this.impl = impl;
            }
        
        @Override
        public String process(String message) {
            impl.process(message);
            return null;  
        }

        @Override
        public boolean shouldTerminate() {
            return impl.shouldTerminate();
        }

        public void start(int connectionId, Connections<String> connections) {
            impl.start(connectionId, connections);
        }
    }
}
