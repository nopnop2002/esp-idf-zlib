#! python3.4
###demo code provided by Steve Cope at www.steves-internet-guide.com
##email steve@steves-internet-guide.com
###Free to use for any purpose
"""
Send File Using MQTT
"""
import os
import sys
import time
import paho.mqtt.client as paho
import hashlib
from enum import Enum

MessageType = Enum('MessageType', ['HEADER', 'TAILER', 'DATA'])

#broker="broker.hivemq.com"
#broker="iot.eclipse.org"
broker="broker.emqx.io"

header_block_size=128
data_block_size=512
#data_block_size=1024
global outfile
global fout
global start_time
global run_flag
global topic_counter
global topic_subscribe

#Change to enable when using password authentication
#username = 'user'
#password = 'password'

client= paho.Client("client-001")
mqtt_qos=1

#Change to enable when using password authentication
#client.username_pw_set(username, password=password)

in_hash_md5 = hashlib.md5()
out_hash_md5 = hashlib.md5()

MQTT_PUT_REQUEST="/mqtt/files/put/req"
MQTT_GET_REQUEST="/mqtt/files/get/req"
MQTT_LIST_REQUEST="/mqtt/files/list/req"
MQTT_DELETE_REQUEST="/mqtt/files/delete/req"

MQTT_PUT_RESPONSE="/mqtt/files/put/res"
MQTT_GET_RESPONSE="/mqtt/files/get/res"
MQTT_LIST_RESPONSE="/mqtt/files/list/res"
MQTT_DELETE_RESPONSE="/mqtt/files/delete/res"

def process_message(msg):
	""" This is the main receiver code
	"""
	# print("process_message len(msg)={}".format(len(msg)))
	if len(msg)==header_block_size: #is header or tailer
		# print("process_message msg=[{}]".format(msg))
		msg_in=msg.decode("utf-8")
		msg_in=msg_in.split(",,")
		if msg_in[0]=="header":
			# print("msg_in[1]=[{}]".format(msg_in[1]))
			return MessageType.HEADER.value, msg_in[1], msg_in[2]
		elif msg_in[0]=="tailer":
			return MessageType.TAILER.value, msg_in[1], msg_in[2]
		else:
			return MessageType.DATA.value, "", ""
	else:
		return MessageType.DATA.value, "", ""

#mqtt callback
def on_message(client, userdata, message):
	global topic_counter
	global topic_subscribe
	print("on_message message.topic[{}]=[{}] {}".format(topic_counter,message.topic,len(message.payload)))
	topic_counter = topic_counter + 1
	#print("on_message message.payload={}".format(message.payload))
	#print("on_message len(message)={}".format(len(message.payload)))
	topic_subscribe = 1

	global outfile
	global fout
	global run_flag
	global start_time
	time.sleep(1)
	type, filename, out_hash = process_message(message.payload)
	# print("type={} filename=[{}] out_hash=[{}]".format(type, filename, out_hash))
	if type == MessageType.HEADER.value:
		if message.topic == MQTT_PUT_RESPONSE:
			print("PUT Responce={}".format(out_hash))
			run_flag=False

		if message.topic == MQTT_GET_RESPONSE:
			#print("filename=[{}]".format(filename))
			#print("out_hash=[{}]".format(out_hash))
			if out_hash=="ACK":
				fout=open(filename,"wb")
				outfile = filename
				start_time = time.time()
			else:
				fout=None

		if message.topic == MQTT_DELETE_RESPONSE:
			print("DELETE Responce={}".format(out_hash))
			run_flag=False

	elif type == MessageType.TAILER.value:
		if message.topic == MQTT_GET_RESPONSE:
			#print("fout={}".format(fout))
			if fout!=None:
				fout.close()
				outfile = None
				in_hash=in_hash_md5.hexdigest()
				print("in_hash=[{}]".format(in_hash))
				print("out_hash=[{}]".format(out_hash))
				if in_hash==out_hash:
					print("File copied OK - valid hash")
				else:
					print("File copied NG - invalid hash")
				time_taken=time.time()-start_time
				print("took ",time_taken)
			else:
				print("File copied NG - spiffs has no files")
			run_flag=False

		if message.topic == MQTT_LIST_RESPONSE:
			run_flag=False

		if message.topic == MQTT_DELETE_RESPONSE:
			run_flag=False

	elif type == MessageType.DATA.value:
		if message.topic == MQTT_GET_RESPONSE:
			in_hash_md5.update(message.payload)
			fout.write(message.payload)

		if message.topic == MQTT_LIST_RESPONSE:
			payload=message.payload.decode('utf-8')
			print("filename={}".format(payload))

def on_publish(client, userdata, mid):
	#logging.debug("pub ack "+ str(mid))
	client.mid_value=mid
	client.puback_flag=True

def wait_for(client,msgType,period=0.25,wait_time=40,running_loop=False):
	client.running_loop=running_loop #if using external loop
	wcount=0  
	while True:
		#print("waiting"+ msgType)
		if msgType=="PUBACK":
			if client.on_publish:		  
				if client.puback_flag:
					return True
	  
		if not client.running_loop:
			client.loop(.01)	#check for messages manually
		time.sleep(period)
		#print("loop flag ",client.running_loop)
		wcount+=1
		if wcount>wait_time:
			print("return from wait loop taken too long")
			return False
	return True 

def send_header(topic, filename):
	header="header"+",,"+filename+",,"
	header=bytearray(header,"utf-8")
	header.extend(b','*(header_block_size-len(header)))
	#print(header)
	c_publish(client,topic,header,mqtt_qos)

def send_tailer(topic, filename, hash):
	#tailer="tailer"+",,"+filename+",,"+out_hash_md5.hexdigest()
	tailer="tailer"+",,"+filename+",,"+hash
	tailer=bytearray(tailer,"utf-8")
	tailer.extend(b','*(header_block_size-len(tailer)))
	#print(tailer)
	c_publish(client,topic,tailer,mqtt_qos)

def c_publish(client,topic,out_message,mqtt_qos):
	#print("out_message={}".format(len(out_message)))
	res,mid=client.publish(topic,out_message,mqtt_qos)#publish
	if res==0: #published ok
		if wait_for(client,"PUBACK",running_loop=True):
			if mid==client.mid_value:
				print("match mid ",str(mid))
				client.puback_flag=False #reset flag
			else:
				raise SystemExit("not got correct puback mid so quitting")
			
		else:
			raise SystemExit("not got puback so quitting")


def wait_for_responce(timeout):
	global run_flag
	global topic_subscribe
	start=time.time()
	run_flag=True
	while run_flag:
		time_taken=time.time()-start
		#print("time_taken={}".format(time_taken))
		if time_taken > timeout:
			break
		print("topic_subscribe={}".format(topic_subscribe))
		if topic_subscribe == 1:
			start=time.time()
		topic_subscribe = 0
		time.sleep(1)

def usage(name):
	print("usage python3 {} path_to_host".format(name))

def main():
	global run_flag
	global topic_subscribe
	argv = sys.argv
	args = len(argv)
	#print("args={}".format(args))
	if args != 2:
		usage(argv[0])
		exit()
	if os.path.isfile(argv[1]) is False:
		print("{} not exist".format(argv[1]))
		exit()

	src_file = argv[1]
	dst_file = "{}.dfl".format(argv[1])
	print("src_file={} dst_file={}".format(src_file, dst_file))

	print("connecting to broker ",broker)
	client.on_message=on_message
	client.on_publish=on_publish
	client.puback_flag=False #use flag in publish ack
	client.mid_value=None
	client.connect(broker)#connect
	client.loop_start() #start loop to process received messages
	global topic_counter
	topic_counter = 0
	topic_subscribe = 0

	pub_topic=MQTT_PUT_REQUEST
	sub_topic=MQTT_PUT_RESPONSE
	client.subscribe(sub_topic)#subscribe
	in_name=src_file #file to input
	in_file=open(in_name,"rb")
	send_header(pub_topic, in_name)
	start=time.time()
	while True:
		chunk=in_file.read(data_block_size) # change if want smaller or larger data blcoks
		if chunk:
			out_hash_md5.update(chunk)
			out_message=chunk
			c_publish(client,pub_topic,out_message,mqtt_qos)
			
		else:
			#send hash
			send_tailer(pub_topic, in_name, out_hash_md5.hexdigest())
			break
	in_file.close()
	time_taken=time.time()-start
	print("took ",time_taken)
	wait_for_responce(30)

	pub_topic=MQTT_GET_REQUEST
	sub_topic=MQTT_GET_RESPONSE
	client.subscribe(sub_topic)#subscribe
	send_header(pub_topic, dst_file)
	wait_for_responce(30)

	pub_topic=MQTT_DELETE_REQUEST
	sub_topic=MQTT_DELETE_RESPONSE
	client.subscribe(sub_topic)#subscribe
	send_header(pub_topic, src_file)
	wait_for_responce(30)

	client.subscribe(sub_topic)#subscribe
	send_header(pub_topic, dst_file)
	wait_for_responce(30)

	#time.sleep(4)
	client.disconnect() #disconnect
	client.loop_stop() #stop loop

if __name__ == "__main__": 
	main() 
