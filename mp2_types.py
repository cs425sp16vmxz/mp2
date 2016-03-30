import re
import datetime
import uuid
from mp2_actions import PutAction, GetAction, DelayAction, DumpAction

class Process(object):

    def __init__(self, id, addr, port):
        super(Process, self).__init__()
        self.id = int(id)
        self.addr = addr
        self.port = int(port)

    def __str__(self):
        return "Id: %d\tHost: %s\tPort: %d" % (self.id, self.addr, self.port)


class Message(object):

    def __init__(self, text, sender):
        self.sender = sender
        self.text = text

    def __str__(self):
        return "#Sender: %d##%s" % (self.sender, self.text)

    @staticmethod
    def parse_message(data):
        # print "Attempting to parse:\n%s" % data
        m = re.match(r"#Sender: (\w*)##(.*)", data, re.M | re.I)
        sender = int(m.group(1))
        text = m.group(2)
        return Message(text, sender)

    @staticmethod
    def receive(message):
        data = message.text
        # print 'receiving'
        # print data
        command = data[0]
        if command == 'p':
            var_name = data[1]
            value = int(data[2])
            return PutAction(var_name, value)
        elif command == 'g':
            var_name = data[1]
            return GetAction(var_name)
        elif command == 'd':
            return DumpAction()

    @staticmethod
    def print_sent(message, destination):
        now = str(datetime.datetime.now())
        print "Sent \"%s\" to process %d, system time is %s" % (message.text, destination, now[now.index(" ") + 1:])

    @staticmethod
    def print_received(message, prefix="Received"):
        now = str(datetime.datetime.now())
        print prefix + " \"%s\" from process %d, system time is %s" % (message.text, message.sender, now[now.index(" ") + 1:])

    @staticmethod
    def print_pretty(message):
        now = str(datetime.datetime.now())
        print "[%s] %d: %s" % (now[now.index(" ") + 1:], message.sender, message.text)


class CausalMessage(object):

    def __init__(self, message, vector):
        self.message = message
        self.vector = vector

    def __str__(self):
        return "@Vector: [%s]@@%s" % (' '.join(map(str, self.vector)), self.message)

    @staticmethod
    def parse_message(data):
        # print "Attempting to parse:\n%s" % data
        cm = re.match(r"@Vector: \[([0-9 ]*)\]@@(.*)", data, re.M | re.I)
        vector = [int(i) for i in cm.group(1).split()]
        # print "This is the vector I got: %s" % str(vector)
        message = Message.parse_message(cm.group(2))

        return CausalMessage(message, vector)


class TotalMessage(object):

    def __init__(self, message, unique_id=None):
        if unique_id is None:
            unique_id = str(uuid.uuid4())
        self.message = message
        self.unique_id = unique_id

    def __str__(self):
        return "@UUID: %s@@%s" % (self.unique_id, self.message)

    @staticmethod
    def parse_message(data):
        # print "Attempting to parse:\n%s" % data
        tm = re.match(r"@UUID: ([a-z0-9\-]*)@@(.*)", data, re.M | re.I)
        unique_id = tm.group(1)
        message = Message.parse_message(tm.group(2))

        return TotalMessage(message, unique_id)


class SequencerMessage(object):

    def __init__(self, unique_id, sequence_number):
        self.unique_id = unique_id
        self.sequence_number = sequence_number

    def __str__(self):
        return "~Seq: %d~~UUID: %s" % (self.sequence_number, self.unique_id)

    @staticmethod
    def parse_message(data):
        # print "Attempting to parse:\n%s" % data
        tm = re.match(r"~Seq: ([0-9]*)~~UUID: (.*)", data, re.M | re.I)
        sequence_number = int(tm.group(1))
        unique_id = tm.group(2)

        return SequencerMessage(unique_id, sequence_number)

def parse_config(config):
        # read and split the text line by line
    fulltext = config.read().split('\n')
    # drop the first line about miliseconds and split each line by space
    pre = map(lambda x: x.split(' '), fulltext[1:])
    # convert the generic array strings to Process objects
    processes = {}
    for i in map(lambda x: Process(x[0], x[1], x[2]), pre):
        processes[i.id] = i
    # pull out the min and max delays
    dr = re.match(r'min_delay\((\w*)\) max_delay\((\w*)\)', fulltext[0], re.M | re.I)
    delays = (int(dr.group(1)), int(dr.group(2)))
    return (processes, delays)
        
