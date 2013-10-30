#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
	
	unsigned int width, height;
	unsigned long size;
	unsigned char *data;
	
	unsigned char min, max, range;
	
} Heightmap;

typedef struct {
	int log;
	char *input;
	char *output;
	float zscale;
	float baseheight;
} Settings;
Settings CONFIG = {0, NULL, NULL, 1.0, 1.0};

// Scan a loaded heightmap to determine minimum and maximum values
// Returns 1 on success, 0 on failure
// modifies hm min, max, and range on success
int PreprocessHeightmap(Heightmap *hm) {
	
	unsigned long i;
	unsigned char min, max;
	
	// data must be loaded before preprocessing is possible
	if (hm->data == NULL) {
		return 0;
	}
	
	min = 254;
	max = 0;
	
	for (i = 0; i < hm->size; i++) {
		if (hm->data[i] < min) {
			min = hm->data[i];
		}
		if (hm->data[i] > max) {
			max = hm->data[i];
		}
	}
	
	hm->min = min;
	hm->max = max;
	hm->range = max - min;
	
	return 1;
}

// Populate a heightmap with data from the specified PGM file
// return 0 on success, nonzero on failure
// modifies hm width, height, size, and allocates hm data on success
int LoadHeightmapFromPGM(Heightmap *hm) {
	
	FILE *fp;
	unsigned int width, height, depth; // depth only supported in unsigned char range (although I suppose we could support larger values, and scale to 8-bit range on load)
	unsigned long size;
	unsigned char *data;
	
	if (CONFIG.input == NULL) {
		fp = stdin;
	}
	else {
		if ((fp = fopen(CONFIG.input, "r")) == NULL) {
			fprintf(stderr, "Cannot open input file %s\n", CONFIG.input);
			return 1;
		}
	}
	
	// the NetPBM spec's allowance of comment lines anywhere in the header
	// spoils the otherwise extraordinarily-simple-to-parse format.
	
	// read pgm header
	// should allow comment lines in header
	// should understand binary (P2) as well as ASCII (P5) PGM formats
	fscanf(fp, "P5 ");
	fscanf(fp, "%d ", &width);
	fscanf(fp, "%d ", &height);
	fscanf(fp, "%d ", &depth);
	// check depth
	
	size = width * height;
	
	if ((data = (unsigned char *)malloc(size)) == NULL) {
		fprintf(stderr, "Cannot allocate memory for input bitmap\n");
		return 1;
	}
	
	if (fread(data, size, 1, fp) != 1) {
		fprintf(stderr, "Cannot read data from input bitmap\n");
		return 1;
	}
		
	if (fp != stdin) {
		fclose(fp);
	}
	
	hm->width = width;
	hm->height = height;
	hm->size = size;
	hm->data = data;
	
	return 0;
}

// frees hm data if allocated, and resets other values to 0
void UnloadHeightmap(Heightmap *hm) {
	if (hm->data != NULL) {
		free(hm->data);
	}
	
	hm->width = 0;
	hm->height = 0;
	hm->size = 0;
	hm->min = 0;
	hm->max = 0;
	hm->range = 0;
}

// Dump information about heightmap
void ReportHeightmap(Heightmap *hm) {
	fprintf(stderr, "Width: %d\n", hm->width);
	fprintf(stderr, "Height: %d\n", hm->height);
	fprintf(stderr, "Size: %ld\n", hm->size);
	fprintf(stderr, "Min: %d\n", hm->min);
	fprintf(stderr, "Max: %d\n", hm->max);
	fprintf(stderr, "Range: %d\n", hm->range);
}

// positive Y values are "up", rather than "down". This explains why
// I've been getting facet orientation (and object orientation) wrong.
// Subtract all y values from height to correct without changing else.

void Triangle(FILE *fp, const Heightmap *hm,
		unsigned int x1, unsigned int y1, float z1,
		unsigned int x2, unsigned int y2, float z2,
		unsigned int x3, unsigned int y3, float z3) {
	
	// imply normals from face winding
	fprintf(fp, "facet normal 0 0 0\n");
	fprintf(fp, "outer loop\n");
	fprintf(fp, "vertex %f %f %f\n", (float)x1, (float)(hm->height - y1), z1);
	fprintf(fp, "vertex %f %f %f\n", (float)x2, (float)(hm->height - y2), z2);
	fprintf(fp, "vertex %f %f %f\n", (float)x3, (float)(hm->height - y3), z3);
	fprintf(fp, "endloop\n");
	fprintf(fp, "endfacet\n");
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
		Triangle(fp, hm,
				a, 0, CONFIG.baseheight + CONFIG.zscale * hm->data[a],
				b, 0, CONFIG.baseheight + CONFIG.zscale * hm->data[b],
				a, 0, 0);
		Triangle(fp, hm,
				b, 0, CONFIG.baseheight + CONFIG.zscale * hm->data[b],
				b, 0, 0,
				a, 0, 0);
		
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
	else {
		if ((fp = fopen(CONFIG.output, "w")) == NULL) {
			fprintf(stderr, "Cannot open output file %s\n", CONFIG.output);
			return 1;
		}
	}
	
	fprintf(fp, "solid mymesh\n");
	
	Mesh(hm, fp);
	Walls(hm, fp);
	Bottom(hm, fp);
	
	fprintf(fp, "endsolid mymesh\n");
	
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
	
	while ((c = getopt(argc, argv, "z:b:o:i:v")) != -1) {
		switch (c) {
			case 'z':
				// Z scale (heightmap value units relative to XY)
				if (sscanf(optarg, "%f", &CONFIG.zscale) != 1 || CONFIG.zscale <= 0) {
					fprintf(stderr, "CONFIG.zscale must be a number greater than 0.\n");
					return 1;
				}
				break;
			case 'b':
				// Base height (default 1.0 units)
				if (sscanf(optarg, "%f", &CONFIG.baseheight) != 1 || CONFIG.baseheight < 1) {
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
			case 'v':
				// Verbose mode (log to stderr)
				CONFIG.log = 1;
				break;
			case '?':
				// unrecognized option OR missing option argument
				switch (optopt) {
					case 'z':
					case 'b':
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
	Heightmap hm = {0, 0, 0, NULL, 0, 0, 0};
	
	if (parseopts(argc, argv)) {
		fprintf(stderr, "Usage: %s [-z CONFIG.zscale] [-b BASEHEIGHT] [-i INPUT] [-o OUTPUT]\n", argv[0]);
		return 1;
	}
	
	LoadHeightmapFromPGM(&hm);
	PreprocessHeightmap(&hm);
	
	if (CONFIG.log) {
		ReportHeightmap(&hm);
	}
	
	HeightmapToSTL(&hm);
	UnloadHeightmap(&hm);
	
	return 0;
}
