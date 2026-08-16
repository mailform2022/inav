#!/usr/bin/env python3
"""Fake IRC Tramp VTX for the SITL build.

Connects to the TCP socket SITL exposes for the UART that carries the VTX
function and answers the capability ('r') and status ('v') queries the way the
real transmitters do, so the AUTO classifier can be exercised over a real
serial path instead of by calling the classifier directly.

Profiles:
  sx33     - what the SX33/FF3741 actually answers: generic 5.8GHz/600mW
  tx3339   - an honest answer for the 3.3GHz grid
  noname1  - what the Noname_1 answers on the bench: 850-5999MHz/1600mW
  mute     - never answers
"""
import argparse
import socket
import struct
import sys
import time

PROFILES = {
    # freqMin, freqMax, powerMax, extra bytes 8..13 of the 'r' frame
    "sx33": (5100, 6000, 600, bytes([0, 0, 0, 0, 0, 0])),
    "tx3339": (3060, 3480, 10000, bytes([0, 0, 0, 0, 0, 0])),
    # Captured from the real unit: r=[0F 72 52 03 6F 17 40 06 ...]
    "noname1": (850, 5999, 1600, bytes([0, 0, 0, 0, 0, 0])),
}


def frame(cmd, payload):
    pkt = bytearray(16)
    pkt[0] = 0x0F
    pkt[1] = cmd
    pkt[2:2 + len(payload)] = payload
    pkt[14] = sum(pkt[1:14]) & 0xFF
    pkt[15] = 0
    return bytes(pkt)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--profile", choices=list(PROFILES) + ["mute"], default="sx33")
    ap.add_argument("--port", type=int, default=5762)
    ap.add_argument("--seconds", type=float, default=20.0)
    args = ap.parse_args()

    sock = socket.create_connection(("127.0.0.1", args.port), 5)
    sock.settimeout(0.2)

    # A status frame of all zeroes is byte-identical to the query frame, and the
    # driver drops those as half-duplex echoes, so start from a plausible state.
    state_freq = 3330 if args.profile == "tx3339" else (3200 if args.profile == "noname1" else 5800)
    state_power = 25
    deadline = time.time() + args.seconds
    buf = bytearray()
    seen = {}

    while time.time() < deadline:
        try:
            data = sock.recv(256)
        except socket.timeout:
            continue
        if not data:
            break
        buf.extend(data)

        while len(buf) >= 16:
            if buf[0] != 0x0F:
                buf.pop(0)
                continue
            pkt = bytes(buf[:16])
            del buf[:16]
            cmd = pkt[1]
            param = struct.unpack("<H", pkt[2:4])[0]
            seen[chr(cmd)] = seen.get(chr(cmd), 0) + 1
            print("rx cmd %r param %d" % (chr(cmd), param), flush=True)

            if args.profile == "mute":
                continue

            fmin, fmax, pmax, extra = PROFILES[args.profile]
            if cmd == 0x72:  # capabilities
                sock.sendall(frame(0x72, struct.pack("<HHH", fmin, fmax, pmax) + extra))
            elif cmd == 0x76:  # status
                sock.sendall(frame(0x76, struct.pack("<HHH", state_freq, state_power, 0)))
            elif cmd == 0x46:  # set frequency
                state_freq = param
            elif cmd == 0x50:  # set power
                state_power = param

    print("summary: commands seen %s, freq=%d power=%d" % (seen, state_freq, state_power))
    sock.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
