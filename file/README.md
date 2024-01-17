# file   
Demonstration of compressing and decompressing files residing in SPIFFS.   
This project uses the SPIFFS file system.   

# Installation

```
git clone https://github.com/nopnop2002/esp-idf-zlib
cd esp-idf-zlib/file
idf.py menuconfig
idf.py flash
```

__There is no MENU ITEM where this application is peculiar.__   


# Compress   
Compress the file using the comp function.   
I used [this](https://www.zlib.net/zpipe.c) as a reference.   

# DeCompress   
Decompress the file using the decomp function.   
I used [this](https://www.zlib.net/zpipe.c) as a reference.   

# Screen Shot   
```
I (442) MAIN: Initializing SPIFFS
I (462) MAIN: Partition size: total: 354161, used: 1757
I (462) printDirectory: /root d_name=README.txt d_ino=0 fsize=1417 ---> Original file
I (472) COMPRESS: Start
I (472) COMPRESS: param.srcPath=[/root/README.txt]
I (472) COMPRESS: param.dstPath=[/root/README.txt.zip]
I (522) COMPRESS: Finish
I (522) MAIN: comp_result=0
I (522) printDirectory: /root d_name=README.txt d_ino=0 fsize=1417
I (522) printDirectory: /root d_name=README.txt.zip d_ino=0 fsize=599 ---> Compressed file
I (542) DECOMPRESS: Start
I (542) DECOMPRESS: param.srcPath=[/root/README.txt.zip]
I (552) DECOMPRESS: param.dstPath=[/root/README.txt.txt]
I (592) DECOMPRESS: Finish
I (592) MAIN: decomp_result=0
I (592) printDirectory: /root d_name=README.txt d_ino=0 fsize=1417
I (592) printDirectory: /root d_name=README.txt.zip d_ino=0 fsize=599
I (602) printDirectory: /root d_name=README.txt.txt d_ino=0 fsize=1417 ---> DeCompressed file
I (622) MAIN: FLASH unmounted
I (622) main_task: Returned from app_main()
```

