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

void Mesh(const Heightmap *hm, trix_mesh *mesh) {

	unsigned int row, col;
	unsigned int Ax, Ay, Bx, By, Cx, Cy, Dx, Dy;
	unsigned long A, B, C, D;
	
	for (row = 0; row < hm->height - 1; row++) {
		for (col = 0; col < hm->width - 1; col++) {
			
			/*
			 * Point a is at coordinates row, col.
			 * We output the quad between point A and C as two
			 * triangles, ABD and BCD.
			 * 
			 * A-D
			 * |/|
			 * B-C
			 * 
			 */
			
			// Vertex coordinates in terms of row and col.
			Ax = col;     Ay = row;
			Bx = col;     By = row + 1;
			Cx = col + 1; Cy = row + 1;
			Dx = col + 1; Dy = row;
			
			// Bitmap indices of vertex coordinates.
			A = (Ay * hm->width) + Ax;
			B = (By * hm->width) + Bx;
			C = (Cy * hm->width) + Cx;
			D = (Dy * hm->width) + Dx;
			
			// hm->data[N] is the raw Z value of vertex N.
			// Z values are not necessarily expressed in same units as XY.
			// We should scale to 1.0 accordingly, then apply any extra factor.
			
			// ABD
			Triangle(mesh, hm,
					Ax, Ay, CONFIG.baseheight + CONFIG.zscale * hm->data[A],
					Bx, By, CONFIG.baseheight + CONFIG.zscale * hm->data[B],
					Dx, Dy, CONFIG.baseheight + CONFIG.zscale * hm->data[D]);
			
			// BCD
			Triangle(mesh, hm,
					Bx, By, CONFIG.baseheight + CONFIG.zscale * hm->data[B],
					Cx, Cy, CONFIG.baseheight + CONFIG.zscale * hm->data[C],
					Dx, Dy, CONFIG.baseheight + CONFIG.zscale * hm->data[D]);
		}
	}
}

void Walls(const Heightmap *hm, trix_mesh *mesh) {
	unsigned int row, col;
	unsigned long a, b;
	unsigned int bottom = hm->height -1;
	unsigned int right = hm->width - 1;
	
	// north and south walls
	for (col = 0; col < hm->width - 1; col++) {
		
		// north wall
		a = col;
		b = col + 1;
		// we're using a and b as xy coordinates only in this case, so cast to col/row type
		Triangle(mesh, hm,
				(unsigned int)a, 0, CONFIG.baseheight + CONFIG.zscale * hm->data[a],
				(unsigned int)b, 0, CONFIG.baseheight + CONFIG.zscale * hm->data[b],
				(unsigned int)a, 0, 0);
		Triangle(mesh, hm,
				(unsigned int)b, 0, CONFIG.baseheight + CONFIG.zscale * hm->data[b],
				(unsigned int)b, 0, 0,
				(unsigned int)a, 0, 0);
		
		// south wall
		a += bottom * hm->width;
		b += bottom * hm->width;
		Triangle(mesh, hm,
				col, bottom, CONFIG.baseheight + CONFIG.zscale * hm->data[a],
				col, bottom, 0,
				col + 1, bottom, CONFIG.baseheight + CONFIG.zscale * hm->data[b]);
		Triangle(mesh, hm,
				col, bottom, 0,
				col + 1, bottom, 0,
				col + 1, bottom, CONFIG.baseheight + CONFIG.zscale * hm->data[b]);
	}
	
	// west and east walls
	for (row = 0; row < hm->height - 1; row++) {
		
		// west wall
		a = row * hm->width;
		b = (row + 1) * hm->width;
		Triangle(mesh, hm,
				0, row, CONFIG.baseheight + CONFIG.zscale * hm->data[a],
				0, row, 0,
				0, row + 1, CONFIG.baseheight + CONFIG.zscale * hm->data[b]);
		Triangle(mesh, hm,
				0, row, 0,
				0, row + 1, 0,
				0, row + 1, CONFIG.baseheight + CONFIG.zscale * hm->data[b]);
		
		// east wall
		a += right;
		b += right;
		Triangle(mesh, hm,
				right, row, CONFIG.baseheight + CONFIG.zscale * hm->data[a],
				right, row + 1, 0,
				right, row, 0);
		Triangle(mesh, hm,
				right, row, CONFIG.baseheight + CONFIG.zscale * hm->data[a],
				right, row + 1, CONFIG.baseheight + CONFIG.zscale * hm->data[b],
				right, row + 1, 0);
	}
	
}

void Bottom(const Heightmap *hm, trix_mesh *mesh) {
	// Technically this may yield an invalid STL, since border
	// triangles will meet the edges of these bottom cap faces
	// in a series of T-junctions.
	Triangle(mesh, hm,
			0, 0, 0,
			hm->width - 1, 0, 0,
			0, hm->height - 1, 0);
	Triangle(mesh, hm,
			hm->width - 1, 0, 0,
			hm->width - 1, hm->height - 1, 0,
			0, hm->height - 1, 0);
}

// returns 0 on success, nonzero otherwise
int HeightmapToSTL(Heightmap *hm) {
	trix_result r;
	trix_mesh *mesh;
	
	if ((r = trixCreate(&mesh, "hmstl")) != TRIX_OK) {
		return (int)r;
	}
	
	Mesh(hm, mesh);
	
	if (CONFIG.base) {
		Walls(hm, mesh);
		Bottom(hm, mesh);
	}
	
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
	
	if ((hm = ReadHeightmap(CONFIG.input)) == NULL) {
		return 1;
	}
	
	if (CONFIG.log) {
		DumpHeightmap(hm);
	}
	
	HeightmapToSTL(hm);
	
	FreeHeightmap(hm);
	
	return 0;
}
