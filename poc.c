// gcc poc.c -o poc

#include <stdio.h>
#include <stdlib.h>

// image is flipped

void scandata(const unsigned char *data, int len, unsigned char *min, unsigned char *max) {
	int i;
	unsigned char dmax = 0, dmin = 254;
	for (i = 0; i < len; i++) {
		if (data[i] < dmin) {
			dmin = data[i];
		}
		if (data[i] > dmax) {
			dmax = data[i];
		}
	}
	*min = dmin;
	*max = dmax;
}

float scalez(unsigned char z) {
	return ((float)z)/3.0;
}

void triangle(
		int ay, int ax, unsigned char az, float azoff,
		int by, int bx, unsigned char bz, float bzoff,
		int cy, int cx, unsigned char cz, float czoff) {
	printf("facet normal 0.0 0.0 0.0\n");
	printf("outer loop\n");

	// a
	printf("vertex %f %f %f\n", (float)ax, (float)ay, scalez(az) + azoff);

	// c
	printf("vertex %f %f %f\n", (float)cx, (float)cy, scalez(cz) + czoff);

	// b
	printf("vertex %f %f %f\n", (float)bx, (float)by, scalez(bz) + bzoff);

	printf("endloop\n");
	printf("endfacet\n");
}

void tessellate(const unsigned char *data, int width, int height, float zoff) {
	int row, col;
	int v0, v1, v2, v3; // offsets
	
	for (row = 0; row < height - 1; row++) {
		for (col = 0; col < width - 1; col++) {
			
			v0 = (row * width) + col;
			v1 = ((row + 1) * width) + col;
			v2 = ((row + 1) * width) + (col + 1);
			v3 = (row * width) + (col + 1);
			
			// northwest triangle of quad
			triangle(
					row, col, data[v0], zoff,
					row + 1, col, data[v1], zoff,
					row, col + 1, data[v3], zoff);
			
			// southeast triangle of quad
			triangle(
					row + 1, col, data[v1], zoff,
					row + 1, col + 1, data[v2], zoff,
					row, col + 1, data[v3], zoff);
			
		}
	}
	
}

// note minimum raster size of 2 x 2

void borders(const unsigned char *data, int width, int height, float zoff) {
	int row, col;
	int a, b, c, d; // offsets
	int bottom = height - 1;
	int right = width - 1;
	
	// top and bottom
	for (col = 0; col < width - 1; col++) {
		
		// top border quad
		a = col;
		b = col + 1;
		
		// upper tri
		triangle(
				0, col, data[a], zoff, 
				0, col + 1, data[b], zoff,
				0, col, 0, 0);
		
		// lower tri
		triangle(
				0, col + 1, data[b], zoff,
				0, col + 1, 0, 0,
				0, col, 0, 0);
		
		// bottom border quad
		
		a = (bottom * width) + col;
		b = (bottom * width) + (col + 1);
		
		// upper tri
		triangle(
				bottom, col, data[a], zoff,
				bottom, col, 0, 0,
				bottom, col + 1, data[b], zoff);
		
		// lower tri
		triangle(
				bottom, col, 0, 0,
				bottom, col + 1, 0, 0,
				bottom, col + 1, data[b], zoff);
	}
	
	for (row = 0; row < height - 1; row++) {
		
		// left border quad
		a = row * width;
		b = (row + 1) * width;
		
		// upper tri
		triangle(
				row, 0, data[a], zoff,
				row, 0, 0, 0,
				row + 1, 0, data[b], zoff);
		
		// lower tri
		triangle(
				row, 0, 0, 0,
				row + 1, 0, 0, 0,
				row + 1, 0, data[b], zoff);
		
		// right border quad
		a = (row * width) + right;
		b = ((row + 1) * width) + right;
		
		// lower tri
		triangle(
				row, right, data[a], zoff,
				row + 1, right, 0, 0,
				row, right, 0, 0);
		
		// upper tri
		triangle(
				row, right, data[a], zoff,
				row + 1, right, data[b], zoff,
				row + 1, right, 0, 0);
	}
}

int main(int argc, const char **args) {
	
	FILE *fp;
	int width, height, depth;
	int len;
	unsigned char *data;//, min, max, range;
	
	if (argc != 2) {
		fprintf(stderr, "usage: %s FILE\n", args[0]);
		exit(1);
	}
	
	fp = fopen(args[1], "r");
	if (fp == NULL) {
		fprintf(stderr, "Cannot open file\n");
		exit(1);
	}
	
	fscanf(fp, "P5 ");
	fscanf(fp, "%d ", &width);
	fscanf(fp, "%d ", &height);
	fscanf(fp, "%d ", &depth);
	
	len = width * height;
	
	
	data = (unsigned char *)malloc(len);
	
	fread(data, len, 1, fp);
	
	//printf("width: %d\nheight: %d\ndepth: %d\nlen: %d\n", width, height, depth, len);
	
	//scandata(data, len, &min, &max);
	//range = max - min;
	
	//printf("min: %d\nmax: %d\n", (int)min, (int)max);
	
	/* so now data is an array of width * height pixel values ranging from min to max */
	
	printf("solid mymesh\n");
	
	tessellate(data, width, height, 10.0);
	borders(data, width, height, 10.0);
	
	triangle(
			0, 0, 0, 0,
			0, width -1, 0, 0,
			height - 1, 0, 0, 0);
	triangle(
			0, width - 1, 0, 0,
			height - 1, width - 1, 0, 0,
			height - 1, 0, 0, 0);
	
	printf("endsolid mymesh\n");
	
	
	free(data);	
	fclose(fp);
	return 0;
}
