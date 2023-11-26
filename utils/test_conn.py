import socket
import time

HOST = '2001:db8::1'
PORT = 4242

count = 0

with socket.socket(socket.AF_INET6, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    while True:
        send_str = f'Good Work! {count}'
        s.sendall(str.encode(send_str))
        print(send_str)
        count = count + 1
        time.sleep(2)
