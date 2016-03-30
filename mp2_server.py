import sys
import argparse
import socket
import select
import re
from mp2_types import Process, Message
from mp2_unicast import Sender, Receiver
from mp2_multicast import TotalOrder


class Server:

    def __init__(self, processes, delays, id, use_total, pretty):
        self.processes = processes
        self.delays = delays
        self.id = id
        self.host = ''  # tell bind to bind to localhost
        self.port = self.processes[self.id].port
        self.pretty = pretty

        self.size = 1024
        self.socket = None
        self.threads = []

        # instantiate the multicast thread
        if not use_total:
            multicast = CausalOrder(self.processes, self.id, self.delays, self.pretty)
        else:
            multicast = TotalOrder(self.processes, self.id, self.delays, self.pretty)
        multicast.start()
        self.threads.append(multicast)
        self.multicast = multicast

    def open_socket(self):
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.bind((self.host, self.port))
            self.socket.listen(20)
        except socket.error, (value, message):
            if self.socket:
                self.socket.close()
            print "Could not open socket: " + message
            sys.exit(1)

    def run(self):
        self.open_socket()
        # setup for select(2) system call
        input = [self.socket, sys.stdin]

        running = True
        while running:
            # block until we receive packets or keyboard input
            inputready, outputready, exceptready = select.select(input, [], [])

            for s in inputready:
                # received packets
                if s == self.socket:
                    # unicast_receive(source, message)
                    c = Receiver(self.socket.accept(), self.multicast)
                    c.start()
                    self.threads.append(c)
                # handle keyboard input
                elif s == sys.stdin:
                    # get line, strip newline
                    line = sys.stdin.readline().rstrip('\n')
                    # get the first word
                    command = line.split(' ')[0]
                    if command == "quit":
                        running = False
                        # wake up the multicast thread so it can cleanly exit
                        self.multicast.running = False
                        self.multicast.receive_event.set()
                    # usage: send destination Message
                    elif command == "send":
                        # first group is the command
                        m = re.match(r'(\w*) (\w*) (.*)', line, re.M | re.I)
                        destination = int(m.group(2))
                        text = m.group(3)

                        # unicast_send(destination, message)
                        message = Message(text, self.id)
                        Message.print_sent(message, destination)
                        data = str(message)
                        c = Sender(self.processes[destination], data, self.delays)
                        c.start()
                        self.threads.append(c)
                    # usage: msend Message
                    elif command == "msend":
                        m = re.match(r'(\w*) (.*)', line, re.M | re.I)
                        text = m.group(2)
                        # this way the multicast can implement whatever ordering procotcol it wants
                        self.multicast.multicast(text)

        # close all threads
        self.socket.close()
        for c in self.threads:
            c.join()

parser = argparse.ArgumentParser()
parser.add_argument("config", help="configuration file as specified in MP2.pdf", type=argparse.FileType('r'))
parser.add_argument("id", help="id of the process", type=int)
parser.add_argument("-t", "--total", help="use total ordering", action="store_true")
parser.add_argument("-p", "--pretty", help="pretty print multicast messages", action="store_true")
args = parser.parse_args()

# read and split the text line by line
fulltext = args.config.read().split('\n')

# drop the first line about miliseconds and split each line by space
pre = map(lambda x: x.split(' '), fulltext[1:])

# convert the generic array strings to Process objects
processes = {}
for i in map(lambda x: Process(x[0], x[1], x[2]), pre):
    processes[i.id] = i

# pull out the min and max delays
dr = re.match(r'min_delay\((\w*)\) max_delay\((\w*)\)', fulltext[0], re.M | re.I)
delays = (int(dr.group(1)), int(dr.group(2)))

# make sure the process supplied at the command line is valid
if (args.id in processes.keys()):
    s = Server(processes, delays, args.id, args.total, args.pretty)
    s.run()
else:
    print "Invalid process ID"
