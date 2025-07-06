import socket
import threading

def recv_msg(sock):
    while True:
        msg = sock.recv(1024).decode()
        if msg:
            print(f"\nGanesh: {msg}")
            print("_" * 72)

def send_msg(sock):
    while True:
        msg = input("You (Friend): ")
        sock.send(msg.encode())
        print("_" * 72)

s = socket.socket()
server_ip = input("Enter Ganesh's IP address: ")
s.connect((server_ip, 12345))
print("Connected to Ganesh!")

threading.Thread(target=recv_msg, args=(s,), daemon=True).start()
send_msg(s)
