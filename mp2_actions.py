import time


class PutAction(object):
    """docstring for PutAction"""
    def __init__(self, var_name, value):
        self.var_name = var_name
        self.value = value

    def client_execute(self, send_to_server):
        data = "p" + self.var_name + str(self.value)
        return send_to_server(data)

    def client_handle_response(self, data):
        assert data == "A"
        # print "received ACK"

    def server_execute(self, key_value_store):
        key_value_store[self.var_name] = self.value


class GetAction(object):
    """docstring for PutAction"""
    def __init__(self, var_name):
        self.var_name = var_name

    def client_execute(self, send_to_server):
        data = 'g' + self.var_name
        return send_to_server(data)

    def client_handle_response(self, data):
        # print "handling response"
        # print repr(data)
        assert data.isdigit()
        print data

    def server_execute(self, key_value_store):
        return key_value_store[self.var_name]


class DelayAction(object):
    """docstring for PutAction"""
    def __init__(self, milliseconds):
        self.milliseconds = milliseconds

    def client_execute(self):
        time.sleep(self.milliseconds / 1000.0)


class DumpAction(object):
    def client_execute(self, send_to_server):
        data = "d"
        return send_to_server(data)

    def client_handle_response(self, data):
        assert data == "A"
        # print "received ACK"

    def server_execute(self, key_value_store):
        print "Dumping"
        print key_value_store


class ActionParser(object):
    @staticmethod
    def parse_message(message):
        command = message[0]
        if command == 'p':
            var_name = message[1]
            value = int(message[2])
            return PutAction(var_name, value)
        elif command == 'g':
            var_name = message[1]
            return GetAction(var_name)
        elif command == 'd':
            return DumpAction()
