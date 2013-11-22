
/* Portions of this file (pm_getc and pm_getuint) are derived,
 * with modification, from functions defined in Netpbm's fileio.c
 * (http://netpbm.sourceforge.net/) and are used in accordance with
 * the applicable terms and conditions, which appear below:

 * fileio.c - routines to read elements from Netpbm image files
 *
 * Copyright (C) 1988 by Jef Poskanzer.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  This software is provided "as is" without express or
 * implied warranty.
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "heightmap.h"

// Read and return one character from file,
// unless the character encountered marks the beginning of a comment,
// in which case the remainder of the comment line is read and the
// end of line character is returned.
// Returns -1 on error (EOF or other).
char pm_getc(FILE * const file) {
	int ich;
	char ch;
	
	// read a character
	ich = getc(file);
	if (ich == EOF) {
		fprintf(stderr, "EOF/read error reading a byte\n");
		return -1;
	}
	ch = (char) ich;
	
	// if the character marks the beginning of a comment,
	// read the rest of the comment line and return the end of line character
	if (ch == '#') {
		do {
			ich = getc(file);
			if (ich == EOF) {
				fprintf(stderr, "EOF/read error reading a comment byte\n");
				return -1;
			}
			ch = (char) ich;
		} while (ch != '\n' && ch != '\r');
	}
	
	return ch;
}

// Read and return an unsigned int >0 from ifP.
// Discards whitespace and comment lines encountered.
// Returns 0 on error; value > 0 otherwise.
unsigned int pm_getuint(FILE * const ifP) {
/*----------------------------------------------------------------------------
   Read an unsigned integer in ASCII decimal from the file stream
   represented by 'ifP' and return its value.

   If there is nothing at the current position in the file stream that
   can be interpreted as an unsigned integer, issue an error message
   to stderr and abort the program.

   If the number at the current position in the file stream is too
   great to be represented by an 'int' (Yes, I said 'int', not
   'unsigned int' (why?)), issue an error message to stderr and exit.
-----------------------------------------------------------------------------*/
	char ch;
	unsigned int i;

	do {
		ch = pm_getc(ifP);
		if (ch == -1) {
			return 0;
		}
	} while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');

	if (ch < '0' || ch > '9') {
		fprintf(stderr, "invalid character in bitmap header (should be unsigned integer)\n");
		return 0;
	}

	i = 0;
	do {
		unsigned int const digitVal = (unsigned int)(ch - '0');
		
		if (i > (unsigned int)(INT_MAX/10) - digitVal) {
			fprintf(stderr, "integer value in bitmap header too large to process\n");
			return 0;
		}
		
		i = i * 10 + digitVal;
		ch = pm_getc(ifP);
		if (ch == -1) {
			return 0;
		}
		
	} while (ch >= '0' && ch <= '9');

	// if i == 0, we've read a valid unsigned int value,
	// but it's invalid in the context of a bitmap header
	if (i == 0) {
		fprintf(stderr, "invalid bitmap header 0 value\n");
	}
	
	return i;
}

void ScanHeightmap(Heightmap *hm) {
	
	unsigned long i;
	unsigned char min, max;
	
	if (hm == NULL || hm->data == NULL) {
		return;
	}
	
	min = 255;
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
}

// Returns pointer to Heightmap
// Returns NULL on error
Heightmap *ReadHeightmap(const char *path) {
	
	FILE *fp;
	unsigned int width, height, depth;
	unsigned long size;
	unsigned char *data;
	Heightmap *hm;
	
	if (path == NULL) {
		fp = stdin;
	}
	else if ((fp = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Cannot open input file %s\n", path);
		return NULL;
	}
	
	if (fgetc(fp) != 'P' || fgetc(fp) != '5') {
		fprintf(stderr, "Unrecognized file format; must be raw PGM (P5)\n");
		fclose(fp);
		return NULL;
	}
	
	if ((width = pm_getuint(fp)) == 0) {
		fclose(fp);
		return NULL;
	}
		
	if ((height = pm_getuint(fp)) == 0) {
		fclose(fp);
		return NULL;
	}
	
	if ((depth = pm_getuint(fp)) == 0) {
		fclose(fp);
		return NULL;
	}
	
	if (depth > 255) {
		fprintf(stderr, "Unsupported bitmap depth. Max value of 255\n");
		fclose(fp);
		return NULL;
	}
	
	size = width * height;
	
	if ((data = (unsigned char *)malloc(size)) == NULL) {
		fprintf(stderr, "Cannot allocate memory for input bitmap\n");
		fclose(fp);
		return NULL;
	}
	
	if (fread(data, size, 1, fp) != 1) {
		fprintf(stderr, "Cannot read data from input bitmap\n");
		free(data);
		fclose(fp);
		return NULL;
	}
		
	if (fp != stdin) {
		fclose(fp);
	}
	
	if ((hm = (Heightmap *)malloc(sizeof(Heightmap))) == NULL) {
		fprintf(stderr, "Cannot allocate memory for heightmap structure\n");
		free(data);
		return NULL;
	}
	
	hm->width = width;
	hm->height = height;
	hm->size = size;
	hm->data = data;
	
	ScanHeightmap(hm);
	
	return hm;
}

void FreeHeightmap(Heightmap **hm) {
	
	if (hm == NULL || *hm == NULL) {
		return;
	}
	
	if ((*hm)->data != NULL) {
		free((*hm)->data);
	}
	
	free(*hm);
	*hm = NULL;
}

void DumpHeightmap(const Heightmap *hm) {
	fprintf(stderr, "Width: %u\n", hm->width);
	fprintf(stderr, "Height: %u\n", hm->height);
	fprintf(stderr, "Size: %lu\n", hm->size);
	fprintf(stderr, "Min: %d\n", hm->min);
	fprintf(stderr, "Max: %d\n", hm->max);
	fprintf(stderr, "Range: %d\n", hm->range);
}


