# http-server   
Demonstration of compression and decompression using HTTP Server.   
This project is based on [this](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/file_serving) example.   

![http-file-server](https://github.com/nopnop2002/esp-idf-zlib/assets/6020549/e80f51ef-cadc-4fef-8bad-e731a05a61cf)

# Installation

```
git clone https://github.com/nopnop2002/esp-idf-zlib
cd esp-idf-zlib/http-server
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


# Partition Table
Define Partition Table in partitions_example.csv.   
When your SoC has 4M Flash, you can expand the partition.   
The total maximum partition size for a SoC with 4M Flash is 0x2F0000(=3,008K).   
storage0 is the storage area for files before compression.   
And storage1 is the storage area for files after compression.   
```
# Name,   Type, SubType, Offset,  Size, Flags
# Note: if you have increased the bootloader size, make sure to update the offsets to avoid overlap
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 1M,
storage0, data, spiffs,  ,        384K,
storage1, data, spiffs,  ,        384K,
```
