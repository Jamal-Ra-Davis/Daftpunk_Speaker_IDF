import socket
import time
import struct
import sys
import crc16
import cmd
from dataclasses import dataclass
from enum import IntEnum
import alive_progress

DEBUG_ENABLE = False
MAX_UINT16 = (2**16 - 1)
MAX_UINT8 = (2**8 -1)

def check_ip(ip):
    ip_split = ip.split('.')
    if len(ip_split) != 4:
        return False
    for i in ip_split:
        try:
            val = int(i)
            if (val > 255):
                return False
        except:
            return False
    return True

@dataclass
class TCPMessage:
    message_id: int
    payload_size: int
    response_expected: bool
    crc_header: int
    crc_payload: int
    payload: bytearray

class MessageID(IntEnum):
    ACK = 0
    NACK = 1
    TEST = 2
    ECHO = 3
    NVM_START = 4
    NVM_SEND_DATA = 5
    NVM_STOP = 6
    NVM_ERASE_CHIP = 7
    AUDIO_LOAD_START = 8
    AUDIO_META_DATA = 9
    PLAY_AUDIO_ASSET = 10
    STACK_INFO = 11
    I2C_BUS_SCAN = 12
    I2C_WRITE = 13
    I2C_WRITE_READ = 14
    MEM_READ_SCRATCH = 15
    MEM_READ = 16
    MEM_WRITE = 17
    GPIO_SET_CONFIG = 18
    GPIO_READ = 19
    GPIO_WRITE = 20
    ADC_READ = 21
    BATT_GET_SOC = 22
    BATT_GET_VOLTAGE = 23

class PinMode(IntEnum):
    GPIO_MODE_DISABLE = 0
    GPIO_MODE_INPUT = 1
    GPIO_MODE_OUTPUT = 2
    GPIO_MODE_OUTPUT_OD = 6
    GPIO_MODE_INPUT_OUTPUT_OD = 7
    GPIO_MODE_INPUT_OUTPUT = 3

class PinStrap(IntEnum):
    PIN_STRAP_DISABLE = 0
    PIN_STRAP_PULLUP = 1
    PIN_STRAP_PULLDOWN = 2

def string_to_bytearray(byte_list):
    out = bytearray()
    for element in byte_list:
        val = int(element, 16)
        out.append(val)
    return out

def simple_crc16(start, buf):
    if (buf == None): 
        return 0
    
    data = [start, start]
    for i in range(len(buf)):
        data[0] ^= buf[i] & 0xFF
        data[1] ^= ((buf[i] << 4) & 0xFF)
    return (data[0] | (data[1] << 8))

def create_packet(message_id, payload, response_expected):
    global DEBUG_ENABLE
    payload_size = 0
    crc_payload = 0
    if (payload):
        payload_size = len(payload)
        crc_payload = crc_header = simple_crc16(0, payload)
    
    header_bytes = struct.pack("<HH?", int(message_id), payload_size, response_expected)

    crc_header = simple_crc16(0, header_bytes)
    crc_bytes = struct.pack("<HH", crc_header, crc_payload)
    
    message = header_bytes + crc_bytes
    if (payload):
        message += payload

    if (DEBUG_ENABLE):
        print("Message:", message)
        print("Header:", header_bytes)
        print("CRC16 header:", hex(crc_header))
        print("CRC16 payload:", hex(crc_payload))
        print("Payload", payload)
    return message

def decode_packet(data):
    global DEBUG_ENABLE
    header = data[:9]
    payload = data[9:]

    if (DEBUG_ENABLE):
        print("Header:", header)
        print("Payload:", payload)
    message_id, payload_size, response_expected, crc_header, crc_payload = struct.unpack('<HH?HH', header)
    msg = TCPMessage(message_id, payload_size, response_expected, crc_header, crc_payload, payload)
    return msg

def send_message(sock, id: MessageID, payload, response_expected: bool):
    sock.sendall(create_packet(id, payload, response_expected))
    if (response_expected):
        msg = decode_packet(sock.recv(1024))
        return msg
    return None

def send_nvm_data_loop(sock, payload):
    PACKET_SIZE = 1024#256
    
    # Loop and send data
    bytes_remaining = len(payload)
    chunks = bytes_remaining // PACKET_SIZE
    if (bytes_remaining % PACKET_SIZE > 0):
        chunks += 1

    with alive_progress.alive_bar(chunks) as bar:
        while (bytes_remaining > 0):
            start = len(payload) - bytes_remaining
            if (start + PACKET_SIZE < len(payload)):
                end = start + PACKET_SIZE
            else:
                end = start + bytes_remaining
            bytes_remaining -= (end - start)

            resp = send_message(sock, MessageID.NVM_SEND_DATA, payload[start:end], True)
            #percent_complete = 100*(len(payload) - bytes_remaining) / len(payload)
            bar()

    # Send End command
    resp = send_message(sock, MessageID.NVM_STOP, None, True)
    if (resp):
        print(resp)
        print()

def send_nvm_data(sock, file_path, payload):
    PACKET_SIZE = 1024#256

    print("Payload:", payload[:256])

    # Send start command
    path_bytes = bytes(file_path, "utf-8")
    path_len = len(path_bytes)
    message_payload = struct.pack("<LH", len(payload), path_len)
    message_payload += path_bytes
    resp = send_message(sock, MessageID.NVM_START, message_payload, True)
    if (resp):
        print(resp)
    
    print(f"Transferring {len(payload)} bytes from {file_path} to device")
    
    send_nvm_data_loop(sock, payload)
        
def send_audio_data(sock, audio_id, filename_dev, payload):
    if (isinstance(audio_id, int) == False):
        raise Exception(f"asset_id ({audio_id}), must be an integer")
    if (audio_id < 0 or audio_id >= 8):
        raise Exception(f"Invalid asset_id ({audio_id}), must be an integer between 0 and 7")
    
    print("Payload:", payload[:256])

    # Send start command
    filename_bytes = bytes(filename_dev, "utf-8")
    filename_len = len(filename_bytes)
    message_payload = struct.pack("<LBH", len(payload), audio_id, filename_len)
    message_payload += filename_bytes
    resp = send_message(sock, MessageID.AUDIO_LOAD_START, message_payload, True)
    if (resp):
        print(resp)
    if (resp.message_id != MessageID.ACK):
        raise Exception("Device did not ACK back")
    
    send_nvm_data_loop(sock, payload)

def get_audio_metadata(sock):
    resp = send_message(sock, MessageID.AUDIO_META_DATA, None, True)

def play_audio_asset(sock, audio_id):
    if (isinstance(audio_id, int) == False):
        raise Exception(f"asset_id ({audio_id}), must be an integer")
    if (audio_id < 0 or audio_id >= 8):
        raise Exception(f"Invalid asset_id ({audio_id}), must be an integer between 0 and 7")
    
    message_payload = struct.pack("<B", audio_id)
    resp = send_message(sock, MessageID.PLAY_AUDIO_ASSET, message_payload, True)
    if (resp):
        print(resp)
    if (resp.message_id != MessageID.ACK):
        raise Exception("Device did not ACK back")

def get_stack_info(sock):
        resp = send_message(sock, MessageID.STACK_INFO, None, True)
        if (resp.message_id != MessageID.STACK_INFO):
            raise Exception("Invalid response")
        return str(resp.payload.decode())

def i2c_write(sock, dev_addr, reg_addr, payload, timeout=500):
    bus = 0
    if (isinstance(dev_addr, int) == False):
        raise Exception(f"dev_addr ({dev_addr}), must be an integer")
    if (dev_addr < 0 or dev_addr >= 128):
        raise Exception(f"Invalid dev_addr ({dev_addr}), must be an integer between 0 and 127")
    
    if (isinstance(reg_addr, int) == False):
        raise Exception(f"reg_addr ({reg_addr}), must be an integer")
    if (reg_addr < 0 or reg_addr > MAX_UINT8):
        raise Exception(f"Invalid reg_addr ({reg_addr}), must be an integer between 0 and  {MAX_UINT8}")
    
    if (isinstance(timeout, int) == False):
        raise Exception(f"timeout ({timeout}), must be an integer")
    if (timeout < 0 or timeout > MAX_UINT16):
        raise Exception(f"Invalid timeout ({timeout}), must be an integer between 0 and {MAX_UINT16}")
    
    length = len(payload) + 1
    if (length >= MAX_UINT16):
        raise Exception(f"Invalid length ({length}), must be less than or equal to {MAX_UINT16}")
    
    message_payload = struct.pack("<BBHHB", bus, dev_addr, timeout, length, reg_addr)
    message_payload += payload
    resp = send_message(sock, MessageID.I2C_WRITE, message_payload, True)
    if (resp):
        print(resp)
    if (resp.message_id != MessageID.ACK):
        raise Exception("Device did not ACK back")
    
def i2c_write_read(sock, dev_addr, reg_addr, length, timeout=500):
    bus = 0
    if (isinstance(dev_addr, int) == False):
        raise Exception(f"dev_addr ({dev_addr}), must be an integer")
    if (dev_addr < 0 or dev_addr >= 128):
        raise Exception(f"Invalid dev_addr ({dev_addr}), must be an integer between 0 and 127")
    
    if (isinstance(reg_addr, int) == False):
        raise Exception(f"reg_addr ({reg_addr}), must be an integer")
    if (reg_addr < 0 or reg_addr > MAX_UINT8):
        raise Exception(f"Invalid reg_addr ({reg_addr}), must be an integer between 0 and  {MAX_UINT8}")
    
    if (isinstance(timeout, int) == False):
        raise Exception(f"timeout ({timeout}), must be an integer")
    if (timeout < 0 or timeout > MAX_UINT16):
        raise Exception(f"Invalid timeout ({timeout}), must be an integer between 0 and {MAX_UINT16}")
    
    if (length >= MAX_UINT16):
        raise Exception(f"Invalid length ({length}), must be less than or equal to {MAX_UINT16}")
    
    message_payload = struct.pack("<BBHBH", bus, dev_addr, timeout, reg_addr, length)
    resp = send_message(sock, MessageID.I2C_WRITE_READ, message_payload, True)
    if (resp):
        print(resp)

    if (resp.message_id != MessageID.I2C_WRITE_READ):
        raise Exception("Invalid response")

    return resp.payload

def mem_read_scratch(sock):
    resp = send_message(sock, MessageID.MEM_READ_SCRATCH, None, True)
    if (resp):
        print(resp)
    if (resp.message_id != MessageID.MEM_READ_SCRATCH):
        raise Exception("Invalid response")

    address = struct.unpack("<I", resp.payload[:4])[0]
    payload = resp.payload[4:]
    return address, payload

def mem_read(sock, addr):
    if (isinstance(addr, int) == False):
        raise Exception(f"addr ({addr}), must be an integer")
    
    message_payload = struct.pack("<I", addr)
    resp = send_message(sock, MessageID.MEM_READ, message_payload, True)

    if (resp):
        print(resp)

    if (resp.message_id != MessageID.MEM_READ):
        raise Exception("Invalid response")

    return resp.payload

def mem_write(sock, addr, val):
    if (isinstance(addr, int) == False):
        raise Exception(f"addr ({addr}), must be an integer")
    if (isinstance(addr, int) == False):
        raise Exception(f"val ({val}), must be an integer")

    message_payload = struct.pack("<II", addr, val)
    resp = send_message(sock, MessageID.MEM_WRITE, message_payload, True)

    if (resp):
        print(resp)
    if (resp.message_id != MessageID.ACK):
        raise Exception("Device did not ACK back")
    
def gpio_set_config(sock, pin_num, pin_mode, pin_strap, pin_int_type=0):
    if (isinstance(pin_num, int) == False):
        raise Exception(f"pin_num ({pin_num}), must be an integer")
    if (isinstance(pin_mode, int) == False):
        raise Exception(f"pin_mode ({pin_mode}), must be an integer")
    if (isinstance(pin_strap, int) == False):
        raise Exception(f"pin_strap ({pin_strap}), must be an integer")
    if (isinstance(pin_int_type, int) == False):
        raise Exception(f"pin_int_type ({pin_int_type}), must be an integer")

    message_payload = struct.pack("<BBBB", pin_num, pin_mode, pin_strap, pin_int_type)
    resp = send_message(sock, MessageID.GPIO_SET_CONFIG, message_payload, True)

    if (resp):
        print(resp)
    if (resp.message_id != MessageID.ACK):
        raise Exception("Device did not ACK back")

def gpio_read(sock, pin_num):
    if (isinstance(pin_num, int) == False):
        raise Exception(f"pin_num ({pin_num}), must be an integer")
    message_payload = struct.pack("<B", pin_num)
    resp = send_message(sock, MessageID.GPIO_READ, message_payload, True)

    if (resp):
        print(resp)
    if (resp.message_id != MessageID.GPIO_READ):
        raise Exception("Invalid response")
    
    val = struct.unpack("<B", resp.payload)[0]
    return val

def gpio_write(sock, pin_num, val):
    if (isinstance(pin_num, int) == False):
        raise Exception(f"pin_num ({pin_num}), must be an integer")
    if (isinstance(val, int) == False):
        raise Exception(f"val ({val}), must be an integer")

    message_payload = struct.pack("<BB", pin_num, val)
    resp = send_message(sock, MessageID.GPIO_WRITE, message_payload, True)

    if (resp):
        print(resp)
    if (resp.message_id != MessageID.ACK):
        raise Exception("Device did not ACK back")

def adc_read(sock, pin_num):
    if (isinstance(pin_num, int) == False):
        raise Exception(f"pin_num ({pin_num}), must be an integer")
    message_payload = struct.pack("<B", pin_num)
    resp = send_message(sock, MessageID.ADC_READ, message_payload, True)

    if (resp):
        print(resp)
    if (resp.message_id != MessageID.ADC_READ):
        raise Exception("Invalid response")
    
    voltage = struct.unpack("<I", resp.payload)[0]
    return voltage

if __name__ == '__main__':
    HOST = "192.168.0.226"
    PORT = 3333

    buffer = b'.'*3200000

    PACKET_SIZE = 2048 -1

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))
        #s.sendall(b"Hello, world")
        '''
        s.sendall(create_packet(1, b'HELLO World!', False))
        data = s.recv(1024)
        print(f"Received {data!r}\n")
        msg = decode_packet(data)
        print("payload:", msg.payload)

        s.sendall(create_packet(1, b'Goodbye Cruel World!', True))
        data = s.recv(1024)
        print(f"Received {data!r}\n")
        msg = decode_packet(data)
        print("payload:", msg.payload)
        '''
        resp = send_message(s, MessageID.ECHO, b'HELLO World!', True)
        if (resp):
            print(resp, "\n")

        resp = send_message(s, MessageID.TEST, b'Goodbye Cruel World!', True)
        if (resp):
            print(resp, "\n")

        resp = send_message(s, MessageID.ECHO, b'Goodbye Cruel World!', True)
        if (resp):
            print(resp, "\n")
        
        send_nvm_data(s, "/data/nvm/test.txt", buffer[:400])
        '''
        bytes_remaining = len(buffer)
        while (bytes_remaining > 0):
            start = len(buffer) - bytes_remaining
            if (start + PACKET_SIZE < len(buffer)):
                end = start + PACKET_SIZE
            else:
                end = start + bytes_remaining
            #print(f"Bytes_Remaining: {bytes_remaining}, start: {start}, end: {end}")
            #print(f"len: {len(buffer[start:end])}")
            bytes_remaining -= (end - start)

            s.sendall(buffer[start:end])
            data = s.recv(1024)
            print(f"{100*(len(buffer) - bytes_remaining) / len(buffer)}%")
            #print(f"Received {data!r}")
        '''
            
        #for i in range():
        #    s.sendall(buffer[start:end])
        #    data = s.recv(1024)
        #    print(f"Received {data!r}")


    # Raw transport command structure
        # message ID        - 2 bytes
        # Payload size      - 2 bytes
        # Response expected - 1 byte
        # CRC_Header        - 2 bytes
        # CRC_Payload       - 2 bytes
        # Payload

    # Transport packet limit is PACKET_SIZE bytes
    # Max  

    # Commands (reserve last 256 IDs for system commands)
        # ACK
        # NACK
        # I2C read/write/bus_scan
        # Start NVM load (Filename, size)
        # 




