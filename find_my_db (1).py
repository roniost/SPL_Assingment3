import sqlite3
import os

# Check both possible locations
paths = ["stomp_server.db", "data/stomp_server.db"]

print(f"{'PATH':<40} | {'STATUS':<10} | {'USERS FOUND'}")
print("-" * 70)

for p in paths:
    if os.path.exists(p):
        try:
            conn = sqlite3.connect(p)
            cursor = conn.cursor()
            cursor.execute("SELECT * FROM Users")
            rows = cursor.fetchall()
            conn.close()
            
            status = "✅ FOUND"
            users = str(rows) if rows else "[Empty Table]"
            print(f"{os.path.abspath(p):<40} | {status:<10} | {users}")
        except Exception as e:
            print(f"{p:<40} | ❌ ERROR   | {e}")
    else:
        print(f"{p:<40} | ❌ MISSING  |")