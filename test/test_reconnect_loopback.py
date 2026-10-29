"""Local-only TCP/WebSocket fault fixture; no cloud tokens or device traffic."""
import base64
import hashlib
import json
import socket
import subprocess
import sys
import threading
import time

listener = socket.socket()
listener.bind(('127.0.0.1', 0))
listener.listen()
listener.settimeout(0.2)
stop = threading.Event()
attempts = []
errors = []


def server():
    while not stop.is_set():
        try:
            conn, _ = listener.accept()
        except socket.timeout:
            continue
        attempts.append(time.monotonic())
        index = len(attempts)
        try:
            with conn:
                conn.settimeout(2)
                headers = b''
                while b'\r\n\r\n' not in headers:
                    chunk = conn.recv(4096)
                    if not chunk:
                        raise RuntimeError('early EOF')
                    headers += chunk
                key = next(line.split(b':', 1)[1].strip() for line in headers.split(b'\r\n')
                           if line.lower().startswith(b'sec-websocket-key:'))
                accept = base64.b64encode(hashlib.sha1(key + b'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest())
                conn.sendall(b'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ' + accept + b'\r\n\r\n')
                if index <= 4:
                    time.sleep(0.05)
                    conn.sendall(b'\x88\x02\x03\xe8')
                    continue
                healthy_start = time.monotonic()
                while not stop.is_set():
                    # Valid inbound activity and ping responses keep heartbeat healthy.
                    conn.sendall(b'\x8a\x00')
                    time.sleep(0.5)
                    if index == 5 and time.monotonic()-healthy_start >= 65:
                        conn.sendall(b'\x88\x02\x03\xe8')
                        break
        except (BrokenPipeError, ConnectionResetError):
            pass
        except Exception as error:
            errors.append(type(error).__name__ + ': ' + str(error))


worker = threading.Thread(target=server)
worker.start()
try:
    result = subprocess.run([sys.argv[1], str(listener.getsockname()[1])], timeout=170,
                            capture_output=True, text=True)
finally:
    stop.set()
    worker.join(timeout=3)
    listener.close()
gaps = [b-a for a, b in zip(attempts, attempts[1:])]
print(result.stdout)
print(result.stderr)
print(json.dumps({'attempts': len(attempts), 'gaps_seconds': gaps, 'server_errors': errors}))
assert result.returncode == 0 and not errors
for gap, base in zip(gaps[:4], [2, 4, 8, 16]):
    assert base/2 <= gap <= base+1, (gap, base)
assert 66 <= gaps[4] <= 69, gaps[4]  # 65s stable session + reset 1-2s delay
print('PASS real Linux transport flapping, stable-session reset and recovery')
