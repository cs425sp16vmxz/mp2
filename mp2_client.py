import sys
import argparse
import socket
import re
import threading
from mp2_types import parse_config
from mp2_unicast import Sender
from mp2_actions import PutAction, GetAction, DelayAction, DumpAction


class Dispatch(threading.Thread):

    def __init__(self, port):  # processes, id, delays, pretty):
        threading.Thread.__init__(self)

        self.host = ''  # tell bind to bind to localhost
        # self.port = self.processes[self.id].port
        self.port = port
        # self.pretty = pretty

        # self.size = 1024
        self.socket = None
        # threads = []

        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.bind((self.host, self.port))
            self.socket.listen(20)
        except socket.error, (value, message):
            if self.socket:
                self.socket.close()
            print "Could not open socket: " + message
            sys.exit(1)

        self.action_queue = []
        self.action_queue_lock = threading.Lock()
        self.receive_event = threading.Event()

        # self.latest_sequence_number = 0

        self.running = True

    def add_action(self, action):
        self.action_queue_lock.acquire()
        self.action_queue.append(action)
        self.receive_event.set()
        self.action_queue_lock.release()

    def run(self):
        while self.running:
            # sleep until woken up by the signal
            # print "sleeping!"
            self.receive_event.wait()
            # print "awake!"
            if self.running:  # woken up because queue got new data
                self.action_queue_lock.acquire()
                try:
                    action = self.action_queue.pop(0)
                    # print "popped an action"
                    # print action
                    self.action_queue_lock.release()

                    if (type(action) is DelayAction):
                        # print "o hai, delaying"
                        action.client_execute()
                    else:
                        thread = action.client_execute(send_to_key_value_store)
                        (connection, address) = self.socket.accept()

                        running = 1
                        while running:
                            data = connection.recv(1024)
                            if data:
                                # may be unnecessary
                                data = data.strip()
                                action.client_handle_response(data)
                            else:
                                connection.close()
                                running = 0

                        thread.join()
                except Exception, e:
                    # print repr(e)
                    if type(e) is not IndexError:
                        raise e
    
                    self.receive_event.clear()
                    if self.action_queue_lock.locked():
                        self.action_queue_lock.release()

        # close all threads
        self.socket.close()


def send_to_key_value_store(data):
    # print "sending"
    c = Sender(current_server, data, delays)
    c.start()
    # print "started send"
    return c


parser = argparse.ArgumentParser()
parser.add_argument("config", help="configuration file as specified in mp2.pdf", type=argparse.FileType('r'))
parser.add_argument("port", help="client port", type=int)

args = parser.parse_args()

(processes, delays) = parse_config(args.config)

current_server = processes[1]


dispatch = Dispatch(args.port)
dispatch.start()

running = True
while running:

    try:
        # get line, strip newline
        line = sys.stdin.readline().rstrip('\n')
        # get the first word
        command = line.split(' ')[0]
        if command == "quit":
            running = False
            # wake up the multicast thread so it can cleanly exit
            # self.multicast.running = False
            dispatch.running = False
            dispatch.receive_event.set()
        # usage: send destination Message
        # You only have to handle variable names a-z (i.e. a single, lowercase letter),
        # and the variables can only take (integer) values of 0-9.
        elif command == "put":
            # first group is the command
            m = re.match(r'put ([a-z]) ([0-9])', line, re.M | re.I)
            var_name = m.group(1)
            value = int(m.group(2))
            p = PutAction(var_name, value)
            dispatch.add_action(p)
            # unicast_send(destination, message)
            # message = Message(text, self.id)
            # Message.print_sent(message, destination)
            # data = str(message)
            # c.start()
        # usage: msend Message
        elif command == "get":
            m = re.match(r'get ([a-z])', line, re.M | re.I)
            var_name = m.group(1)
            g = GetAction(var_name)
            dispatch.add_action(g)
            # this way the multicast can implement whatever ordering procotcol it wants
            # self.multicast.multicast(text)
        elif command == "dump":
            d = DumpAction()
            dispatch.add_action(d)
        elif command == "delay":
            m = re.match(r'delay ([0-9]+)', line, re.M | re.I)
            milliseconds = int(m.group(1))
            d = DelayAction(milliseconds)
            dispatch.add_action(d)
            # this way the multicast can implement whatever ordering procotcol it wants
            # self.multicast.multicast(text)
    except Exception, e:
        print "exception main REPL"
        print e





# s.run()
