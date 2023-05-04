import socket
import time
from threading import Thread
import math
udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_socket.bind(("0.0.0.0", 1338))

tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcp_socket.connect(("esp_telemetry.local", 55671))
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
    # return "192.168.4.2"
    return IP

def send(buf):
    totalsent = 0
    while totalsent < len(buf):
        sent = tcp_socket.send(buf[totalsent:])
        if sent == 0:
            raise RuntimeError("socket connection broken")
        totalsent = totalsent + sent

def udp_thread():
    received = 0
    last_print = time.time()
    frame_number = 0
    lost_frames = 0
    received_frames = 0
    while True:
        buf = udp_socket.recv(10000)
        if (len(buf) == 0):
            continue
        received += len(buf)
        if buf[0] != frame_number:
            delta = buf[0] - frame_number
            if delta <= 0:
                delta += 255
            lost_frames += delta
            frame_number = buf[0]
        received_frames += 1
        for i in range(len(buf)):
            if buf[i] != (frame_number + i) % 256:
                pass
                #print("invalid byte", (frame_number + i) % 256, buf[i])
        frame_number = (frame_number + 1) % 256

        if time.time() - last_print > 1:
            print(f"time scale {round(time.time() - last_print, 4)}")
            print(f"bandwidth {received} B/s")
            print(f"bandwidth {round(received * 8 / 1000000, 2)} Mb/s")
            print(f"lost {lost_frames}   \t got {received_frames}\t total {lost_frames + received_frames},\t packet loss ratio {round(lost_frames / (lost_frames + received_frames), 2)}")
            received = 0
            received_frames = 0
            lost_frames = 0
            last_print = time.time()


t = Thread(target=udp_thread)
t.start()

ip = [int(x) for x in get_ip().split(".")]
buf = bytes([32, ip[0], ip[1], ip[2], ip[3], (1338 >> 8) & 0xff, 1338 & 0xff])
print(buf)
for penis in buf:
    print(int(penis))
send(buf)
tcp_socket.close()