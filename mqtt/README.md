# mqtt
Demonstration of compression using MQTT.   
I found [this](http://www.steves-internet-guide.com/send-file-mqtt/) article.   
It's great to be able to exchange files using MQTT.   
Since the python code was publicly available, I ported it to esp-idf.   

# Installation

```
git clone https://github.com/nopnop2002/esp-idf-zlib
cd esp-idf-zlib/mqtt
idf.py menuconfig
idf.py flash
```

# Configuration   

![config-top](https://github.com/nopnop2002/esp-idf-zlib/assets/6020549/571c3698-9ce6-48ad-a009-6b01b836d608)
![config-app](https://github.com/nopnop2002/esp-idf-zlib/assets/6020549/8c10e256-22f2-4fea-9855-be162822097e)

## WiFi Setting
Set the information of your access point.   

![config-wifi](https://github.com/nopnop2002/esp-idf-zlib/assets/6020549/d5ed8120-7fec-4b9c-b46e-eab16261c8f8)

## Broker Setting

MQTT broker is specified by one of the following.
- IP address   
 ```192.168.10.20```   
- mDNS host name   
 ```mqtt-broker.local```   
- Fully Qualified Domain Name   
 ```broker.emqx.io```

You can download the MQTT broker from [here](https://github.com/nopnop2002/esp-idf-mqtt-broker).   

![config-broker-1](https://github.com/nopnop2002/esp-idf-zlib/assets/6020549/29c1f5a1-3b39-4db5-965f-5a35f171da2b)

Specifies the username and password if the server requires a password when connecting.   
[Here's](https://www.digitalocean.com/community/tutorials/how-to-install-and-secure-the-mosquitto-mqtt-messaging-broker-on-debian-10) how to install and secure the Mosquitto MQTT messaging broker on Debian 10.   

![config-broker-2](https://github.com/nopnop2002/esp-idf-zlib/assets/6020549/a47119d4-d591-40ae-9f73-cfea7c71d333)


# How to use

Run the following python script on the host side.   
```
$ python3 -m pip install paho-mqtt

Default Broker is broker.emqx.io.   
You can specify a different Broker at startup.   

$ python3 mqtt-file.py path_to_host [broker]
- Send source file to ESP32 using mqtt
- Compress source file using zlib
- Receiving compressed files from ESP32 using mqtt
- Delete source files from ESP32
- Delete compressed files from ESP32

$ ls -l *.zlib
```

When the file on the host side is test.txt, a compressed file of test.txt.zlib is created.   


# MQTT Topic
This project uses the following topics:
```
MQTT_PUT_REQUEST="/mqtt/files/put/req"
MQTT_GET_REQUEST="/mqtt/files/get/req"
MQTT_LIST_REQUEST="/mqtt/files/list/req"
MQTT_DELETE_REQUEST="/mqtt/files/delete/req"

MQTT_PUT_RESPONSE="/mqtt/files/put/res"
MQTT_GET_RESPONSE="/mqtt/files/get/res"
MQTT_LIST_RESPONSE="/mqtt/files/list/res"
MQTT_DELETE_RESPONSE="/mqtt/files/delete/res"
```

When using public brokers, these topics may be used for other purposes.   
If you want to change the topic to your own, you will need to change both the ESP32 side and the python side.   
You can use [this](https://github.com/nopnop2002/esp-idf-mqtt-broker) as your personal Broker.   
