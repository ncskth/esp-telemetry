import socket
import time

udp_server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_server.bind(("0.0.0.0", 1338))

last_print = time.time()
received = 0
while True:
    buf = udp_server.recv(2048)
    received += len(buf)
    if time.time() - last_print > 1:
        print(received * 8 / 1e6)
        received = 0
        last_print = time.time()