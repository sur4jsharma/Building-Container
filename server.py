import sys
import socket

s = socket.socket()
#host = socket.gethostname()
#host = '192.168.1.11'
#host = '127.0.0.1'
#host = raw_input("Enter ip for server: ")

arg_count = len(sys.argv)
if arg_count == 1:
    print "server ip address is not passed as argument"
    exit(0)
host = sys.argv[1]
port = 12345
s.bind((host, port))
s.listen(5)
print("listening")
cli_socket, addr = s.accept()
print 'Got connection from', addr
cli_socket.send('Thank you for connecting')
cli_socket.close()
