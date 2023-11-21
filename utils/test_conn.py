import socket
import time

HOST = '2001:db8::1'
PORT = 4242

count = 0

with socket.socket(socket.AF_INET6, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    while True:
        s.sendall(str.encode(f'Good Work! {count}'))
        count = count + 1
        time.sleep(2)
