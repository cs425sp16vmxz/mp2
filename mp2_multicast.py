import threading
from mp2_types import Process, Message, CausalMessage, TotalMessage, SequencerMessage
from mp2_unicast import Sender
from mp2_actions import ActionParser


class TotalOrder(threading.Thread):
    """Total order protocol. Implements a total order multicast on top of the unicast primitives.

    Attributes
    ----------
    delays : (int, int)
        Pair of min and max delay to use when generating the random delay for sending
    hold_back_queue : TotalMessage list
        Queue where messages are buffered for delivery
    id : int
        The current process's id
    is_sequencer : bool
        This process is the sequencer for the set
    latest_sequence_number : int
        Recipient side, the last sequence number that I have seen
    pretty : bool
        Pretty print instead of verbose MP print
    processes : Process list
        List of processes from Server
    queue_lock : Lock
        Mutex for working with the hold_back_queue
    receive_event : Event
        Signal to wake up the thread that delivers messages when a new message is enqueued
    running : bool
        Main event loop condition
    sequence_number : int
        Sequencer side, the number that will get sent on the next sequence message
    sequence_queue : list
        Recipient side, queue to hold sequence messages
    sequence_queue_lock : Lock
        Lock for the sequence queue, this might be overkill
    threads : Thread list
        Keeps track of the threads spawned to multicast messages
    """
    def __init__(self, processes, id, delays, pretty):
        threading.Thread.__init__(self)
        self.processes = processes
        self.id = id
        self.delays = delays
        self.pretty = pretty
        self.is_sequencer = False

        # make a lookup table that maps the ids of the processes -> indexes of vector timestamp
        ids = self.processes.keys()[:]
        ids.sort()  # sort to guarantee consistency
        if self.id == ids[0]:
            self.is_sequencer = True
            self.sequence_number = 0

        self.sequence_queue = []
        self.sequence_queue_lock = threading.Lock()

        self.hold_back_queue = []
        self.queue_lock = threading.Lock()
        self.receive_event = threading.Event()

        self.latest_sequence_number = 0

        self.threads = []

        self.key_value_store = {}

        self.running = True

    def run(self):
        """Main event loop
        """
        while self.running:
            # sleep until woken up by the signal
            self.receive_event.wait()

            if self.running:  # woken up because queue got new data
                self.queue_lock.acquire()
                self.sequence_queue_lock.acquire()

                changed_sequence_number = True

                # while messages remain to be delivered
                while (changed_sequence_number):
                    changed_sequence_number = False
                    # find the sequencer message for the current sequence number
                    sequencer_message = next((m for m in self.sequence_queue if m.sequence_number == self.latest_sequence_number), None)
                    if sequencer_message is not None:
                        # find the message described by that sequence message
                        total_message = next((m for m in self.hold_back_queue if m.unique_id == sequencer_message.unique_id), None)
                        if total_message is not None:
                            # process
                            self.hold_back_queue.remove(total_message)
                            self.sequence_queue.remove(sequencer_message)


                            action = Message.receive(total_message.message)
                            response = action.server_execute(self.key_value_store)
                            
                            if total_message.message.sender == self.id:
                                if not response:
                                    response = "A"

                                client = Process(0, '127.0.0.1', 6000)
                                c = Sender(client, str(response), self.delays)
                                c.start()
                                print "I need to respond to client"
                            
                            # print repr(action)
                            # if not self.pretty:
                            # Message.print_received(total_message.message, "Delivered")
                            # else:
                            #     Message.print_pretty(total_message.message)

                            self.latest_sequence_number = sequencer_message.sequence_number + 1

                            changed_sequence_number = True

                self.sequence_queue_lock.release()
                self.queue_lock.release()
                # processing done, sleep the thread
                self.receive_event.clear()
        for c in self.threads:
            c.join()

    def deliver(self, data):
        """Parses received multicast data and puts the message onto the hold back queue

        Parameters
        ----------
        data : string
            Raw string received from socket
        """
        # message is a sequence message
        head = data[0]
        if (head.isalpha()):
            self.multicast(data)
        elif (head == '~'):
            sequencer_message = SequencerMessage.parse_message(data)
            self.sequence_queue_lock.acquire()
            self.sequence_queue.append(sequencer_message)
            self.sequence_queue_lock.release()
        else:
            total_message = TotalMessage.parse_message(data)
            self.queue_lock.acquire()
            self.hold_back_queue.append(total_message)
            if self.is_sequencer:
                self.sequencer_multicast(total_message.unique_id, self.sequence_number)
                self.sequence_number += 1
            self.queue_lock.release()
        # wake up the delivery thread
        self.receive_event.set()

    def multicast(self, text):
        """Total order multicast to the registered processes

        Parameters
        ----------
        text : string
            Text of the message to be sent
        """
        message = Message(text, self.id)
        total_message = TotalMessage(message)
        self.b_multicast(total_message)

    def sequencer_multicast(self, unique_id, sequence_number):
        sequencer_message = SequencerMessage(unique_id, sequence_number)
        self.b_multicast(sequencer_message)

    def b_multicast(self, total_or_sequencer_message):
        """Function for b-multicast, used by sequencer and total order thread

        Parameters
        ----------
        total_or_sequencer_message : TotalMessage or SequencerMessage
            Push out to everyone, can handle sequencer messages too
        """
        data = str(total_or_sequencer_message)
        for destination in self.processes:
            # spawn a thread to send the message
            if (type(total_or_sequencer_message) is TotalMessage):
                if not self.pretty:
                    Message.print_sent(total_or_sequencer_message.message, destination)
            c = Sender(self.processes[destination], data, self.delays)
            c.start()
            self.threads.append(c)
