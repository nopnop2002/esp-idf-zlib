# How to use zlib on python
```
cd esp-idf-zlib/python
python3 zlib.py

# Compress file
usage python3 zlib.py -c path_to_compress path_to_output

# Decompress file
usage python3 zlib.py -d path_to_decompress path_to_output
```

### Example
```
python3 zlib.py -c test.txt test.txt.zlib
python3 zlib.py -d test.txt.zlib test.txt.txt
diff test.txt test.txt.txt
```

