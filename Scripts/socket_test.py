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
    
    # Loop and send data
    bytes_remaining = len(payload)
    chunks = bytes_remaining // PACKET_SIZE
    if (bytes_remaining % PACKET_SIZE > 0):
        chunks += 1

    print(f"Transferring {bytes_remaining} bytes from {file_path} to device")
    with alive_progress.alive_bar(chunks) as bar:
        while (bytes_remaining > 0):
            start = len(payload) - bytes_remaining
            if (start + PACKET_SIZE < len(payload)):
                end = start + PACKET_SIZE
            else:
                end = start + bytes_remaining
            #print(f"Bytes_Remaining: {bytes_remaining}, start: {start}, end: {end}")
            #print(f"len: {len(buffer[start:end])}")
            bytes_remaining -= (end - start)

            #s.sendall(payload[start:end])
            #data = s.recv(1024)

            resp = send_message(sock, MessageID.NVM_SEND_DATA, payload[start:end], True)
            #if (resp):
            #    print(resp)
            percent_complete = 100*(len(payload) - bytes_remaining) / len(payload)
            #print(f"{int(percent_complete)}")
            bar()
        

    # Send End command
    resp = send_message(sock, MessageID.NVM_STOP, None, True)
    if (resp):
        print(resp)
        print()

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




