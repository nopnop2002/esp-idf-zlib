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

\*There is no MENU ITEM where this application is peculiar.   

# Compress   
This project uses the FAT file system.   
Compress 10 bytes of data 10 times and write it to a file.
The total size of the data is 150 bytes, but the file size on the FAT file system is 32 bytes.   


# UnCompress   
Reads ZIP files on FAT file systems.   
The total read size is 150 bytes.   
