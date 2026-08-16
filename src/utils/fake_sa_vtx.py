#!/usr/bin/env python3
"""Fake SmartAudio V2.1 VTX for the SITL build.

Answers GET_SETTINGS and echoes the SET_* commands the FC issues, so the
3.3GHz SmartAudio path (FF3741 / FF3.7) can be exercised over a real serial
path. Reports the same bogus 5865MHz / 2-level power table the real FF3741
answers with, which is exactly what the firmware has to ignore.
"""
import argparse
import socket
import sys
import time

POLYGEN = 0xD5


def crc8(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ POLYGEN) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def response(cmd, payload):
    body = bytes([cmd, len(payload)]) + payload
    return b"\xAA\x55" + body + bytes([crc8(body)])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=5762)
    ap.add_argument("--seconds", type=float, default=20.0)
    args = ap.parse_args()

    sock = socket.create_connection(("127.0.0.1", args.port), 5)
    sock.settimeout(0.2)

    channel = 0
    power_dbm = 14
    freq = 5865
    mode = 0x14

    def settings():
        # V2.1 layout: chan, power(legacy), mode, freq hi/lo, power dbm, count, levels
        return response(0x11, bytes([channel, 0, mode, freq >> 8, freq & 0xFF,
                                     power_dbm, 4, 0, 14, 23, 27, 30]))

    deadline = time.time() + args.seconds
    buf = bytearray()
    while time.time() < deadline:
        try:
            data = sock.recv(256)
        except socket.timeout:
            continue
        if not data:
            break
        buf.extend(data)

        while True:
            start = buf.find(b"\xAA\x55")
            if start < 0 or len(buf) < start + 5:
                break
            cmd = buf[start + 2]
            length = buf[start + 3]
            end = start + 4 + length + 1
            if len(buf) < end:
                break
            payload = bytes(buf[start + 4:start + 4 + length])
            del buf[:end]

            # Commands go out as (code << 1) | 1.
            code = cmd >> 1
            if code == 0x01:        # GET_SETTINGS
                sock.sendall(settings())
            elif code == 0x02:      # SET_POWER
                power_dbm = payload[0] & 0x7F
                print("SET_POWER %d dBm" % power_dbm, flush=True)
                sock.sendall(settings())
            elif code == 0x03:      # SET_CHANNEL
                channel = payload[0]
                print("SET_CHANNEL index %d" % channel, flush=True)
                sock.sendall(settings())
            elif code == 0x04:      # SET_FREQUENCY
                freq = (payload[0] << 8) | payload[1]
                print("SET_FREQ %d MHz" % (freq & 0x3FFF), flush=True)
                sock.sendall(settings())
            elif code == 0x05:      # SET_MODE
                mode = payload[0]
                print("SET_MODE 0x%02X" % mode, flush=True)
                sock.sendall(settings())
            else:
                print("unknown cmd 0x%02X" % cmd, flush=True)

    sock.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
