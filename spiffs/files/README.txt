# esp-idf-zlib
IDF component registry includes zlib for compression and decompression.   
https://components.espressif.com/components/espressif/zlib   

However, unfortunately there is no sample code for ESP-IDF.   
This is example compression and decompression using zlib.   

# Software requirements
ESP-IDF V5.x.   

# Installation

```
git clone https://github.com/nopnop2002/esp-idf-zlib
cd esp-idf-zlib
idf.py menuconfig
idf.py flash
```

__There is no MENU ITEM where this application is peculiar.__   

# Compress   
This project uses the FAT file system.   
Compress 15 bytes of data 10 times and write it to a ZIP file.   
The total size of the data is 150 bytes, but the file size on the FAT file system is 32 bytes.   

# UnCompress   
Reads ZIP files on FAT file systems.   
The total read size is 150 bytes.   

# Screen Shot   
```
I (400) MAIN: Initializing FLASH file system
I (400) MAIN: Mount FAT filesystem on /root
I (400) MAIN: s_wl_handle=0
I (660) MAIN: outSize=150 ---> This is the total data size written
I (660) MAIN: fileSize=32 ---> This is the file size on the FAT file system
I (660) MAIN: buf=[test test test
test test test
test test test
test test test
test]
I (660) MAIN: buf=[ test test
test test test
test test test
test test test
test tes]
I (670) MAIN: buf=[t test
test test test
]
I (670) MAIN: inSize=150 ---> This is the total data size read
I (750) MAIN: FLASH unmounted
```
