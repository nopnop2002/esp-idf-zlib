set(srcs "main.c" "mqtt_pub.c" "mqtt_sub.c" "compress.c")

idf_component_register(SRCS "${srcs}" INCLUDE_DIRS ".")
