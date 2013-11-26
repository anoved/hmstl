# Heightmap to STL

`hmstl` is a simple program to convert heightmap images to 3D models. The supported input heightmap image format is [raw PGM](http://netpbm.sourceforge.net/doc/pgm.html). The output format is [STL](http://www.ennex.com/~fabbers/StL.asp).

## Prerequisites

`hmstl` requires [libtrix](https://github.com/anoved/libtrix/), my rudimentary C library for generating STL files from triangle lists.

## Build

Compile `hmstl` with:

	make hmstl

## Usage

By default, `hmstl` can be used as a filter to convert heightmap images on standard input to STL models on standard output. The following options are also supported:

- `-i INPUT` read heightmap image from the specified `INPUT` file. Otherwise, read heightmap image from standard input.
- `-o OUTPUT` write STL data to the specified `OUTPUT` file. Otherwise, write STL data to standard output.
- `-z SCALE` multiple heightmap values by `SCALE`. Default: `1`
- `-b HEIGHT` set base thickness to `HEIGHT`. Default and minimum: `1`
- `-s` terrain surface only; omits base walls and bottom
- `-a` output ASCII STL instead of default binary STL

The following options apply a mask to the heightmap. Only the portion of the heightmap visible through the mask is output. This can be used to generate models of areas with non-rectangular boundaries.

- `-m MASK` load mask image from the specified `MASK` file. Dimensions must match heightmap dimensions.
- `-t THRESHOLD` consider mask values equal to or less than `THRESHOLD` to be opaque. Default: `127` (in range 0..255)
- `-h` as an alternative to `-m`, use the heightmap as its own mask; elevations below `THRESHOLD` are considered masked.
- `-r` reverse mask interpretation (swap transparent and opaque areas)

Supported input image formats include JPG (excluding progressive JPG), PNG, GIF, and BMP. Color images are interpreted as grayscale based on pixel luminance (0.3 R, 0.59 G, 0.11 B).

## Example

[![Test scene heightmap](tests/scene.png)](tests/scene.png)

Create an STL model of `tests/scene.pgm`, the heightmap image above, with the following command. The `-z` option is used to scale height values; an appropriate value depends on dataset resolution and desired exaggeration.

	hmstl -z 0.25 < tests/scene.png > tests/scene.stl

Here is the output displayed in [Meshlab](http://meshlab.sourceforge.net/):

[![Test scene STL file](tests/scene-stl.png)](tests/scene.stl)

Here is a contrived masking example using the same heightmap and a compound oval mask:

	hmstl -z 0.25 -i tests/scene.png -m tests/mask.png -o tests/blob.stl

[![Masked model](tests/blob-stl.png)](tests/blob.stl)

Here is a photo of a Makerbot printing of the [scene-thick](tests/scene-thick.stl) sample model:

[![Printed model of sample scene](tests/scene-thick-print.jpg)](tests/scene-thick-print.jpg)

## License

Freely distributed under an [MIT License](LICENSE). See the `LICENSE` files for details.

## Acknowledgements

Heightmap images are loaded using [Sean Barrett](http://nothings.org/)'s public domain [stb_image.c](http://nothings.org/stb_image.c) library.
