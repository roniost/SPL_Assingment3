package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.MessageEncoderDecoder;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

public class StompMessageEncoderDecoder implements MessageEncoderDecoder<String> {

    //private byte[] bytes = new byte[1024];
    private byte[] bytes = new byte[1 << 10];
    private int len = 0;

    @Override
    public String decodeNextByte(byte nextByte) {
        if (nextByte == '\u0000') {
            String result = new String(bytes, 0, len, StandardCharsets.UTF_8);
            len = 0;
            return result; // return the decoded message
        }
        //push byte to array
        if (len >= bytes.length) { 
            bytes = Arrays.copyOf(bytes, len * 2);
        }
        bytes[len++] = nextByte;
        return null;
    }

    @Override
    public byte[] encode(String message) {
        //return (message + '\u0000').getBytes(StandardCharsets.UTF_8);
        return (message).getBytes(StandardCharsets.UTF_8);
    }
}
