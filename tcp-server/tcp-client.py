#!/usr/bin/python3
#-*- encoding: utf-8 -*-
import os
import sys
import time
import array
import socket
import hashlib
from enum import Enum

host = "esp32.local" # mDNS hostname
port = 9876
in_hash_md5 = hashlib.md5()
out_hash_md5 = hashlib.md5()
HEADER_SIZE = 128

def usage(name):
	print("usage python3 {} path_to_host [port]".format(name))

def send_command(client, command):
	# Send packet length
	length = len(command)
	bytes = length.to_bytes(2, 'big')
	client.send(array.array('B', bytes))

	# Send packet body
	client.send(command)

	# Receive response
	response = str(client.recv(2), 'utf-8')
	print("response={}".format(response))
	return response

def send_data(client, data, datalen):
	# Send packet length
	bytes = datalen.to_bytes(2, 'big')
	client.send(array.array('B', bytes))

	# Send packet body
	data_array = array.array('B', list(data))
	client.send(data_array)

	# Receive response
	response = str(client.recv(2), 'utf-8')
	print("response={}".format(response))
	return response

def send_header(client, filesize, block_size):
	# Send packet length
	bytes = block_size.to_bytes(2, 'big')
	client.send(array.array('B', bytes))

	# Send packet body
	header="header"+",,"+filesize
	header=bytearray(header,"utf-8")
	header.extend(b','*(block_size-len(header)))
	client.send(header)

	# Receive response
	response = str(client.recv(2), 'utf-8')
	print("response={}".format(response))
	return response

def send_tailer(client, hash, block_size):
	# Send packet length
	bytes = block_size.to_bytes(2, 'big')
	client.send(array.array('B', bytes))

	# Send packet body
	tailer="tailer"+",,"+hash
	tailer=bytearray(tailer,"utf-8")
	tailer.extend(b','*(block_size-len(tailer)))
	client.send(tailer)

	# Receive response
	response = str(client.recv(2), 'utf-8')
	print("response={}".format(response))
	return response

def send_ok(client):
	# Send packet body
	payload="OK"
	payload=bytearray(payload, "utf-8")
	client.send(payload)

# Send file to Server
def put_file(client, filename):
	command = "put_file,{}".format(filename)
	command = bytes(command, 'utf-8')
	response = send_command(client, command)
	if response != "OK":
		return

	file_stats = os.stat(filename)
	print("File Size in Bytes is {}".format(file_stats.st_size))
	response = send_header(client, str(file_stats.st_size), HEADER_SIZE)
	if response != "OK":
		return

	size = 2048
	with open(filename, 'rb') as f:
		while True:
			data = f.read(size)
			print("read len(data)={}".format(len(data)))
			if len(data) == 0:
				break
			out_hash_md5.update(data)
			#print(type(array.array('B', [len(data)])))
			#print(type(list(data)))
			#print(type(array.array('B', list(data))))
			response = send_data(client, data, len(data))

	response = send_tailer(client, out_hash_md5.hexdigest(), HEADER_SIZE)

# Compress file on server
def compress(client, filename):
	command = "compress,{}".format(filename)
	command = bytes(command, 'utf-8')
	response = send_command(client, command)
	if response != "OK":
		return

MessageType = Enum('MessageType', ['HEADER', 'TAILER', 'DATA'])

def process_message(msg):
	""" This is the main receiver code
	"""
	# print("process_message len(msg)={}".format(len(msg)))
	if len(msg) == HEADER_SIZE: #is header or tailer
		# print("process_message msg=[{}]".format(msg))
		msg_in=msg.decode("utf-8")
		msg_in=msg_in.split(",,")
		if msg_in[0]=="header":
			# print("msg_in[1]=[{}]".format(msg_in[1]))
			return MessageType.HEADER.value
		elif msg_in[0]=="tailer":
			return MessageType.TAILER.value
		else:
			return MessageType.DATA.value
	else:
		return MessageType.DATA.value

# Receive file from Server
def get_file(client, filename):
	command = "get_file,{}".format(filename)
	command = bytes(command, 'utf-8')
	response = send_command(client, command)
	if response != "OK":
		return
	
	while True:
		# Receive packet length
		payload = client.recv(2)
		# print("payload={}".format(payload))
		# print("len={}".format(len(payload)))
		if (len(payload) != 2):
			print("Illegal packet length")
			print("payload={}".format(payload))
			print("payload length={}".format(len(payload)))
			break
		remain_length = int.from_bytes(payload, 'big')
		print("remain_length={}".format(remain_length))

		# Receive packet body
		payload = b''
		while True:
			chunk = client.recv(remain_length)
			payload = payload + chunk
			remain_length = remain_length - len(chunk)
			# print("len(chunk)={}".format(len(chunk)))
			# print("len(payload)={}".format(len(payload)))
			# print("remain_length={}".format(remain_length))
			if (remain_length == 0):
				break;

		# Determine message type
		message_type = process_message(payload)
		print("message_type={}".format(message_type))

		# Open file
		if (message_type == MessageType.HEADER.value):
			print(payload)
			msg_in = str(payload, 'utf-8')
			msg_in=msg_in.split(",,")
			print(msg_in)
			fout = open(filename, "wb")
			start_time = time.time()
			file_size = 0
			send_ok(client)

		# Close file
		elif message_type == MessageType.TAILER.value:
			print(payload)
			msg_in = str(payload, 'utf-8')
			msg_in=msg_in.split(",,")
			print(msg_in)
			out_hash = msg_in[1]
			fout.close()
			in_hash=in_hash_md5.hexdigest()
			print("file_size={}".format(file_size))
			print("in_hash=[{}]".format(in_hash))
			print("out_hash=[{}]".format(out_hash))
			if in_hash == out_hash:
				print("File copied OK - valid hash")
			else:
				print("File copied NG - invalid hash")
			time_taken = time.time() - start_time
			print("took ",time_taken)
			send_ok(client)
			break

		# Write file
		else:
			in_hash_md5.update(payload)
			fout.write(payload)
			file_size = file_size + len(payload)
			send_ok(client)

# Delete file from Server
def del_file(client, filename):
	command = "del_file,{}".format(filename)
	command = bytes(command, 'utf-8')
	response = send_command(client, command)
	if response != "OK":
		return

def main():
	argv = sys.argv
	args = len(argv)
	# print("args={}".format(args))
	if args == 1:
		usage(argv[0])
		exit()

	client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	_port = port
	if args == 3:
		_port = int(argv[2])
	print("Connect to {}".format(_port))
	client.connect((host, _port))

	put_file(client, argv[1])
	compress(client, argv[1])
	get_file(client, argv[1]+".zlib")
	del_file(client, argv[1])
	del_file(client, argv[1]+".zlib")
	client.close()


if __name__ == "__main__": 
	main() 
