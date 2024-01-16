# file   
Demonstration of compressing and decompressing files residing in SPIFFS.   
This project uses the SPIFFS file system.   

# Compress   
Compress the file using the comp function.   
This function was referenced [here](https://www.zlib.net/zpipe.c).

# DeCompress   
Decompress the file using the decomp function.   
This function was referenced [here](https://www.zlib.net/zpipe.c).

# Screen Shot   
```
I (400) MAIN: Initializing SPIFFS
I (430) MAIN: Partition size: total: 354161, used: 1757
I (440) printDirectory: /root d_name=README.txt d_ino=0 fsize=1417 ---> Original file
I (490) MAIN: comp=0
I (500) printDirectory: /root d_name=README.txt d_ino=0 fsize=1417
I (500) printDirectory: /root d_name=README.txt.zip d_ino=0 fsize=595 ---> Compressed file
I (550) MAIN: decomp=0
I (560) printDirectory: /root d_name=README.txt d_ino=0 fsize=1417
I (560) printDirectory: /root d_name=README.txt.zip d_ino=0 fsize=595
I (560) printDirectory: /root d_name=README.txt.txt d_ino=0 fsize=1417 ---> DeCompressed file
I (580) MAIN: FLASH unmounted
I (580) main_task: Returned from app_main()
```

