import socket
import threading

def recv_msg(conn):
    while True:
        msg = conn.recv(1024).decode()
        if msg:
            print(f"\nFriend: {msg}")
            print("_" * 72)

def send_msg(conn):
    while True:
        msg = input("You (Ganesh): ")
        conn.send(msg.encode())
        print("_" * 72)

s = socket.socket()
s.bind(('', 12345))
s.listen(1)

print("Waiting for connection...")
conn, addr = s.accept()
print(f"Connected with {addr}")

threading.Thread(target=recv_msg, args=(conn,), daemon=True).start()
send_msg(conn)