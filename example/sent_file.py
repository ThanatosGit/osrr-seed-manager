import os
import socket
import sys

if len(sys.argv) < 3:
    print("First parameter is filepath, second parameter the ip")
    exit(-1)

filepath = sys.argv[1]
ip = sys.argv[2]
blocksize = 8192 * 10

if os.path.exists(filepath):
    socket_handle = socket.create_connection((ip, 1234), 1000)
    with open(filepath, 'rb') as file:
        send_bytes = bytearray()
        send_bytes.extend(map(ord, filepath))
        # length of string
        socket_handle.send(len(filepath).to_bytes(4, "little"))
        # string itself
        socket_handle.send(send_bytes)

        # length of zip
        file_size = file.seek(0, os.SEEK_END)
        file.seek(0)
        socket_handle.send(file_size.to_bytes(4, "little"))
        # zip itself
        packet = file.read(blocksize)
        sent_bytes = 0
        while packet != b'':
            sent_bytes = sent_bytes + len(packet)
            left_to_sent = len(packet)
            while left_to_sent > 0:
               left_to_sent = left_to_sent - socket_handle.send(packet[-left_to_sent:])
            print(sent_bytes)
            packet = file.read(blocksize)
        socket_handle.shutdown(socket.SHUT_RDWR)
        socket_handle.close()



