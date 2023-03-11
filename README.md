# ezimg

## Description

A very simple, lightweight image decompressor (loader).

```c
unsigned int ezimg_bmp_size(void *in, unsigned int in_size);
int ezimg_bmp_load(
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
 * uncomp_img = uncompressed image buffer
 */
unsigned int width, height;
uncomp_img_size = ezimg_bmp_size(comp_image, comp_image_size);
uncomp_img = malloc(uncomp_img_size);
ezimg_bmp_load(
    comp_image, comp_image_size,
    uncomp_img, uncomp_img_size,
    &width, &height);
```

## Supported image formats

- [x] BMP
- [ ] PNG
