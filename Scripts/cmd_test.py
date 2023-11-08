import cmd, sys
import socket
import socket_test

class TCPShell(cmd.Cmd):
    intro = 'Welcome to the TCPShell.   Type help or ? to list commands.\n'
    prompt = '>> '
    file = None

    HOST = "192.168.0.226"
    PORT = 3333
    tcp_socket = None

    # ----- basic commands -----
    def do_echo(self, arg):
        'Send string to device and echo the response'
        print(arg)
        payload = bytes(arg, "utf-8")
        resp = socket_test.send_message(self.tcp_socket, socket_test.MessageID.ECHO, payload, True)
        if (resp):
            print(resp)
    def do_crc(self, arg):
        'Calculate CRC for given input'
        data = bytes(arg, 'utf-8')
        res = socket_test.simple_crc16(0, data)
        print(hex(res))
    def do_nvm_write(self, arg):
        file_name = arg.split()[0]
        print(file_name)
        file = open(file_name, "rb")
        file_bytes = file.read()
        socket_test.send_nvm_data(self.tcp_socket, file_name, file_bytes)

    def do_exit(self, arg):
        'Stop recording, close the turtle window, and exit:  BYE'
        print('Exiting TCPShell...')
        self.close()
        return True

    # ----- record and playback -----
    def do_record(self, arg):
        'Save future commands to filename:  RECORD rose.cmd'
        self.file = open(arg, 'w')
    def do_playback(self, arg):
        'Playback commands from a file:  PLAYBACK rose.cmd'
        self.close()
        with open(arg) as f:
            self.cmdqueue.extend(f.read().splitlines())
    def precmd(self, line):
        line = line.lower()
        if self.file and 'playback' not in line:
            print(line, file=self.file)
        return line
    def close(self):
        if self.file:
            self.file.close()
            self.file = None

def parse(arg):
    'Convert a series of zero or more numbers to an argument tuple'
    return tuple(map(int, arg.split()))

if __name__ == '__main__':
    TCPShell.tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    TCPShell.tcp_socket.connect((TCPShell.HOST, TCPShell.PORT))
    TCPShell().cmdloop()