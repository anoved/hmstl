#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "heightmap.h"

// must be exactly 80 characters
#define BINARY_STL_HEADER "hmstl                                                                           "

typedef struct {
	int log; // boolean; verbose logging if true
	int base; // boolean; output walls and bottom as well as terrain surface if true
	int ascii; // boolean; output ASCII STL instead of binary STL if true
	char *input; // path to input file; use stdin if NULL
	char *output; // path to output file; use stdout if NULL
	float zscale; // scaling factor applied to raw Z values; default 1.0
	float baseheight; // height in STL units of base below lowest terrain (technically, offset added to scaled Z values); default and minimum 1.0
} Settings;
Settings CONFIG = {0, 1, 0, NULL, NULL, 1.0, 1.0};

unsigned long Facecount(const Heightmap *hm) {
	unsigned long facecount;
	facecount = 2 * (hm->width - 1) * (hm->height - 1);
	if (CONFIG.base) {
		facecount += (4 * hm->width) + (4 * hm->height) - 6;
	}
	return facecount;
}

void StartSTL(FILE *fp, const Heightmap *hm) {
	if (CONFIG.ascii) {
		if (fprintf(fp, "solid hmstl\n") < 0) {
			// write failure
		}
	}
	else {
		char header[] = BINARY_STL_HEADER;
		unsigned long facecount;
		
		if (fwrite(header, 80, 1, fp) != 1) {
			// write failure
		}
		
		facecount = Facecount(hm);
		if (fwrite(&facecount, 4, 1, fp) != 1) {
			// write failure
		}
	}
}

void EndSTL(FILE *fp) {
	if (CONFIG.ascii) {
		if (fprintf(fp, "endsolid hmstl\n") < 0) {
		}
	}
}

void TriangleASCII(FILE *fp,
		float nx, float ny, float nz,
		float x1, float y1, float z1,
		float x2, float y2, float z2,
		float x3, float y3, float z3) {
	if (fprintf(fp,
			"facet normal %f %f %f\n"
			"outer loop\n"
			"vertex %f %f %f\n"
			"vertex %f %f %f\n"
			"vertex %f %f %f\n"
			"endloop\n"
			"endfacet\n",
			nx, ny, nz,
			x1, y1, z1,
			x2, y2, z2,
			x3, y3, z3) < 0) {
		// write failure
	}
}

void TriangleBinary(FILE *fp,
		float nx, float ny, float nz,
		float x1, float y1, float z1,
		float x2, float y2, float z2,
		float x3, float y3, float z3) {
	
	unsigned short attributes = 0;
	float coordinates[12] = {nx, ny, nz, x1, y1, z1, x2, y2, z2, x3, y3, z3};
	
	if (fwrite(coordinates, 4, 12, fp) != 12) {
		// write failure
	}
	
	if (fwrite(&attributes, 2, 1, fp) != 1) {
		// write failure
	}
}

void Triangle(FILE *fp, const Heightmap *hm,
		unsigned int x1i, unsigned int y1i, float z1,
		unsigned int x2i, unsigned int y2i, float z2,
		unsigned int x3i, unsigned int y3i, float z3) {
	
	float x1, y1, x2, y2, x3, y3;
	float nx, ny, nz;
	
	x1 = (float)x1i; y1 = (float)(hm->height - y1i);
	x2 = (float)x2i; y2 = (float)(hm->height - y2i);
	x3 = (float)x3i; y3 = (float)(hm->height - y3i);
	
	// imply normals from face winding
	nx = 0; ny = 0; nz = 0;
	
	if (CONFIG.ascii) {
		TriangleASCII(fp, nx, ny, nz, x1, y1, z1, x2, y2, z2, x3, y3, z3);
	} else {
		TriangleBinary(fp, nx, ny, nz, x1, y1, z1, x2, y2, z2, x3, y3, z3);
	}
}

void Mesh(const Heightmap *hm, FILE *fp) {

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
			Triangle(fp, hm,
					Ax, Ay, CONFIG.baseheight + CONFIG.zscale * hm->data[A],
					Bx, By, CONFIG.baseheight + CONFIG.zscale * hm->data[B],
					Dx, Dy, CONFIG.baseheight + CONFIG.zscale * hm->data[D]);
			
			// BCD
			Triangle(fp, hm,
					Bx, By, CONFIG.baseheight + CONFIG.zscale * hm->data[B],
					Cx, Cy, CONFIG.baseheight + CONFIG.zscale * hm->data[C],
					Dx, Dy, CONFIG.baseheight + CONFIG.zscale * hm->data[D]);
		}
	}
}

void Walls(const Heightmap *hm, FILE *fp) {
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
		Triangle(fp, hm,
				(unsigned int)a, 0, CONFIG.baseheight + CONFIG.zscale * hm->data[a],
				(unsigned int)b, 0, CONFIG.baseheight + CONFIG.zscale * hm->data[b],
				(unsigned int)a, 0, 0);
		Triangle(fp, hm,
				(unsigned int)b, 0, CONFIG.baseheight + CONFIG.zscale * hm->data[b],
				(unsigned int)b, 0, 0,
				(unsigned int)a, 0, 0);
		
		// south wall
		a += bottom * hm->width;
		b += bottom * hm->width;
		Triangle(fp, hm,
				col, bottom, CONFIG.baseheight + CONFIG.zscale * hm->data[a],
				col, bottom, 0,
				col + 1, bottom, CONFIG.baseheight + CONFIG.zscale * hm->data[b]);
		Triangle(fp, hm,
				col, bottom, 0,
				col + 1, bottom, 0,
				col + 1, bottom, CONFIG.baseheight + CONFIG.zscale * hm->data[b]);
	}
	
	// west and east walls
	for (row = 0; row < hm->height - 1; row++) {
		
		// west wall
		a = row * hm->width;
		b = (row + 1) * hm->width;
		Triangle(fp, hm,
				0, row, CONFIG.baseheight + CONFIG.zscale * hm->data[a],
				0, row, 0,
				0, row + 1, CONFIG.baseheight + CONFIG.zscale * hm->data[b]);
		Triangle(fp, hm,
				0, row, 0,
				0, row + 1, 0,
				0, row + 1, CONFIG.baseheight + CONFIG.zscale * hm->data[b]);
		
		// east wall
		a += right;
		b += right;
		Triangle(fp, hm,
				right, row, CONFIG.baseheight + CONFIG.zscale * hm->data[a],
				right, row + 1, 0,
				right, row, 0);
		Triangle(fp, hm,
				right, row, CONFIG.baseheight + CONFIG.zscale * hm->data[a],
				right, row + 1, CONFIG.baseheight + CONFIG.zscale * hm->data[b],
				right, row + 1, 0);
	}
	
}

void Bottom(const Heightmap *hm, FILE *fp) {
	// Technically this may yield an invalid STL, since border
	// triangles will meet the edges of these bottom cap faces
	// in a series of T-junctions.
	Triangle(fp, hm,
			0, 0, 0,
			hm->width - 1, 0, 0,
			0, hm->height - 1, 0);
	Triangle(fp, hm,
			hm->width - 1, 0, 0,
			hm->width - 1, hm->height - 1, 0,
			0, hm->height - 1, 0);
}

// returns 0 on success, nonzero otherwise
int HeightmapToSTL(Heightmap *hm) {
	
	FILE *fp;
	
	if (CONFIG.output == NULL) {
		fp = stdout;
	}
	else if ((fp = fopen(CONFIG.output, "w")) == NULL) {
		fprintf(stderr, "Cannot open output file %s\n", CONFIG.output);
		return 1;
	}
		
	StartSTL(fp, hm);
	
	Mesh(hm, fp);
	
	if (CONFIG.base) {
		Walls(hm, fp);
		Bottom(hm, fp);
	}
	
	EndSTL(fp);
	
	if (fp != stdout) {
		fclose(fp);
	}
	
	return 0;
}

// https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
// returns 0 if options are parsed successfully; nonzero otherwise
int parseopts(int argc, char **argv) {
	
	int c;
	
	// suppress automatic error messages generated by getopt
	opterr = 0;
	
	while ((c = getopt(argc, argv, "az:b:o:i:mv")) != -1) {
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
				// mesh only mode - omit base (walls and bottom)
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
		fprintf(stderr, "Usage: %s [-z CONFIG.zscale] [-b BASEHEIGHT] [-i INPUT] [-o OUTPUT]\n", argv[0]);
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
