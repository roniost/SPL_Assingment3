import socket
import time

def send_frame(sock, frame):
    sock.sendall(frame.encode() + b'\x00')

def recv_frame(sock, name, timeout=2):
    sock.settimeout(timeout)
    try:
        data = b""
        while True:
            chunk = sock.recv(1024)
            if not chunk: break
            data += chunk
            if b'\x00' in data: break
        response = data.decode().replace('\x00', '')
        print(f"[{name}] 📥 RECEIVED:\n{response}")
        return response
    except:
        print(f"[{name}] ⏳ Timeout (No response)")
        return None

def run_suite(mode_name):
    print(f"\n--- STARTING {mode_name} FULL LOGIC TEST ---")
    host, port = "127.0.0.1", 7777
    
    try:
        # --- TEST 0: WRONG PASSWORD LOGIC ---
        print("\n🕵️ TEST 0: Wrong Password Logic")
        
        # Step A: Register 'badguy' with password '123'
        tmp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        tmp.connect((host, port))
        send_frame(tmp, "CONNECT\naccept-version:1.2\nhost:stomp\nlogin:badguy\npasscode:123\n\n")
        resp = recv_frame(tmp, "Setup-Reg")
        if "CONNECTED" not in resp:
            print("❌ Failed to register user for password test")
            return
        
        # Step B: Disconnect him
        send_frame(tmp, "DISCONNECT\nreceipt:999\n\n")
        recv_frame(tmp, "Setup-Disc")
        tmp.close()

        # Step C: Try to login 'badguy' with '999' (Should Fail)
        print("   -> Attempting login with WRONG password...")
        fail_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        fail_sock.connect((host, port))
        send_frame(fail_sock, "CONNECT\naccept-version:1.2\nhost:stomp\nlogin:badguy\npasscode:999\n\n")
        resp = recv_frame(fail_sock, "Expect-Error")
        
        if "ERROR" in resp and "Password" in resp:
             print("✅ SUCCESS: Server rejected wrong password correctly.")
        else:
             print("❌ FAILURE: Server accepted wrong password or gave wrong error!")
             # return # נמשיך בכל זאת לשאר הבדיקות
        
        fail_sock.close()


        # --- MAIN FLOW (Alice & Bob) ---

        # 1. Connect Alice
        alice = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        alice.connect((host, port))
        print("\n👩 Alice: Connecting...")
        send_frame(alice, "CONNECT\naccept-version:1.2\nhost:stomp.cs.bgu.ac.il\nlogin:alice\npasscode:123\n\n")
        recv_frame(alice, "Alice")

        # 2. Alice Subscribes
        print("\n👩 Alice: Subscribing to /topic/games...")
        send_frame(alice, "SUBSCRIBE\ndestination:/topic/games\nid:1\nreceipt:77\n\n")
        recv_frame(alice, "Alice") # Expecting RECEIPT 77

        # 3. Connect Bob
        bob = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        bob.connect((host, port))
        print("\n👨 Bob: Connecting...")
        send_frame(bob, "CONNECT\naccept-version:1.2\nhost:stomp.cs.bgu.ac.il\nlogin:bob\npasscode:456\n\n")
        recv_frame(bob, "Bob")

        # 4. Bob Subscribes (Required to SEND in your protocol)
        print("\n👨 Bob: Subscribing to /topic/games...")
        send_frame(bob, "SUBSCRIBE\ndestination:/topic/games\nid:2\n\n")
        
        # 5. Bob Sends Message
        print("\n👨 Bob: Sending 'Goal for Brazil!'...")
        send_frame(bob, "SEND\ndestination:/topic/games\n\nGoal for Brazil!")
        
        # 6. Alice checks inbox
        print("\n👀 Checking Alice's inbox for Bob's message...")
        recv_frame(alice, "Alice") # Expecting MESSAGE frame

        # 7. Alice Unsubscribes
        print("\n👩 Alice: Unsubscribing...")
        send_frame(alice, "UNSUBSCRIBE\nid:1\nreceipt:101\n\n")
        recv_frame(alice, "Alice") # Expecting RECEIPT 101

        # 8. Bob sends another (Alice should NOT get this)
        print("\n👨 Bob: Sending 'Goal for Germany!' (Alice should ignore)...")
        send_frame(bob, "SEND\ndestination:/topic/games\n\nGoal for Germany!")
        time.sleep(0.5)
        recv_frame(alice, "Alice") # Expecting Timeout

        alice.close()
        bob.close()
    except Exception as e:
        print(f"❌ Error: {e}")
    print(f"\n--- {mode_name} TESTS FINISHED ---")

if __name__ == "__main__":
    print("Step 1: Start '7777 tpc' and press Enter.")
    input()
    run_suite("THREAD-PER-CLIENT")
    
    print("\n" + "="*50)
    print("Step 2: Start '7777 reactor' and press Enter.")
    input()
    run_suite("REACTOR")

'''import socket
import time

def send_frame(sock, frame):
    sock.sendall(frame.encode() + b'\x00')

def recv_frame(sock, name, timeout=2):
    sock.settimeout(timeout)
    try:
        data = b""
        while True:
            chunk = sock.recv(1024)
            if not chunk: break
            data += chunk
            if b'\x00' in data: break
        response = data.decode().replace('\x00', '')
        print(f"[{name}] 📥 RECEIVED:\n{response}")
        return response
    except:
        print(f"[{name}] ⏳ Timeout (No response)")
        return None

def run_suite(mode_name):
    print(f"\n--- STARTING {mode_name} FULL LOGIC TEST ---")
    host, port = "127.0.0.1", 7777
    
    try:
        # 1. Connect Alice
        alice = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        alice.connect((host, port))
        print("\n👩 Alice: Connecting...")
        send_frame(alice, "CONNECT\naccept-version:1.2\nhost:stomp.cs.bgu.ac.il\nlogin:alice\npasscode:123\n\n")
        recv_frame(alice, "Alice")

        # 2. Alice Subscribes
        print("\n👩 Alice: Subscribing to /topic/games...")
        send_frame(alice, "SUBSCRIBE\ndestination:/topic/games\nid:1\nreceipt:77\n\n")
        recv_frame(alice, "Alice") # Expecting RECEIPT 77

        # 3. Connect Bob
        bob = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        bob.connect((host, port))
        print("\n👨 Bob: Connecting...")
        send_frame(bob, "CONNECT\naccept-version:1.2\nhost:stomp.cs.bgu.ac.il\nlogin:bob\npasscode:456\n\n")
        recv_frame(bob, "Bob")

        # 4. Bob Subscribes (Required to SEND in your protocol)
        print("\n👨 Bob: Subscribing to /topic/games...")
        send_frame(bob, "SUBSCRIBE\ndestination:/topic/games\nid:2\n\n")
        
        # 5. Bob Sends Message
        print("\n👨 Bob: Sending 'Goal for Brazil!'...")
        send_frame(bob, "SEND\ndestination:/topic/games\n\nGoal for Brazil!")
        
        # 6. Alice checks inbox
        print("\n👀 Checking Alice's inbox for Bob's message...")
        recv_frame(alice, "Alice") # Expecting MESSAGE frame

        # 7. Alice Unsubscribes
        print("\n👩 Alice: Unsubscribing...")
        send_frame(alice, "UNSUBSCRIBE\nid:1\nreceipt:101\n\n")
        recv_frame(alice, "Alice") # Expecting RECEIPT 101

        # 8. Bob sends another (Alice should NOT get this)
        print("\n👨 Bob: Sending 'Goal for Germany!' (Alice should ignore)...")
        send_frame(bob, "SEND\ndestination:/topic/games\n\nGoal for Germany!")
        time.sleep(0.5)
        recv_frame(alice, "Alice") # Expecting Timeout

        alice.close()
        bob.close()
    except Exception as e:
        print(f"❌ Error: {e}")
    print(f"\n--- {mode_name} TESTS FINISHED ---")

if __name__ == "__main__":
    print("Step 1: Start '7777 tpc' and press Enter.")
    input()
    run_suite("THREAD-PER-CLIENT")
    
    print("\n" + "="*50)
    print("Step 2: Start '7777 reactor' and press Enter.")
    input()
    run_suite("REACTOR")'''