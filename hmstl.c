#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libtrix.h>
#include "heightmap.h"

typedef struct {
	int log; // boolean; verbose logging if true
	int base; // boolean; output walls and bottom as well as terrain surface if true
	int ascii; // boolean; output ASCII STL instead of binary STL if true
	char *input; // path to input file; use stdin if NULL
	char *output; // path to output file; use stdout if NULL
	char *mask; // path to mask file; no mask if NULL
	float zscale; // scaling factor applied to raw Z values; default 1.0
	float baseheight; // height in STL units of base below lowest terrain (technically, offset added to scaled Z values); default and minimum 1.0
} Settings;
Settings CONFIG = {0, 1, 0, NULL, NULL, NULL, 1.0, 1.0};

Heightmap *mask = NULL;

void Triangle(trix_mesh *mesh, const Heightmap *hm,
		unsigned int x1i, unsigned int y1i, float z1,
		unsigned int x2i, unsigned int y2i, float z2,
		unsigned int x3i, unsigned int y3i, float z3) {
	
	trix_triangle t;
	
	t.a.x = (float)x1i; t.a.y = (float)(hm->height - y1i); t.a.z = z1;
	t.b.x = (float)x2i; t.b.y = (float)(hm->height - y2i); t.b.z = z2;
	t.c.x = (float)x3i; t.c.y = (float)(hm->height - y3i); t.c.z = z3;
	
	// imply normals from face winding
	t.n.x = 0; t.n.y = 0; t.n.z = 0;
	
	(void)trixAddTriangle(mesh, &t);
}

// return true if the pixel at row/col is masked (not visible)
int Masked(unsigned int row, unsigned int col) {
	unsigned long index;
	
	// just bake check for CONFIG.mask in here and return unmasked
	// if masking is not enabled.
	
	index = (row * mask->width) + col;
	if (mask->data[index] > 128) {
		return 1;
	}
	return 0;
}

// returns average of all non-negative arguments.
// If any argument is negative, it is not included in the average.
// argument zp will always be nonnegative.
static float avgnonneg(float zp, float z1, float z2, float z3) {
	float avg = zp;
	
	if (z1 >= 0) {
		avg = (avg + z1) / 2.0;
	}
	
	if (z2 >= 0) {
		avg = (avg + z2) / 2.0;
	}
	
	if (z3 >= 0) {
		avg = (avg + z3) / 2.0;
	}
	
	return avg;
}

static unsigned char hmat(const Heightmap *hm, unsigned int x, unsigned int y) {
	return hm->data[(hm->width * y) + x];
}

static float hmz(unsigned char v) {
	return CONFIG.baseheight + (CONFIG.zscale * v);
}

void Mesh(const Heightmap *hm, trix_mesh *surface) {
	unsigned int x, y;
	float az, bz, cz, dz, ez, fz, gz, hz;
	trix_triangle ti = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
	trix_triangle tj = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
	trix_triangle tk = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
	trix_triangle tl = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
	trix_vertex vp, v1, v2, v3, v4;		
	unsigned long index;
	
	for (y = 0; y < hm->height; y++) {
		for (x = 0; x < hm->width; x++) {
			
			if (Masked(x, y)) {
				continue;
			}
			
			/*
				+---+---+---+
				|   |   |   |
				| A | B | C |
				|   |   |   |
				+---1---2---+
				|   |\I/|   |
				| H |LPJ| D |
				|   |/K\|   |
				+---4---3---+
				|   |   |   |
				| G | F | E |
				|   |   |   |
				+---+---+---+
			
			Current pixel position is marked by center, P.
			This pixel is output as four triangles, I, J, K,
			and L. Vertex P is shared among them.
			The remaining four vertices are 1, 2, 3, and 4.
			Neighbring pixels are A, B, C, D, E, F, G, and H.
			
			Vertex 1 z is average of ABPH z
			Vertex 2 z is average of BCDP z
			Vertex 3 z is average of PDEF z
			Vertex 4 z is average of HPFG z
			
			Any neighbor pixel that is masked or out of bounds
			(as with map edges) is omitted from the average.
						
			*/
			
			if (x == 0 || y == 0 || Masked(x-1, y-1)) {
				az = -1;
			} else {
				az = hmz(hmat(hm, x - 1, y - 1));
			}
			
			if (y == 0 || Masked(x, y - 1)) {
				bz = -1;
			} else {
				bz = hmz(hmat(hm, x, y - 1));
			}
			
			if (y == 0 || x + 1 == hm->width || Masked(x + 1, y - 1)) {
				cz = -1;
			} else {
				cz = hmz(hmat(hm, x + 1, y - 1));
			}
			
			if (x + 1 == hm->width || Masked(x + 1, y)) {
				dz = -1;
			} else {
				dz = hmz(hmat(hm, x + 1, y));
			}
			
			if (x + 1 == hm->width || y + 1 == hm->height || Masked(x + 1, y + 1)) {
				ez = -1;
			} else {
				ez = hmz(hmat(hm, x + 1, y + 1));
			}
			
			if (y + 1 == hm->height || Masked(x, y + 1)) {
				fz = -1;
			} else {
				fz = hmz(hmat(hm, x, y + 1));
			}
			
			if (y + 1 == hm->height || x == 0 || Masked(x - 1, y + 1)) {
				gz = -1;
			} else {
				gz = hmz(hmat(hm, x - 1, y + 1));
			}
			
			if (x == 0 || Masked(x - 1, y)) {
				hz = -1;
			} else {
				hz = hmz(hmat(hm, x - 1, y));
			}			
			
			// common pixel vertex p
			vp.x = (float)x;
			vp.y = (float)(hm->height - y);
			index = (y * hm->width) + x;
			vp.z = hmz(hm->data[index]);
			
			// Vertex 1
			v1.x = (float)x - 0.5;
			v1.y = (float)(hm->height - (y - 0.5));
			v1.z = avgnonneg(vp.z, az, bz, hz);
			
			// Vertex 2
			v2.x = (float)x + 0.5;
			v2.y = v1.y;
			v2.z = avgnonneg(vp.z, bz, cz, dz);
			
			// Vertex 3
			v3.x = v2.x;
			v3.y = (float)(hm->height - (y + 0.5));
			v3.z = avgnonneg(vp.z, dz, ez, fz);
			
			// Vertex 4
			v4.x = v1.x;
			v4.y = v3.y;
			v4.z = avgnonneg(vp.z, hz, fz, gz);
			
			// this can be done by assigning triangle vertices directly
			// (doing everything seperately just helps keep it clear)
			
			// Triangle I
			// Vertices 2, 1, and P
			ti.a = v2;
			ti.b = v1;
			ti.c = vp;
			
			// Triangle J
			// Vertices 3, 2, and P
			tj.a = v3;
			tj.b = v2;
			tj.c = vp;
			
			// Triangle K
			// Vertices 4, 3, and P
			tk.a = v4;
			tk.b = v3;
			tk.c = vp;
			
			// Triangle L
			// Vertices 1, 4, and P
			tl.a = v1;
			tl.b = v4;
			tl.c = vp;
			
			// ought to attend to result, so we can stop
			// if there is a problem with the mesh.
			
			(void)trixAddTriangle(surface, &ti);
			(void)trixAddTriangle(surface, &tj);
			(void)trixAddTriangle(surface, &tk);
			(void)trixAddTriangle(surface, &tl);
			
			// generate walls for b, d, f, or h if -1
		}
	}
	
}

// returns 0 on success, nonzero otherwise
int HeightmapToSTL(Heightmap *hm) {
	trix_result r;
	trix_mesh *mesh;
	
	if ((r = trixCreate(&mesh, "hmstl")) != TRIX_OK) {
		return (int)r;
	}
	
	Mesh(hm, mesh);

	// writes to stdout if CONFIG.output is null, otherwise writes to path it names
	if ((r = trixWrite(mesh, CONFIG.output, (CONFIG.ascii ? TRIX_STL_ASCII : TRIX_STL_BINARY))) != TRIX_OK) {
		return (int)r;
	}
	
	(void)trixRelease(&mesh);
	
	return 0;
}

// https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
// returns 0 if options are parsed successfully; nonzero otherwise
int parseopts(int argc, char **argv) {
	
	int c;
	
	// suppress automatic error messages generated by getopt
	opterr = 0;
	
	while ((c = getopt(argc, argv, "az:b:o:i:m:sv")) != -1) {
		switch (c) {
			case 'a':
				// ASCII mode output
				CONFIG.ascii = 1;
				break;
			case 'z':
				// Z scale (heightmap value units relative to XY)
				if (sscanf(optarg, "%20f", &CONFIG.zscale) != 1 || CONFIG.zscale <= 0) {
					fprintf(stderr, "CONFIG.zscale must be a number greater than 0.\n");
					return 1;
				}
				break;
			case 'b':
				// Base height (default 1.0 units)
				if (sscanf(optarg, "%20f", &CONFIG.baseheight) != 1 || CONFIG.baseheight < 1) {
					fprintf(stderr, "BASEHEIGHT must be a number greater than or equal to 1.\n");
					return 1;
				}
				break;
			case 'o':
				// Output file (default stdout)
				CONFIG.output = optarg;
				break;
			case 'i':
				// Input file (default stdin)
				CONFIG.input = optarg;
				break;
			case 'm':
				// Mask file (default none)
				CONFIG.mask = optarg;
			case 's':
				// surface only mode - omit base (walls and bottom)
				CONFIG.base = 0;
				break;
			case 'v':
				// Verbose mode (log to stderr)
				CONFIG.log = 1;
				break;
			case '?':
				// unrecognized option OR missing option argument
				switch (optopt) {
					case 'z':
					case 'b':
					case 'i':
					case 'o':
						fprintf(stderr, "Option -%c requires an argument.\n", optopt);
						break;
					default:
						if (isprint(optopt)) {
							fprintf(stderr, "Unknown option -%c\n", optopt);
						}
						else {
							fprintf(stderr, "Unknown option character \\x%x\n", optopt);
						}
						break;
				}
				return 1;
				break;
			default:
				// My understand is getopt will return ? for all unrecognized options,
				// so I'm not sure what other cases will be caught here. Perhaps just
				// options specified in optstring that I forget to handle above...
				fprintf(stderr, "Unexpected getopt result: %c\n", optopt);
				return 1;
				break;
		}
	}
	
	// should be no parameters left
	if (optind < argc) {
		fprintf(stderr, "Extraneous arguments\n");
		return 1;
	}
	
	return 0;
}

int main(int argc, char **argv) {
	Heightmap *hm = NULL;
	
	if (parseopts(argc, argv)) {
		fprintf(stderr, "option parsing failed\n", argv[0]);
		return 1;
	}
	
	if (CONFIG.mask != NULL) {
		if ((mask = ReadHeightmap(CONFIG.mask)) == NULL) {
			return 1;
		}
	}
	
	if ((hm = ReadHeightmap(CONFIG.input)) == NULL) {
		return 1;
	}
	
	if (CONFIG.log) {
		DumpHeightmap(hm);
	}
	
	HeightmapToSTL(hm);
	
	FreeHeightmap(hm);
	
	if (CONFIG.mask != NULL) {
		FreeHeightmap(mask);
	}
	
	return 0;
}
