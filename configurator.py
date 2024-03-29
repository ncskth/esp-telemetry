import argparse
import os
import time
import socket
import sys
import struct

#telemetry frames
ID_START_STREAMING = 32
ID_FIRMWARE = 42
ID_WIFI = 1
ID_TEMPERATURE = 2
ID_TX_POWER = 3
PORT = 55671

def send(buf):
    totalsent = 0
    while totalsent < len(buf):
        sent = s.send(buf[totalsent:])
        if sent == 0:
            raise RuntimeError("socket connection broken")
        totalsent = totalsent + sent

def receive(msg_len):
    chunks = []
    bytes_recd = 0
    while bytes_recd < msg_len:
        chunk = s.recv(min(msg_len - bytes_recd, 2048))
        if chunk == b'':
            raise RuntimeError("socket connection broken")
        chunks.append(chunk)
        bytes_recd = bytes_recd + len(chunk)
        buf = b''.join(chunks)
        return buf

def wait_ack():
    buf = receive(1)
    if buf[0] == 1:
        return True
    else:
        return False

parser = argparse.ArgumentParser(description='generic configurator')
parser.add_argument('ip', type=str,
                    help='ip')

parser.add_argument('--wifi', nargs = 2, type=str,
                    help='--wifi [ssid] [password]')

parser.add_argument('--firmware', type=str,
                    help="upload a firmware file")

parser.add_argument('--verify', action='store_true',
                    help="verify and apply the new firmware")

parser.add_argument('--temperature', action='store_true')

parser.add_argument('--tx-power', type=int)


args = parser.parse_args()

print("connecting...")
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((args.ip, PORT))
print("connected")

if args.wifi:
    print("updating wifi")
    buf = bytearray([ID_WIFI])
    buf += struct.pack("32s64s", bytes(args.wifi[0], "utf-8"), bytes(args.wifi[1], "utf-8"))
    send(buf)
    if not wait_ack():
        print("failed to set wifi")

success = True
if args.firmware:
    print("updating firmware")
    f = open(args.firmware, "rb")
    size = os.path.getsize(args.firmware)
    send(bytearray([ID_FIRMWARE]))
    send(struct.pack("<Q", size))
    sent = 0
    while True:
        buf = f.read(255)
        if len(buf) == 0:
            break
        send(buf)
        sent += len(buf)
        print(f"sent ota packet. {round(sent/size * 100, 2)}% done")
    if not wait_ack():
        success = False
        print("failed to send update\naborting")

if success and args.verify:
    if args.firmware:
        print("upload done. attempting to verify")
        time.sleep(5)
        while True:
            try:
                s.close()
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.settimeout(5)
                print("trying to connect...")
                s.connect((args.ip, PORT))
                s.settimeout(None)
                break
            except:
                pass
        print("connected")

    print("sending a zero size OTA packet to confirm a successful update")
    send(bytearray([ID_FIRMWARE]))
    send(bytearray([0] * 8))
    if not wait_ack():
        print("could not confirm the update and it will probably be rolled back")
    else:
        print("updated!")

if args.temperature:
    send(bytearray([ID_TEMPERATURE]))
    print(f"temperature: {receive(1)[0]}")

if args.tx_power:
    print("setting tx-power")
    buf = bytearray([ID_TX_POWER, args.tx_power])
    send(buf)
    if not wait_ack():
        print("failed to set tx power")

s.close()