import zlib
import os
import sys

def usage(name):
	print("usage python3 {} -c path_to_compress path_to_output".format(name))
	print("usage python3 {} -d path_to_decompress path_to_output".format(name))

def main():
	argv = sys.argv
	args = len(argv)
	#print("args={}".format(args))
	if args != 4:
		usage(argv[0])
		exit()

	if os.path.isfile(argv[2]) is False:
		print("{} not exist".format(argv[2]))
		exit()

	if os.path.isfile(argv[3]) is True:
		print("{} already exist".format(argv[3]))
		exit()

	mode = argv[1]
	print("mode={}".format(mode))
	src = argv[2]
	dst = argv[3]
	if mode == '-c':
		print("src={} dst={}".format(src, dst))
		with open(src, "rb") as f, open(dst, "wb") as c:
			text = f.read()
			#c.write(zlib.compress(bytes(text, 'utf8')))
			c.write(zlib.compress(text))
	elif mode == '-d':
		print("src={} dst={}".format(src, dst))
		with open(dst, "wb") as out, open(src, "rb") as c:
			text = zlib.decompress(c.read())
			#out.write(text.decode('utf8'))
			out.write(text)

if __name__ == "__main__":
	main()

