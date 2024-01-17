# basic
Demonstration of gzopen/gzread/gzwrite/gzclose.   
This project uses the SPIFFS file system.   
When using the FAT file system, enable the following line.   
```
//#define USE_FAT
```

# Installation

```
git clone https://github.com/nopnop2002/esp-idf-zlib
cd esp-idf-zlib/basic
idf.py menuconfig
idf.py flash
```

__There is no MENU ITEM where this application is peculiar.__   


# Compress   
Write 15 bytes of data to a file 10 times using gzwrite.   
The total size of the data is 150 bytes, but the file size on the file system is 32 bytes.   

# DeCompress   
Read compressed files from the file system using gzread.   
The total read size is 150 bytes.   

# Screen Shot   
```
I (402) MAIN: Initializing SPIFFS file system
I (432) MAIN: Partition size: total: 354161, used: 251
I (492) printDirectory: /root d_name=README.txt d_ino=0 fsize=0
I (492) printDirectory: /root d_name=out.gz d_ino=0 fsize=32
I (512) MAIN: outSize=150 ---> This is the total data size written
I (512) MAIN: fileSize=32 ---> This is the file size on the FAT file system
I (512) MAIN: buf=[test test test
test test test
test test test
test test test
test]
I (512) MAIN: buf=[ test test
test test test
test test test
test test test
test tes]
I (522) MAIN: buf=[t test
test test test
]
I (532) MAIN: inSize=150 ---> This is the total data size read
I (532) MAIN: FLASH unmounted
```

