# tcp-server   
Demonstration of compression and decompression using TCP Server.   
This project is based on [this](https://github.com/espressif/esp-idf/tree/master/examples/protocols/sockets/tcp_server) example.   

# Installation

```
git clone https://github.com/nopnop2002/esp-idf-zlib
cd esp-idf-zlib/tcp-server
idf.py menuconfig
idf.py flash
```

# Configuration
![config-top](https://github.com/nopnop2002/esp-idf-zlib/assets/6020549/9ad2472c-2e30-4bb0-a032-7b8a2ba85a3c)
![config-app-1](https://github.com/nopnop2002/esp-idf-zlib/assets/6020549/373385f3-fb27-426e-8976-13a87ae7df64)

You can connect using the mDNS hostname instead of the IP address.   
![config-app-2](https://github.com/nopnop2002/esp-idf-zlib/assets/6020549/9fdaeb76-dcce-49a3-9161-acb345f78795)


# How to use
ESP32 acts as a tcp server.   
```
python3 tcp-client path_to_host [port]
- Send source file to ESP32 using TCP/IP
- Compress source file using zlib
- Receiving compressed files from ESP32 using TCP/IP
- Delete source files from ESP32
- Delete compressed files from ESP32

$ ls -l *.zlib
```
