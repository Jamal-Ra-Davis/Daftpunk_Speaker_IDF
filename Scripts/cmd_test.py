import cmd, sys
import socket
import socket_test
import argparse
import time

class TCPShell(cmd.Cmd):
    intro = 'Welcome to the TCPShell.   Type help or ? to list commands.\n'
    prompt = '>> '
    file = None

    HOST = None
    PORT = 3333
    tcp_socket = None

    # ----- basic commands -----
    def do_echo(self, arg):
        'Send string to device and echo the response: echo <echo_string>'
        print(arg)
        payload = bytes(arg, "utf-8")
        resp = socket_test.send_message(self.tcp_socket, socket_test.MessageID.ECHO, payload, True)
        if (resp):
            print(resp)

    def do_crc(self, arg):
        'Calculate CRC for given input: crc <input>'
        data = bytes(arg, 'utf-8')
        res = socket_test.simple_crc16(0, data)
        print(hex(res))

    def do_nvm_write(self, arg):
        'Writes specificed file to NVM on device: nvm_write <file_path>'
        file_name = arg.split()[0]
        print(file_name)
        file = open(file_name, "rb")
        file_bytes = file.read()
        file.close()
        socket_test.send_nvm_data(self.tcp_socket, file_name, file_bytes)

    def do_write_audio_asset(self, arg):
        'Writes specified audio file to NVM on device: write_audio_asset <Audio ID (0 - 7)> <file_path> <filename_dev>'
        audio_id = int(arg.split()[0])
        file_name = arg.split()[1]
        file_name_dev = arg.split()[2]
        print(f"Audio ID: {audio_id}, File Name: {file_name}, File Name Device: {file_name_dev}")
        file = open(file_name, "rb")
        file_bytes = file.read()
        file.close()
        try:
            socket_test.send_audio_data(self.tcp_socket, audio_id, file_name_dev, file_bytes)
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_get_audio_metadata(self, arg):
        'Returns audio meta data: get_audio_metadata'
        try:
            data = socket_test.get_audio_metadata(self.tcp_socket)
            print(data)
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")
            
    def do_play_audio_asset(self, arg):
        'Plays specificed audio file based on Audio ID: play_audio_asset <Audio ID (0 - 7)>'
        audio_id = int(arg.split()[0])
        print(f"Audio ID: {audio_id}")
        try:
            socket_test.play_audio_asset(self.tcp_socket, audio_id)
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_nvm_erase_chip(self, arg):
        'Fomat flash storage: nvm_erase_chip'
        print("About to format NVM, this will take a while...")
        print("NOTE: Device will need to be reset after formatting for filesystem to be created")
        socket_test.send_message(self.tcp_socket, socket_test.MessageID.NVM_ERASE_CHIP, None, False)

    def do_get_stack_info(self, arg):
        'Returns Stack Information: get_stack_info'
        try:
            info = socket_test.get_stack_info(self.tcp_socket)
            print(info)
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_i2c_write(self, arg):
        'i2c_write <device_address> <reg_address> <payload>'
        arg_list = arg.split()
        if (len(arg_list) < 3):
            print("Error: Invalid input, need at least 3 arguments")
            return
        device_address = int(arg_list[0], 0)
        reg_address = int(arg_list[1], 0)
        payload = socket_test.string_to_bytearray(arg_list[2:])
        print(f"Device Address: {device_address}")
        print(f"Reg Address: {reg_address}")
        print(f"Payload: {payload}")

        try:
            socket_test.i2c_write(self.tcp_socket, device_address, reg_address, payload)
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_i2c_write_read(self, arg):
        'i2c_write_read <device_address> <reg_address> <read_len>'
        arg_list = arg.split()
        if (len(arg_list) != 3):
            print("Error: Invalid input, needs 3 arguments")
            return
        device_address = int(arg_list[0], 0)
        reg_address = int(arg_list[1], 0)
        read_len = int(arg_list[2], 0)
        print(f"Device Address: {device_address}")
        print(f"Reg Address: {reg_address}")
        print(f"Read Len: {read_len}")

        try:
            resp = socket_test.i2c_write_read(self.tcp_socket, device_address, reg_address, read_len)
            print(f"Resp: {resp}")
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_mem_read_scratch(self, arg):
        'Returns address and contents of scratch memory: mem_read_scratch'
        try:
            address, data = socket_test.mem_read_scratch(self.tcp_socket)
            print(f"Address: {hex(address)}, data_len: {len(data)}, data: {data}")
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_mem_read(self, arg):
        'mem_read <address>'
        arg_list = arg.split()
        if (len(arg_list) != 1):
            print("Error: Invalid input, needs 1 arguments")
            return
        address = int(arg_list[0], 0)
        print(f"Address: {hex(address)}")

        try:
            val = socket_test.mem_read(self.tcp_socket, address)
            print(f"mem[{hex(address)}] = {val}")
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_mem_write(self, arg):
        'mem_write <address> <val>'
        arg_list = arg.split()
        if (len(arg_list) != 2):
            print("Error: Invalid input, needs 2 arguments")
            return
        address = int(arg_list[0], 0)
        val = int(arg_list[1], 0)
        print(f"Address: {hex(address)}")
        print(f"Val: {hex(val)}")

        try:
            socket_test.mem_write(self.tcp_socket, address, val)
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_gpio_set_config(self, arg):
        'gpio_set_config <pin_num> <pin_mode> <pin_strap>'
        arg_list = arg.split()
        if (len(arg_list) != 3):
            print("Error: Invalid input, needs 3 arguments")
            return
        pin_num = int(arg_list[0], 0)
        pin_mode = int(arg_list[1], 0)
        pin_strap = int(arg_list[2], 0)
        print(f"Pin Num: {pin_num}")
        print(f"Pin Mode: {pin_mode}")
        print(f"Pin Strap: {pin_strap}")

        try:
            socket_test.gpio_set_config(self.tcp_socket, pin_num, pin_mode, pin_strap)
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_gpio_read(self, arg):
        'gpio_read <pin_num>'
        arg_list = arg.split()
        if (len(arg_list) != 1):
            print("Error: Invalid input, needs 1 arguments")
            return
        pin_num = int(arg_list[0], 0)
        print(f"Pin Num: {pin_num}")

        try:
            val = socket_test.gpio_read(self.tcp_socket, pin_num)
            print(f"GPIO_{pin_num} = {val}")
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_gpio_write(self, arg):
        'gpio_write <pin_num> <val>'
        arg_list = arg.split()
        if (len(arg_list) != 2):
            print("Error: Invalid input, needs 2 arguments")
            return
        pin_num = int(arg_list[0], 0)
        val = int(arg_list[1], 0)
        print(f"Pin Num: {pin_num}")
        print(f"Val: {val}")

        try:
            socket_test.gpio_write(self.tcp_socket, pin_num, val)
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_adc_read(self, arg):
        'adc_read <pin_num>'
        arg_list = arg.split()
        if (len(arg_list) != 1):
            print("Error: Invalid input, needs 1 arguments")
            return
        pin_num = int(arg_list[0], 0)
        print(f"Pin Num: {pin_num}")

        try:
            voltage = socket_test.adc_read(self.tcp_socket, pin_num)
            print(f"GPIO_{pin_num} = {voltage}mV")
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_battery_get_soc(self, arg):
        'battery_get_soc'
        try:
            soc = socket_test.battery_get_soc(self.tcp_socket)
            print(f"Battery Voltage = {soc}%")
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_battery_get_voltage(self, arg):
        'battery_get_voltage'
        try:
            voltage = socket_test.battery_get_voltage(self.tcp_socket)
            print(f"Battery Voltage = {voltage}mV")
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_exp_gpio_read(self, arg):
        'exp_gpio_read'
        try:
            resp = socket_test.exp_gpio_read(self.tcp_socket)
            print(f"Resp: {resp}")
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

            
    def do_exp_gpio_write(self, arg):
        'exp_gpio_write <bitmask>'
        arg_list = arg.split()
        if (len(arg_list) != 1):
            print("Error: Invalid input, needs 3 arguments")
            return
        bitmask = int(arg_list[0], 0)
        print(f"Bitmask: {hex(bitmask)}")

        try:
            socket_test.exp_gpio_write(self.tcp_socket, bitmask)
        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

    def do_exp_gpio_interrupt_sequence(self, arg):
        'exp_gpio_interrupt_sequence'
        dev_addr = 0x43
        try:
            # Perform soft reset (0x01)
            payload = bytearray()
            payload.append(0x01)
            socket_test.i2c_write(self.tcp_socket, dev_addr, 0x01, payload)

            # Read config register to clear interrupt reset (0x01)
            resp = socket_test.i2c_write_read(self.tcp_socket, dev_addr, 0x01, 1)
            print(f"Device ID: {resp}")

            # Configure IO direction (0x03) 
            payload = bytearray()
            payload.append(0x08)
            socket_test.i2c_write(self.tcp_socket, dev_addr, 0x03, payload)

            # Configure output ports (0x05): Set the output LOW
            payload = bytearray()
            payload.append(0x00)
            socket_test.i2c_write(self.tcp_socket, dev_addr, 0x05, payload)

            # Set high impedance outputs (0x07)
            payload = bytearray()
            payload.append(0xF7)
            socket_test.i2c_write(self.tcp_socket, dev_addr, 0x07, payload)

            # Set pullup resistors (0x0B): Disable pullup/pulldowns
            payload = bytearray()
            payload.append(0x00)
            socket_test.i2c_write(self.tcp_socket, dev_addr, 0x0B, payload)

            # Pullup/Pulldown sel not needed

            # Set default input state (0x09) (keep as 0)
            payload = bytearray()
            payload.append(0x08)
            socket_test.i2c_write(self.tcp_socket, dev_addr, 0x03, payload)

            # Set interrupt mask register (0x11)
            payload = bytearray()
            payload.append(0xEF)
            socket_test.i2c_write(self.tcp_socket, dev_addr, 0x11, payload)

            # Read interrupt status register to clear interrupt
            resp = socket_test.i2c_write_read(self.tcp_socket, dev_addr, 0x01, 1)
            print(f"Interrupt Status: {resp}")
            resp = socket_test.i2c_write_read(self.tcp_socket, dev_addr, 0x01, 1)
            print(f"Interrupt Status: {resp}")

            # ----------------------------

            print("Sleeping for 2 seconds")
            time.sleep(2)
            while True:
                val = input("(0: to exit, 1: continue) >> ")
                if val == '0':
                    break

                # drive pin 3 HIGH
                print("Driving pin 3 HIGH")
                socket_test.exp_gpio_write(self.tcp_socket, 0x08)
                time.sleep(0.005)

                # Read interrupt status, and clear
                resp = socket_test.i2c_write_read(self.tcp_socket, dev_addr, 0x13, 1)
                print(f"Interrupt Status: {resp}")

                # Read input status
                resp = socket_test.i2c_write_read(self.tcp_socket, dev_addr, 0x0F, 1)
                print(f"Input Status: {resp}")

                # invert default state
                print("Setting default interrupt state HIGH")
                payload = bytearray()
                payload.append(0x10)
                socket_test.i2c_write(self.tcp_socket, dev_addr, 0x09, payload)

                #time.sleep(0.5)


                # drive pin 3 LOW
                print("Driving pin 3 LOW")
                socket_test.exp_gpio_write(self.tcp_socket, 0x00)
                time.sleep(0.005)

                # Read interrupt status, and clear
                resp = socket_test.i2c_write_read(self.tcp_socket, dev_addr, 0x13, 1)
                print(f"Interrupt Status: {resp}")

                # Read input status
                resp = socket_test.i2c_write_read(self.tcp_socket, dev_addr, 0x0F, 1)
                print(f"Input Status: {resp}")

                # invert default state
                print("Setting default interrupt state LOW")
                payload = bytearray()
                payload.append(0x00)
                socket_test.i2c_write(self.tcp_socket, dev_addr, 0x09, payload)

                time.sleep(0.5)

        except Exception as e:
            print(f"Error: {e} - ({type(e).__name__})")

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
    parser = argparse.ArgumentParser(description="Command interface to Daftpunk Speaker")
    parser.add_argument("--ip", type=str, required=True)
    args = parser.parse_args()

    if socket_test.check_ip(args.ip) == False:
        print(f"Error: Invalid IP address ({args.ip})")
        sys.exit(-1)

    TCPShell.HOST = args.ip
    print(f"Attempting to connect to device at IP: {TCPShell.HOST}...")
    TCPShell.tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    TCPShell.tcp_socket.connect((TCPShell.HOST, TCPShell.PORT))
    TCPShell().cmdloop()