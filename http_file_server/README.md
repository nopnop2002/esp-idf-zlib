# http_file_server   
Demonstration of compression and decompression using HTTP Server.   
This project is based on [this](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/file_serving) sample.   

![http-file-server](https://github.com/nopnop2002/esp-idf-zlib/assets/6020549/915d930d-2651-4277-b8bb-1b110fd4bb45)

# Installation

```
git clone https://github.com/nopnop2002/esp-idf-zlib
cd esp-idf-zlib/http_file_server
idf.py menuconfig
idf.py flash
```

# Configuration
![config-app-1](https://github.com/nopnop2002/esp-idf-zlib/assets/6020549/ab1b777a-2ea0-4d75-a22d-59027a4c54cc)

You can use staic IP address.   
![config-app-2](https://github.com/nopnop2002/esp-idf-zlib/assets/6020549/defd2b59-6ccf-41c4-adde-10532b74de34)

You can connect using the mDNS hostname instead of the IP address.   
![config-app-3](https://github.com/nopnop2002/esp-idf-zlib/assets/6020549/0a7fc562-0f00-4d0c-a077-f968643be074)


# How to use
ESP32 acts as a web server.   
Enter the following in the address bar of your web browser.   
```
http:://{IP of ESP32}/
or
http://esp32-server.local/
```

Drag one file to drop zone and push Compress button.   
