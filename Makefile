.PHONY: test clean

hmstl: hmstl.c heightmap.c heightmap.h stb_image.o
	gcc hmstl.c heightmap.c stb_image.o -o hmstl -ltrix -lm -L/usr/local/lib -Wl,-R/usr/local/lib

stb_image.o: stb_image.c stb_image.h
	gcc -c stb_image.c -o stb_image.o

test: hmstl
	tclsh test/all.tcl -constraint static

clean:
	rm -f hmstl stb_image.o
