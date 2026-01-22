package bgu.spl.net.impl.stomp;

import java.util.function.Supplier;

import bgu.spl.net.srv.Server;


public class StompServer {
    
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static void main(String[] args) {
        // TODO: implement this

        if (args.length<2) {
            System.out.println("Usage: java StompServer <port> <frame-delimiter>");
            return;
        }
        int port = Integer.parseInt(args[0]);
        String serverType = args[1];
        
        if (serverType.equals("tpc")) {
            System.out.println("Starting STOMP TPC server on port: " + port);
            Server.threadPerClient(
                port,
                (Supplier) StompMessagingProtocolImpl::new,
                StompMessageEncoderDecoder::new).serve();
        }
        else if (serverType.equals("reactor")) {
            System.out.println("Starting STOMP Reactor server on port: " + port);
            Server.reactor(
                Runtime.getRuntime().availableProcessors(),
                port,
                (Supplier) StompMessagingProtocolImpl::new,
                StompMessageEncoderDecoder::new).serve();
        }
        else {
            System.out.println("Unknown server type: " + serverType);
        }
    }
}
