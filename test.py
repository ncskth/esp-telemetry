import socket
import time
from threading import Thread

udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_socket.bind(("0.0.0.0", 1338))

tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcp_socket.connect(("172.16.223.237", 55671))
print("connected")
def get_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(0)
    try:
        s.connect(("34.34.34.34", 1))
        IP = s.getsockname()[0]
    except Exception:
        IP = "127.0.0.1"
    finally:
        s.close()
    return IP

def send(buf):
    totalsent = 0
    while totalsent < len(buf):
        sent = tcp_socket.send(buf[totalsent:])
        if sent == 0:
            raise RuntimeError("socket connection broken")
        totalsent = totalsent + sent

def udp_thread():
    while True:
        buf = udp_socket.recv(10000)
        printf(len(buf))
        for i in range(256):
            print(buf[i], end="")

t = Thread(target=udp_thread)
t.start()

ip = [int(x) for x in get_ip().split(".")]
buf = bytes([32, ip[0], ip[1], ip[2], ip[3], (1338 >> 8) & 0xff, 1338 & 0xff])
send(buf)