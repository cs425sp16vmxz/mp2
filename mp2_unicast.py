import threading
import time
import socket
import random
from mp2_types import Message




class Receiver(threading.Thread):

    def __init__(self, (connection, address), multicast):
        # print >> sys.stderr, "Spawning Receiver thread"
        threading.Thread.__init__(self)
        self.connection = connection
        self.size = 1024
        # self.receive_event = receive_event
        self.multicast = multicast

    def run(self):
        running = 1
        while running:
            data = self.connection.recv(self.size)
            if data:
                # if (data[0] == '#'):
                #     message = Message.parse_message(data)
                #     Message.print_received(message)
                # else:
                self.multicast.deliver(data)
                # print >> sys.stderr, "Got some data " + data
            else:
                # print >> sys.stderr, "Closing connection"
                self.connection.close()
                running = 0
        # print >> sys.stderr, "Closing Receiver thread"


class Sender(threading.Thread):

    def __init__(self, to_process, data, delays):
        # print >> sys.stderr, "Spawning Sender thread"
        self.delay = random.randint(delays[0], delays[1])
        self.to_process = to_process
        self.data = data
        # print >> sys.stderr, "Sending message \"" + message + "\" to " + to_process.addr + ":" + str(to_process.port)
        threading.Thread.__init__(self)
        self.size = 1024

    def run(self):
        # print >> sys.stderr, "Starting Sender thread"
        time.sleep(self.delay / 1000.0)
        # print >> sys.stderr, "Slept for " + str(self.delay) + " ms."
        host = self.to_process.addr
        port = self.to_process.port
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # print >> sys.stderr, "Created client socket"
        try:
            s.connect((host, port))
            s.send(self.data)
            # data = s.recv(size)
        except Exception, e:
            print e
            print "Failed to connect to process %d" % self.to_process.id
        # print >> sys.stderr, data

        s.close()
        # print >> sys.stderr, "Closed client socket"
        # print >> sys.stderr, "Closing Sender thread"
