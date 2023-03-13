# ezimg

## Description

A very simple, lightweight image decompressor (loader).

```c
/* BMP */
unsigned int ezimg_bmp_size(void *in, unsigned int in_size);
int ezimg_bmp_load(
    void *in, unsigned int in_size,
    void *out, unsigned int out_size,
    unsigned int *width, unsigned int *height);

/* PNG */
unsigned int ezimg_png_size(void *in, unsigned int in_size);
int ezimg_png_load(
    void *in, unsigned int in_size,
    void *out, unsigned int out_size,
    unsigned int *width, unsigned int *height);
```

## Usage
1. Define ```EZIMG_IMPLEMENTATION``` and include the library:
```c
#define EZIMG_IMPLEMENTATION
#include "ezimg.h"
```

2. Use it
```c
/**
 * comp_img = compressed image buffer
 * raw_img = raw image buffer (ARGB format)
 */
unsigned int width, height;
raw_img_size = ezimg_bmp_size(comp_image, comp_image_size);
raw_img = malloc(raw_img_size);
ezimg_bmp_load(
    comp_image, comp_image_size,
    raw_img, raw_img_size,
    &width, &height);
```

## Supported image formats

- [x] BMP
- [x] PNG
- [ ] QOI (coming soon)
- [ ] PPM (coming soon)

## Technical details

For the PNG format the ```raw_img_size``` is calculated according to this formula: ```width*height*4 + 1*height```.

So it is a bit bigger than the effective raw img size (effective size: ```width*height*4```), but we need the extra space to accomodate the png filtered data and to avoid to dynamically allocate new memory for it.
