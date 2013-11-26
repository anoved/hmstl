#include <stdio.h>
#include <stdlib.h>

#ifndef S_SPLINT_S
#include <unistd.h>
#endif

#include <libtrix.h>
#include "heightmap.h"

typedef struct {
	int base; // boolean; output walls and bottom as well as terrain surface if true
	int ascii; // boolean; output ASCII STL instead of binary STL if true	
	char *input; // path to input file; use stdin if NULL
	char *output; // path to output file; use stdout if NULL
	char *mask; // path to mask file; no mask if NULL
	int threshold; // 0-255; maximum value considered masked
	int reversed; // boolean; reverse results of mask tests if true
	int heightmask; // boolean; use heightmap as it's own mask if true
	float zscale; // scaling factor applied to raw Z values
	float baseheight; // height in STL units of base below lowest terrain (technically, offset added to scaled Z values)
} Settings;

Settings CONFIG = {
	1,    // generate base (walls and bottom)
	0,    // binary output
	NULL, // read from stdin
	NULL, // write to stdout
	NULL, // no mask
	127,  // middle of 8 bit range
	0,    // normal un-reversed mask
	0,    // no heightmasking
	1.0,  // no z scaling (use raw heightmap values)
	1.0   // minimum base thickness of one unit
};

Heightmap *mask = NULL;

// If a mask is defined, only portions of the heightmap that are visible through the mask are output.
// Bright areas of the mask image are considered transparent and dark areas are considered opaque.
int Masked(unsigned int x, unsigned int y) {
	unsigned long index;
	int result = 0;
	
	// is masking mode even on?
	// (not if no heightmask or mask file.)
	if (mask == NULL) {
		return 0;
	}
	
	index = (y * mask->width) + x;
	if (mask->data[index] <= (unsigned char)CONFIG.threshold) {
		result = 1;
	}
	
	if (CONFIG.reversed) {
		result = !result;
	}
	
	return result;
}

static void Wall(trix_mesh *mesh, const trix_vertex *a, const trix_vertex *b) {
	trix_vertex a0 = *a;
	trix_vertex b0 = *b;
	trix_triangle t1;
	trix_triangle t2;
	a0.z = 0;
	b0.z = 0;
	t1.a = *a;
	t1.b = *b;
	t1.c = b0;
	t2.a = b0;
	t2.b = a0;
	t2.c = *a;
	(void)trixAddTriangle(mesh, &t1);
	(void)trixAddTriangle(mesh, &t2);
}

// returns average of all non-negative arguments.
// If any argument is negative, it is not included in the average.
// argument zp will always be nonnegative.
static float avgnonneg(float zp, float z1, float z2, float z3) {
	float sum = zp;
	float z[3] = {z1, z2, z3};
	int i, n = 1;
	
	for (i = 0; i < 3; i++) {
		if (z[i] >= 0) {
			sum += z[i];
			n++;
		}
	}
	
	return sum / (float)n;
}

static float hmzat(const Heightmap *hm, unsigned int x, unsigned int y) {
	return CONFIG.baseheight + (CONFIG.zscale * hm->data[(hm->width * y) + x]);;
}

void Mesh(const Heightmap *hm, trix_mesh *mesh) {
	unsigned int x, y;
	float az, bz, cz, dz, ez, fz, gz, hz;
	trix_vertex vp, v1, v2, v3, v4;
	trix_triangle ti, tj, tk, tl;

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
			
			Current pixel position is marked at center as P.
			This pixel is output as four triangles, I, J, K,
			and L. Vertex P is common to all four triangles.
			The remaining four vertices are 1, 2, 3, and 4.
			Neighboring pixels are A, B, C, D, E, F, G, and H.
			
			Vertex 1 z is average of ABPH z
			Vertex 2 z is average of BCDP z
			Vertex 3 z is average of PDEF z
			Vertex 4 z is average of HPFG z
			
			Averages do not include neighbors that would lie
			outside the image, but do included masked values.
			
			*/
			
			if (x == 0 || y == 0) {
				az = -1;
			} else {
				az = hmzat(hm, x - 1, y - 1);
			}
			
			if (y == 0) {
				bz = -1;
			} else {
				bz = hmzat(hm, x, y - 1);
			}
			
			if (y == 0 || x + 1 == hm->width) {
				cz = -1;
			} else {
				cz = hmzat(hm, x + 1, y - 1);
			}
			
			if (x + 1 == hm->width) {
				dz = -1;
			} else {
				dz = hmzat(hm, x + 1, y);
			}
			
			if (x + 1 == hm->width || y + 1 == hm->height) {
				ez = -1;
			} else {
				ez = hmzat(hm, x + 1, y + 1);
			}
			
			if (y + 1 == hm->height) {
				fz = -1;
			} else {
				fz = hmzat(hm, x, y + 1);
			}
			
			if (y + 1 == hm->height || x == 0) {
				gz = -1;
			} else {
				gz = hmzat(hm, x - 1, y + 1);
			}
			
			if (x == 0) {
				hz = -1;
			} else {
				hz = hmzat(hm, x - 1, y);
			}			
			
			// common pixel vertex p
			vp.x = (float)x;
			vp.y = (float)(hm->height - y);
			vp.z = hmzat(hm, x, y);
			
			// Vertex 1
			v1.x = (float)x - 0.5;
			v1.y = ((float)hm->height - ((float)y - 0.5));
			v1.z = avgnonneg(vp.z, az, bz, hz);
			
			// Vertex 2
			v2.x = (float)x + 0.5;
			v2.y = v1.y;
			v2.z = avgnonneg(vp.z, bz, cz, dz);
			
			// Vertex 3
			v3.x = v2.x;
			v3.y = ((float)hm->height - ((float)y + 0.5));
			v3.z = avgnonneg(vp.z, dz, ez, fz);
			
			// Vertex 4
			v4.x = v1.x;
			v4.y = v3.y;
			v4.z = avgnonneg(vp.z, hz, fz, gz);
						
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
			
			(void)trixAddTriangle(mesh, &ti);
			(void)trixAddTriangle(mesh, &tj);
			(void)trixAddTriangle(mesh, &tk);
			(void)trixAddTriangle(mesh, &tl);
			
			// nothing left to do for this pixel unless we need to make walls
			if (!CONFIG.base) {
				continue;
			}
		
			// bottom quad
			ti.a = v1; ti.b = v2; ti.c = v4;
			ti.a.z = 0; ti.b.z = 0; ti.c.z = 0;
			tj.a = v2; tj.b = v3; tj.c = v4;
			tj.a.z = 0; tj.b.z = 0; tj.c.z = 0;
			(void)trixAddTriangle(mesh, &ti);
			(void)trixAddTriangle(mesh, &tj);
			
			// north wall (vertex 1 to 2)
			if (y == 0 || Masked(x, y - 1)) {
				Wall(mesh, &v1, &v2);
			}
			
			// east wall (vertex 2 to 3)
			if (x + 1 == hm->width || Masked(x + 1, y)) {
				Wall(mesh, &v2, &v3);
			}
			
			// south wall (vertex 3 to 4)
			if (y + 1 == hm->height || Masked(x, y + 1)) {
				Wall(mesh, &v3, &v4);
			}
			
			// west wall (vertex 4 to 1)
			if (x == 0 || Masked(x - 1, y)) {
				Wall(mesh, &v4, &v1);
			}
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
	
	while ((c = getopt(argc, argv, "az:b:o:i:m:t:rhs")) != -1) {
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
				break;
			case 't':
				// Mask threshold
				if (sscanf(optarg, "%5d", &CONFIG.threshold) != 1 || CONFIG.threshold < 0 || CONFIG.threshold > 255) {
					fprintf(stderr, "THRESHOLD must be a number between 0 to 255 inclusive\n");
					return 1;
				}
				break;
			case 'r':
				// Reverse mask
				CONFIG.reversed = 1;
				break;
			case 'h':
				// Heightmask mode
				CONFIG.heightmask = 1;
				break;
			case 's':
				// surface only mode - omit base (walls and bottom)
				CONFIG.base = 0;
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
		fprintf(stderr, "option parsing failed\n");
		return 1;
	}
	
	if ((hm = ReadHeightmap(CONFIG.input)) == NULL) {
		return 1;
	}
	
	if (CONFIG.heightmask) {
		// point the mask at the heightmap
		// instead of reading a separate image
		mask = hm;
	} else {
		mask = NULL;
	}
	
	// mask file loaded only if heightmask isn't already assigned
	if (CONFIG.mask != NULL && mask == NULL) {
		
		if ((mask = ReadHeightmap(CONFIG.mask)) == NULL) {
			return 1;
		}
		
		if ((mask->width != hm->width) || (mask->height != hm->height)) {
			fprintf(stderr, "Mask dimensions do not match heightmap dimensions.\n");
			fprintf(stderr, "Heightmap width: %u, height: %u\n", hm->width, hm->height);
			return 1;
		}
	}
	
	HeightmapToSTL(hm);
	
	FreeHeightmap(&hm);
	
	// free mask iff it is its own image
	if (mask != NULL && !CONFIG.heightmask) {
		FreeHeightmap(&mask);
	}
	
	return 0;
}
