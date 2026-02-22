idf_component_register(SRCS "main.c" "mesh.c" "packet.c" "cache.c"
                    INCLUDE_DIRS "."
                    REQUIRES mirf driver)