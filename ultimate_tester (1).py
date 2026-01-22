import socket
import time
import threading
import sys

# ================= CONFIGURATION =================
HOST = '127.0.0.1'
PORT = 7777
# =================================================

class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'

def print_pass(msg):
    print(f"{Colors.OKGREEN}[PASS]{Colors.ENDC} {msg}")

def print_fail(msg):
    print(f"{Colors.FAIL}[FAIL]{Colors.ENDC} {msg}")

def print_info(msg):
    print(f"{Colors.OKCYAN}[INFO]{Colors.ENDC} {msg}")

def send_frame(sock, frame_str):
    """Sends a frame ensuring the null byte is appended."""
    try:
        sock.sendall(frame_str.encode('utf-8') + b'\x00')
    except Exception as e:
        print_fail(f"Socket error during send: {e}")

def recv_frame(sock, timeout=3):
    """Receives a frame, handling buffering until null byte."""
    sock.settimeout(timeout)
    try:
        data = b""
        while True:
            chunk = sock.recv(1024)
            if not chunk: break
            data += chunk
            if b'\x00' in data: break
        
        decoded = data.decode('utf-8').replace('\x00', '')
        return decoded
    except socket.timeout:
        return "TIMEOUT"
    except Exception as e:
        return f"ERROR: {e}"

def connect_client(login, passcode):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))
    frame = f"CONNECT\naccept-version:1.2\nhost:stomp.cs.bgu.ac.il\nlogin:{login}\npasscode:{passcode}\n\n"
    send_frame(s, frame)
    response = recv_frame(s)
    return s, response

# ================= TEST SUITES =================

def test_1_login_scenarios():
    print_info("--- Test 1: Login Scenarios ---")
    
    # 1.1 New User Registration (Success)
    user = f"user_{int(time.time())}"
    pw = "pass123"
    s1, resp = connect_client(user, pw)
    
    if "CONNECTED" in resp:
        print_pass("New user registration successful")
    else:
        print_fail(f"New user registration failed. Got: {resp}")
        return

    # 1.2 Double Login (Fail)
    print_info("Attempting double login with same user...")
    s2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s2.connect((HOST, PORT))
    frame = f"CONNECT\naccept-version:1.2\nhost:stomp.cs.bgu.ac.il\nlogin:{user}\npasscode:{pw}\n\n"
    send_frame(s2, frame)
    resp2 = recv_frame(s2)
    
    if "ERROR" in resp2 and "User already logged in" in resp2:
        print_pass("Double login correctly rejected")
    else:
        print_fail(f"Double login should fail. Got: {resp2}")
    s2.close()

    # 1.3 Logout
    print_info("Logging out first user...")
    send_frame(s1, "DISCONNECT\nreceipt:77\n\n")
    resp_disc = recv_frame(s1)
    if "RECEIPT" in resp_disc and "77" in resp_disc:
        print_pass("Graceful disconnect successful")
    else:
        print_fail("Disconnect receipt missing")
    s1.close()

    # 1.4 Wrong Password (Fail)
    # This PROVES database persistence: The user must exist from step 1.1 for the password check to happen.
    print_info("Attempting login with WRONG password...")
    s3, resp3 = connect_client(user, "wrong_pass")
    
    if "ERROR" in resp3 and "wrong password" in resp3:
        print_pass("Wrong password correctly rejected (DB persistence confirmed!)")
    else:
        print_fail(f"Wrong password should fail. Got: {resp3}")
    s3.close()

def test_2_pub_sub_logic():
    print_info("\n--- Test 2: Pub/Sub & Broadcast Logic ---")
    
    # Setup: Alice and Bob
    alice_name = "Alice_The_Spy"
    bob_name = "Bob_The_Builder"
    common_pass = "123"
    
    alice, r_a = connect_client(alice_name, common_pass)
    bob, r_b = connect_client(bob_name, common_pass)
    
    if "CONNECTED" not in r_a or "CONNECTED" not in r_b:
        print_fail("Setup failed: Could not connect Alice or Bob")
        return

    topic = "/topic/secret_mission"
    
    # 2.1 Alice Subscribes
    print_info("Alice subscribing...")
    send_frame(alice, f"SUBSCRIBE\ndestination:{topic}\nid:100\nreceipt:99\n\n")
    r_sub = recv_frame(alice)
    if "RECEIPT" in r_sub:
        print_pass("Alice subscribed successfully")
    else:
        print_fail("Alice failed to subscribe")

    # 2.2 Bob sends message (Alice should get it)
    # Note: Bob must be subscribed to send? Req says: "if a client is not subscribed to a topic it is not allowed to send"
    print_info("Bob attempting to send WITHOUT subscribing (Should Fail)...")
    send_frame(bob, f"SEND\ndestination:{topic}\n\nI am not subscribed!")
    r_fail = recv_frame(bob)
    if "ERROR" in r_fail:
        print_pass("Server correctly rejected Send-Without-Sub")
        bob.close() # Bob got disconnected by error
        bob, _ = connect_client(bob_name, common_pass) # Reconnect Bob
    else:
        print_fail("Server allowed sending without subscription! (Or silent failure)")

    # 2.3 Bob Subscribes and Sends
    print_info("Bob subscribing and sending...")
    send_frame(bob, f"SUBSCRIBE\ndestination:{topic}\nid:200\n\n")
    # Small delay to ensure sub processed
    time.sleep(0.2)
    
    msg_body = "We build it!"
    send_frame(bob, f"SEND\ndestination:{topic}\n\n{msg_body}")
    
    # Check Alice
    r_alice = recv_frame(alice)
    if "MESSAGE" in r_alice and msg_body in r_alice and "subscription:100" in r_alice:
        print_pass("Alice received Bob's message with correct subscription ID")
    else:
        print_fail(f"Alice did not receive message properly. Got: {r_alice}")

    # 2.4 Unsubscribe logic
    print_info("Alice unsubscribing...")
    send_frame(alice, "UNSUBSCRIBE\nid:100\nreceipt:101\n\n")
    recv_frame(alice) # clear receipt
    
    print_info("Bob sending again (Alice should NOT get this)...")
    send_frame(bob, f"SEND\ndestination:{topic}\n\nAre you there?")
    
    r_alice_late = recv_frame(alice, timeout=1)
    if r_alice_late == "TIMEOUT":
        print_pass("Alice correctly did not receive message after unsubscribe")
    else:
        print_fail(f"Alice received message after unsubscribing! Got: {r_alice_late}")

    alice.close()
    bob.close()

def run_tests():
    print(f"{Colors.BOLD}Starting Ultimate SPL Assignment 3 Tester{Colors.ENDC}")
    print(f"Target: {HOST}:{PORT}")
    
    try:
        # Simple connectivity check
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((HOST, PORT))
        s.close()
    except:
        print_fail(f"Cannot connect to server at {HOST}:{PORT}. Is it running?")
        sys.exit(1)

    test_1_login_scenarios()
    test_2_pub_sub_logic()
    
    print(f"\n{Colors.BOLD}Tests Completed.{Colors.ENDC}")

if __name__ == "__main__":
    run_tests()