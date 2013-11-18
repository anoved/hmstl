
hmstl: hmstl.c heightmap.c heightmap.h
	gcc hmstl.c heightmap.c -o hmstl -ltrix -lm -L/usr/local/lib -Wl,-R/usr/local/lib
