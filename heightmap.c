#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "heightmap.h"


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
	
	int width, height, depth;
	unsigned char *data;
	Heightmap *hm;
	
	if (path == NULL) {
		data = stbi_load_from_file(stdin, &width, &height, &depth, 1);
	}
	else {
		data = stbi_load(path, &width, &height, &depth, 1);
	}
	
	if (data == NULL) {
		fprintf(stderr, "%s\n", stbi_failure_reason());
		return NULL;
	}
	
	if ((hm = (Heightmap *)malloc(sizeof(Heightmap))) == NULL) {
		fprintf(stderr, "Cannot allocate memory for heightmap structure\n");
		free(data);
		return NULL;
	}
	
	hm->width = (unsigned int)width;
	hm->height = (unsigned int)height;
	hm->size = (unsigned long)width * (unsigned long)height;
	hm->data = data;
	
	ScanHeightmap(hm);
	
	return hm;
}

void FreeHeightmap(Heightmap **hm) {
	
	if (hm == NULL || *hm == NULL) {
		return;
	}
	
	if ((*hm)->data != NULL) {
		stbi_image_free((*hm)->data);
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


