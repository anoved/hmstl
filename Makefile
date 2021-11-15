.PHONY: test clean

hmstl: hmstl.c heightmap.c heightmap.h stb_image.h
	gcc hmstl.c heightmap.c -o hmstl -ltrix -lm -L/usr/local/lib -Wl,-R/usr/local/lib

test: hmstl
	tclsh test/all.tcl -constraint static

clean:
	rm -f hmstl stb_image.o
