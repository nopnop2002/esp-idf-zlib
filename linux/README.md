# How to use zlib on Linux

### Installing zlib on Linux   
```
sudo apt install zlib1g-dev
cd esp-idf-zlib/linux
cc -o test test.c -lz
cc -o zpipe zpipe.c -lz
```

zpipe.c is published [here](https://www.zlib.net/zpipe.c).   

### Testing zlib   
```
./test
```

### Compress file using zlib
```
./zpipe < path_to_input > path_to_output
```

### Decompress file using zlib
```
./zpipe -d < path_to_input > path_to_output
```

If path_to_input is a compressed text file, this is fine.   
```
./zpipe -d < path_to_input
```

### Example
```
./zpipe < zpipe.c > zpipe.c.zlib
./zpipe -d < zpipe.c.zlib > zpipe.c.c
diff zpipe.c zpipe.c.c
```
