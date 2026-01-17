package bgu.spl.net.srv;

import bgu.spl.net.api.MessageEncoderDecoder;
import bgu.spl.net.api.MessagingProtocol;
import bgu.spl.net.api.StompMessagingProtocol; // ADDED
import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.function.Supplier;

public abstract class BaseServer<T> implements Server<T> {

    private final int port;
    private final Supplier<MessagingProtocol<T>> protocolFactory;
    private final Supplier<MessageEncoderDecoder<T>> encdecFactory;
    private ServerSocket sock;

    private final ConnectionsImpl<T> connections = new ConnectionsImpl<>(); // ADDED
    private int connectionIdCounter = 0; // ADDED

    public BaseServer(
            int port,
            Supplier<MessagingProtocol<T>> protocolFactory,
            Supplier<MessageEncoderDecoder<T>> encdecFactory) {

        this.port = port;
        this.protocolFactory = protocolFactory;
        this.encdecFactory = encdecFactory;
		this.sock = null;
    }

    @Override
    public void serve() {

        try (ServerSocket serverSock = new ServerSocket(port)) {
			System.out.println("Server started");

            this.sock = serverSock; //just to be able to close

            while (!Thread.currentThread().isInterrupted()) {

                Socket clientSock = serverSock.accept();
                System.out.println("DEBUG 1: Client accepted via TCP");

                MessagingProtocol<T> protocol = protocolFactory.get();// ADDED
                if (protocol instanceof StompMessagingProtocol) {
                    StompMessagingProtocol<T> stompProtocol = (StompMessagingProtocol<T>) protocol;
                    int connectionId = connectionIdCounter; // ADDED
                    connectionIdCounter++; // ADDED
                    stompProtocol.start(connectionId, connections); // ADDED
                    
                }
                //protocol.start(connectionIdCounter, connections); // ADDED
                //StompMessagingProtocol<T> stompProtocol = (StompMessagingProtocol<T>) protocol; // ADDED

                MessagingProtocol<T> adapter = new MessagingProtocol<T>() {
                    @Override
                    public T process(T message) {
                        protocol.process(message);
                        return null;
                    }

                    public boolean shouldTerminate() {
                        return protocol.shouldTerminate();
                    }
                };
                
                BlockingConnectionHandler<T> handler = new BlockingConnectionHandler<T>(
                        clientSock,
                        encdecFactory.get(),
                        adapter); // CHANGED
                System.out.println("DEBUG 2: Handler created");

                int connectionId = connectionIdCounter++; // ADDED
                connections.addConnection(connectionId, handler); // ADDED
                System.out.println("DEBUG 3: Before protocol start");
                //stompProtocol.start(connectionId, connections); // ADDED
                System.out.println("DEBUG 4: After protocol start");

                execute(handler);
                System.out.println("DEBUG 5: Execute called");
            }
        } catch (IOException ex) {
            ex.printStackTrace();
        }

        System.out.println("server closed!!!");
    }

    @Override
    public void close() throws IOException {
		if (sock != null)
			sock.close();
    }

    protected abstract void execute(BlockingConnectionHandler<T>  handler);

}
