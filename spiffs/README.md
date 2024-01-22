# spiffs
Demonstration of compressing and decompressing files on SPIFFS.   
This project uses the SPIFFS file system.   

# Installation

```
git clone https://github.com/nopnop2002/esp-idf-zlib
cd esp-idf-zlib/spiffs
idf.py menuconfig
idf.py flash
```

__There is no MENU ITEM where this application is peculiar.__   


# Screen Shot   
```
I (412) MAIN: Initializing SPIFFS
I (442) MAIN: Partition size: total: 354161, used: 1757
I (442) printDirectory: /root d_name=README.txt d_ino=0 fsize=1417 ---> Original file
I (452) COMPRESS: Start
I (452) COMPRESS: param.srcPath=[/root/README.txt]
I (452) COMPRESS: param.dstPath=[/root/README.txt.zlib]
I (502) COMPRESS: Finish
I (502) MAIN: comp_result=0
I (502) printDirectory: /root d_name=README.txt d_ino=0 fsize=1417
I (502) printDirectory: /root d_name=README.txt.zlib d_ino=0 fsize=595 ---> Compressed file
I (522) DECOMPRESS: Start
I (522) DECOMPRESS: param.srcPath=[/root/README.txt.zlib]
I (522) DECOMPRESS: param.dstPath=[/root/README.txt.txt]
I (572) DECOMPRESS: Finish
I (572) MAIN: decomp_result=0
I (582) printDirectory: /root d_name=README.txt d_ino=0 fsize=1417
I (582) printDirectory: /root d_name=README.txt.zlib d_ino=0 fsize=595
I (582) printDirectory: /root d_name=README.txt.txt d_ino=0 fsize=1417 ---> Decompressed file
I (602) MAIN: FLASH unmounted
I (602) main_task: Returned from app_main()
```

