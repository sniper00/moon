# -*- coding:utf-8 -*-
from socket import *
import struct
import time
import Queue
import threading

HOST = "127.0.0.1"
PORT = 11111
ADDR = (HOST, PORT)

# BufferSize
BUFSIZ = 1024

def make_msg(msg):
    packdsp = "H%ss"%(len(msg))
    return struct.pack(packdsp,len(msg),msg)

class Network:
#build socket
    def __init__(self,addr):
        self.tcpCliSocket = socket(AF_INET, SOCK_STREAM)
        self.tcpCliSocket.connect(addr)
    def Send(self,data):
        self.tcpCliSocket.sendall(make_msg(data))

    def Recv(self):
        data = self.tcpCliSocket.recv(BUFSIZ)
        if data:
            mlen,msg = struct.unpack("H%ds"%(len(data)-2),data)
            print msg,"len:",mlen

c = Network(ADDR)

while True:
    data = raw_input(">")
    if not data:
        break
    c.Send(data)
    c.Recv()