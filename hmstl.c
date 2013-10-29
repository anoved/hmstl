#include <stdio.h>
#include <stdlib.h>

typedef struct {
	
	unsigned int width, height;
	unsigned long size;
	unsigned char *data;
	
	unsigned char min, max, range;
	
} Heightmap;

float ZSCALE = 1.0;
float ZOFFSET = 1.0; // base padding - absolute amount added to scaled surface Z

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
// return 1 on success, 0 on failure
// modifies hm width, height, size, and allocates hm data on success
int LoadHeightmapFromPGM(Heightmap *hm, const char *pgmpath) {
	
	FILE *fp;
	unsigned int width, height, depth; // depth only supported in unsigned char range (although I suppose we could support larger values, and scale to 8-bit range on load)
	unsigned long size;
	unsigned char *data;
	
	fp = fopen(pgmpath, "r");
	// check open success
	
	// read pgm header
	// should allow comment lines in header
	// should understand binary (P2) as well as ASCII (P5) PGM formats
	fscanf(fp, "P5 ");
	fscanf(fp, "%d ", &width);
	fscanf(fp, "%d ", &height);
	fscanf(fp, "%d ", &depth);
	// check that each element of header is read successfully
	
	size = width * height;
	
	data = (unsigned char *)malloc(size);
	// check that the allocation succeeded
	
	fread(data, size, 1, fp);
	// check that we actually got the expected number of bytes
	
	fclose(fp);
	
	hm->width = width;
	hm->height = height;
	hm->size = size;
	hm->data = data;
	
	return 1;
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
	printf("Width: %d\n", hm->width);
	printf("Height: %d\n", hm->height);
	printf("Size: %ld\n", hm->size);
	printf("Min: %d\n", hm->min);
	printf("Max: %d\n", hm->max);
	printf("Range: %d\n", hm->range);
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
					Ax, Ay, ZOFFSET + ZSCALE * hm->data[A],
					Bx, By, ZOFFSET + ZSCALE * hm->data[B],
					Dx, Dy, ZOFFSET + ZSCALE * hm->data[D]);
			
			// BCD
			Triangle(fp, hm,
					Bx, By, ZOFFSET + ZSCALE * hm->data[B],
					Cx, Cy, ZOFFSET + ZSCALE * hm->data[C],
					Dx, Dy, ZOFFSET + ZSCALE * hm->data[D]);
		}
	}
}

// all mesh z coordinates should have an offset added *after* scaling

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
				a, 0, ZOFFSET + ZSCALE * hm->data[a],
				b, 0, ZOFFSET + ZSCALE * hm->data[b],
				a, 0, 0);
		Triangle(fp, hm,
				b, 0, ZOFFSET + ZSCALE * hm->data[b],
				b, 0, 0,
				a, 0, 0);
		
		// south wall
		a += bottom * hm->width;
		b += bottom * hm->width;
		Triangle(fp, hm,
				col, bottom, ZOFFSET + ZSCALE * hm->data[a],
				col, bottom, 0,
				col + 1, bottom, ZOFFSET + ZSCALE * hm->data[b]);
		Triangle(fp, hm,
				col, bottom, 0,
				col + 1, bottom, 0,
				col + 1, bottom, ZOFFSET + ZSCALE * hm->data[b]);
	}
	
	// west and east walls
	for (row = 0; row < hm->height - 1; row++) {
		
		// west wall
		a = row * hm->width;
		b = (row + 1) * hm->width;
		Triangle(fp, hm,
				0, row, ZOFFSET + ZSCALE * hm->data[a],
				0, row, 0,
				0, row + 1, ZOFFSET + ZSCALE * hm->data[b]);
		Triangle(fp, hm,
				0, row, 0,
				0, row + 1, 0,
				0, row + 1, ZOFFSET + ZSCALE * hm->data[b]);
		
		// east wall
		a += right;
		b += right;
		Triangle(fp, hm,
				right, row, ZOFFSET + ZSCALE * hm->data[a],
				right, row + 1, 0,
				right, row, 0);
		Triangle(fp, hm,
				right, row, ZOFFSET + ZSCALE * hm->data[a],
				right, row + 1, ZOFFSET + ZSCALE * hm->data[b],
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

int HeightmapToSTL(Heightmap *hm, const char *stlpath) {
	
	FILE *fp;
	
	// check open
	fp = fopen(stlpath, "w");
	
	fprintf(fp, "solid mymesh\n");
	
	Mesh(hm, fp);
	Walls(hm, fp);
	Bottom(hm, fp);
	
	fprintf(fp, "endsolid mymesh\n");
	
	fclose(fp);
}

int main(int argc, const char **args) {
	Heightmap hm = {0, 0, 0, NULL, 0, 0, 0};
	
	if (argc < 3 || argc > 5) {
		fprintf(stderr, "Usage: %s PGM STL [ZSCALE=1.0] [OFFSET=1]\n", args[0]);
		exit(1);
	}
	
	if (argc >= 4) {
		sscanf(args[3], "%f", &ZSCALE);
		if (ZSCALE <= 0) {
			fprintf(stderr, "ZSCALE must be greater than zero.\n");
			exit(1);
		}
	}
	
	if (argc == 5) {
		sscanf(args[4], "%f", &ZOFFSET);
		if (ZOFFSET < 1.0) {
			fprintf(stderr, "ZOFFSET must be greater than one.\n");
			exit(1);
		}
	}
	
	LoadHeightmapFromPGM(&hm, args[1]);
	PreprocessHeightmap(&hm);
	ReportHeightmap(&hm);
	
	HeightmapToSTL(&hm, args[2]);
	
	UnloadHeightmap(&hm);
	
	return 0;
}
