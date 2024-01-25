# esp-idf-zlib
IDF component registry includes zlib for compression and decompression.   
https://components.espressif.com/components/espressif/zlib   

However, unfortunately there is no sample code for ESP-IDF.   
This is example compression and decompression using zlib.   

# Software requirements
ESP-IDF V4.4/V5.x.   
ESP-IDF V5.0 is required when using ESP32-C2.   
ESP-IDF V5.1 is required when using ESP32-C6.   

# Installation

```
git clone https://github.com/nopnop2002/esp-idf-zlib
cd esp-idf-zlib/basic
idf.py menuconfig
idf.py flash
```

# Memory Footprint   
zlib's memory footprint can also be specified fairly precisely.   
It is larger for compression than for decompression, and the exact requirements depend on how the library was compiled.   

The memory requirements for compression depend on two parameters, windowBits and memLevel:   

```deflate memory usage (bytes) = (1 << (windowBits+2)) + (1 << (memLevel+9)) + 6 KB```

For the default values of 15 and 8, respectively, this is 268 KB, where the approximately 6 KB is for the deflate data structure. 
Both windowBits and memLevel can be set to lower values at compile time via the MAX_WBITS and MAX_MEM_LEVEL macros, but only at a cost in compression efficiency.   

The memory requirements for decompression depend only on windowBits, but this is, in a sense, a harsher limitation: whereas data streams compressed with a smaller window will merely be a bit larger than they would have otherwise, a reduced window size for decompression means that streams compressed with larger windows cannot be decompressed at all. Having said that:   

```inflate memory usage (bytes) = (1 << windowBits) + 7 KB```

Typically, therefore, inflate() requires no more than 40 KB of storage on a 64-bit machine. This includes the 32768-byte sliding window and approximately 7 KB for the inflate data structure.   

The MAX_WBITS and MAX_MEM_LEVEL macros can be specified as compile options.   
In this project, appropriate values are set for each ESP32 SoC.   

# How to use zlib on Linux

### Installing zlib on Linux   
```
sudo apt install zlib1g-dev
```

### Testing zlib   
```
cd esp-idf-zlib/linux
cc -o test test.c -lz
./test
```

### Compress file using zlib
```
cc -o zpipe zpipe.c -lz
./zpipe < path_to_input > path_to_output
```

zpipe.c is published [here](https://www.zlib.net/zpipe.c).   

### DeCompress file using zlib
```
cc -o zpipe zpipe.c -lz
./zpipe -d < path_to_input > path_to_output
```

If path_to_input is a compressed text file, this is fine.   
```
./zpipe -d < path_to_input
```

### Example
```
./zpipe < zpipe.c > zpipe.z.zlib
./zpipe -d < zpipe.z.zlib > zpipe.c.c
diff zpipe.c zpipe.c.c
```

# How to use zlib on python
```
cd python
python3 zlib.py

# Compress file
usage python3 zlib.py -c path_to_compress path_to_output

# Decompress file
usage python3 zlib.py -d path_to_decompress path_to_output
```

### Example
``
python3 zlib.py -c test.txt test.txt.zlib
python3 zlib.py -d test.txt.zlib test.txt.txt
diff test.txt test.txt.txt
```


# Comparison of zlib and brotli

|source file|source size(byte)|brotli comress(byte)|zlib compress(byte)|
|:-:|:-:|:-:|:-:|
|test.txt|20479|9470|4571|
|esp32.jpeg|18753|18613|18218|
|esp32.png|43540|43640|43264|

# Reference

https://github.com/nopnop2002/esp-idf-brotli

