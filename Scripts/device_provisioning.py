import yaml
from pathlib import Path
import socket
import socket_test
import sys
import os
import argparse

PORT = 3333

scripts_dir = Path(os.path.dirname(os.path.realpath(__file__)))
asset_dir_path = scripts_dir / ".." / "Assets"
asset_info_path = asset_dir_path / 'asset_info.yaml'

def flash_audio_asset(tcp_socket, audio_data):
    if (audio_data['audio_id'] < 0 or audio_data['audio_id'] >= 8):
        return -1
    abs_path = asset_dir_path / "RAW" / audio_data['filename']
    file = open(abs_path, "rb")
    file_bytes = file.read()
    file.close()
    try:
        socket_test.send_audio_data(tcp_socket, audio_data['audio_id'],  audio_data['filename_dev'], file_bytes)
    except Exception as e:
        print(f"Error: {e} - ({type(e).__name__})")
        return -1
    return 0

def flash_audio_asset_info(tcp_socket, mock):
    for key in prime_service['audio_asset_mapping'].keys():
        audio_data = prime_service['audio_asset_mapping'][key]
        
        if (audio_data['active']):
            print(key)
            print("-------------------------")
            print("audio_id:", audio_data['audio_id'])
            print("filename:", audio_data['filename'])
            print("filename_dev:", audio_data['filename_dev'])
            
            if (mock):
                print()
                continue

            ret = flash_audio_asset(tcp_socket, audio_data)
            if (ret < 0):
                return ret
        print()
    return 0

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Provisioning Script for Daftpunk Speaker")
    parser.add_argument("--ip", type=str, required=True)
    args = parser.parse_args()

    if socket_test.check_ip(args.ip) == False:
        print(f"Error: Invalid IP address ({args.ip})")
        sys.exit(-1)

    with open(asset_info_path, 'r') as file:
        prime_service = yaml.safe_load(file)

    HOST = args.ip

    tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_socket.connect((HOST, PORT))

    # Basic connection check before attempting writing to flash, exit if fails
    print("Testing device connection...")
    connect_string = "testing 123"
    resp = socket_test.send_message(tcp_socket, socket_test.MessageID.ECHO, bytes(connect_string, "utf-8"), True)
    if (not resp):
        print("Error: Could not make connection with device")
        sys.exit()
    resp_string = resp.payload.decode()
    if (resp_string != connect_string):
        print(f"Error: Connection String ('{connect_string}') and response ('{resp_string}') do not match")
        sys.exit()
    print("Connection with the device was successful!\n")

    print("Printing audio asset info...")
    if (flash_audio_asset_info(tcp_socket, mock=True) < 0):
        print("Error: Failed to print asset information")
        sys.exit()

    # Get user confirmation to flash device
    confirm = input("Before flashing device, please confirm if the above information is correct? (Yes/No): ")
    if (confirm.lower() not in ['y', 'yes']):
        print("Exiting script...")
        sys.exit()

    if (flash_audio_asset_info(tcp_socket, mock=False) < 0):
        print("Error: Failed to flash audio assets")
        sys.exit()
    tcp_socket.close()
