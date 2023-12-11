import socket
import ssl
import time
import pprint

HOST = '2001:db8::1'
PORT = 4242

count = 0

context = ssl.SSLContext(ssl.PROTOCOL_TLSv1_2)
context.load_verify_locations('./echo-apps-cert.pem')
context.set_ciphers('DEFAULT')

with socket.socket(socket.AF_INET6, socket.SOCK_STREAM) as sock:
    with context.wrap_socket(sock, server_hostname=HOST) as wsock:
        wsock.connect((HOST, PORT))
        cert = wsock.getpeercert()
        pprint.pprint(cert)
        while True:
            send_str = f'Good Work! {count}'
            wsock.sendall(str.encode(send_str))
            print(send_str)
            count = count + 1
            time.sleep(2)
