import sys
import socket
s = socket.socket()
#host = '192.168.1.11'
#host = raw_input("Enter server listen ip: ")
host = sys.argv[1]
#host = '127.0.0.1'
#print(host)
port = 12345
s.connect((host, port))
print(s.recv(1024))
s.close() 
