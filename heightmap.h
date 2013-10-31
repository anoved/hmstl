#ifndef _HEIGHTMAP_H
#define _HEIGHTMAP_H

typedef struct {
	
	// xy dimensions (size = width * height)
	unsigned int width, height;
	unsigned long size;
	
	// z dimensions (range = max - min = relief)
	unsigned char min, max, range;
	
	// raster with size pixels ranging in value from min to max
	unsigned char *data;
	
} Heightmap;

Heightmap *ReadHeightmap(const char *path);
void FreeHeightmap(Heightmap *hm);
void DumpHeightmap(const Heightmap *hm);

#endif
