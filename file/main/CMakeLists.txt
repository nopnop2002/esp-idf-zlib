set(srcs "main.c" "compress.c")

idf_component_register(SRCS "${srcs}" INCLUDE_DIRS ".")
