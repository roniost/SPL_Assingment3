#!/usr/bin/env python3
"""
Basic Python Server for STOMP Assignment – Stage 3.3

IMPORTANT:
DO NOT CHANGE the server name or the basic protocol.
Students should EXTEND this server by implementing
the methods below.
"""

import socket
import sys
import threading
import sqlite3
import os


SERVER_NAME = "STOMP_PYTHON_SQL_SERVER"  # DO NOT CHANGE!
DB_FILE = "stomp_server.db"              # DO NOT CHANGE!

# Reads data from the socket until a null byte is received.
def recv_null_terminated(sock: socket.socket) -> str: 
    data = b""
    while True:
        chunk = sock.recv(1024)
        if not chunk:
            return ""
        data += chunk
        if b"\0" in data:
            msg, _ = data.split(b"\0", 1)
            return msg.decode("utf-8", errors="replace")


def init_database():
    if os.path.exists(DB_FILE):
        print(f"[{SERVER_NAME}] Database file '{DB_FILE}' already exists.")
        return
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()

    c.execute('''CREATE TABLE IF NOT EXISTS users
                (username TEXT PRIMARY KEY, password TEXT NOT NULL)''')
    
    c.execute('''CREATE TABLE IF NOT EXISTS logins
                 (id INTEGER PRIMARY KEY AUTOINCREMENT,
                  username TEXT NOT NULL,
                  login_time TEXT NOT NULL,
                  logout_time TEXT,
                  FOREIGN KEY(username) REFERENCES users(username))''')
    
    c.execute('''CREATE TABLE IF NOT EXISTS files
                 (id INTEGER PRIMARY KEY AUTOINCREMENT,
                  username TEXT NOT NULL,
                  filename TEXT NOT NULL,
                  upload_time TEXT,
                  FOREIGN KEY(username) REFERENCES users(username))''')
    
    conn.commit()
    conn.close()
    print(f"[{SERVER_NAME}] Database initialized and tables created.")


def execute_sql_command(sql_command: str) -> str:
    try:
        conn = sqlite3.connect(DB_FILE)
        c = conn.cursor()
        c.execute(sql_command)
        conn.commit()
        return "done"
    except sqlite3.Error as e:
        return f"ERROR: {e}"
    finally: #maybe not needed
        try:
            conn.close()
        except Exception:
            pass

def execute_sql_query(sql_query: str) -> str:
    try:
        conn = sqlite3.connect(DB_FILE)
        c = conn.cursor()
        c.execute(sql_query)
        rows = c.fetchall()
        if not rows:
            return "EMPTY"
        result = "\n".join(["|".join(map(str, row)) for row in rows])
        return result
    except sqlite3.Error as e:
        return f"ERROR: {e}"
    finally:
        conn.close()

def handle_client(client_socket: socket.socket, addr):
    print(f"[{SERVER_NAME}] Client connected from {addr}")

    try:
        while True:
            message = recv_null_terminated(client_socket)
            if message == "":
                break

            if message.strip().upper().startswith("SELECT"): #ADDED
                response = execute_sql_query(message) #ADDED
            else: #ADDED
                response = execute_sql_command(message)  #ADDED
            
            #print(f"[{SERVER_NAME}] Received:")
            #print(message)

            client_socket.sendall((response + "\0").encode("utf-8"))

    except Exception as e:
        print(f"[{SERVER_NAME}] Error handling client {addr}: {e}")
    finally:
        try:
            client_socket.close()
        except Exception:
            pass
        print(f"[{SERVER_NAME}] Client {addr} disconnected")


def start_server(host="127.0.0.1", port=7778):

    init_database()
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        server_socket.bind((host, port))
        server_socket.listen(5)
        print(f"[{SERVER_NAME}] Server started on {host}:{port}")
        print(f"[{SERVER_NAME}] Waiting for connections...")

        while True:
            client_socket, addr = server_socket.accept()
            t = threading.Thread(
                target=handle_client,
                args=(client_socket, addr), 
                daemon=True
            )
            t.start()

    except KeyboardInterrupt:
        print(f"\n[{SERVER_NAME}] Shutting down server...")
    finally:
        try:
            server_socket.close()
        except Exception:
            pass


if __name__ == "__main__":
    port = 7778
    if len(sys.argv) > 1:
        raw_port = sys.argv[1].strip()
        try:
            port = int(raw_port)
        except ValueError:
            print(f"Invalid port '{raw_port}', falling back to default {port}")

    start_server(port=port)
