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
cd esp-idf-zlib/basic
idf.py menuconfig
idf.py flash
```

# How to use zlib on Linux

## Installing zlib on Linux   
```
sudo apt install zlib1g-dev
```

## Testing zlib   
```
cd esp-idf-zlib/linux
cc -o test test.c -lz
./test
```

## Compress file using zlib
```
cc -o zpipe zpipe.c -lz
./zpipe < path_to_input > path_to_output
```

zpipe.c is published [here](https://www.zlib.net/zpipe.c).   

## DeCompress file using zlib
```
cc -o zpipe zpipe.c -lz
./zpipe -d < path_to_input > path_to_output
```

If path_to_input is a compressed text file, this is fine.   
```
./zpipe -d < path_to_input
```
