/*

ezimg: a lightweight image decompressor library

Usage:

#define EZIMG_IMPLEMENTATION
#include "ezimg.h"

unsigned int width, height;
uncomp_img_size = ezimg_bmp_size(comp_image, comp_image_size);
uncomp_img = malloc(uncomp_img_size);
ezimg_bmp_load(
    comp_image, comp_image_size,
    uncomp_img, uncomp_img_size,
    &width, &height);

Supported formats:
[x] BMP:
[ ] PNGs

 */
#ifndef EZIMG_H
#define EZIMG_H

enum
{
    EZIMG_OK,
    EZIMG_INVALID_IMAGE,
    EZIMG_NOT_ENOUGH_SPACE,
    EZIMG_NOT_SUPPORTED
};

unsigned int ezimg_bmp_size(void *in, unsigned int in_size);

int ezimg_bmp_load(
    void *in, unsigned int in_size,
    void *out, unsigned int out_size,
    unsigned int *width, unsigned int *height);

#ifdef EZIMG_IMPLEMENTATION
#ifndef EZIMG_IMPLEMENTED
#define EZIMG_IMPLEMENTED

#define EZIMG_ABS(x) (((x)<0)?(-(x)):(x))

int
ezimg_least_significant_set_bit(unsigned int value)
{
    int lssb = -1;
    unsigned int test;

    for(test = 0;
        test < 32;
        ++test)
    {
        if(value & (1 << test))
        {
            lssb = test;
            break;
        }
    }

    return(lssb);
}

typedef struct
ezimg_stream
{
    unsigned char *buffer;
    unsigned int buffer_size;
    unsigned char *ptr;
    unsigned char *ptr_end;
} ezimg_stream;

void
ezimg_init_stream(
    ezimg_stream *stream,
    unsigned char *buffer,
    unsigned int buffer_size)
{
    stream->buffer = buffer;
    stream->buffer_size = buffer_size;
    stream->ptr = buffer;
    stream->ptr_end = buffer + buffer_size;
}

unsigned char
ezimg_read_u8(ezimg_stream *stream)
{
    unsigned char byte_read;

    if(stream->ptr >= stream->ptr_end)
    {
        return(0);
    }

    byte_read = *stream->ptr++;
    return(byte_read);
}

unsigned int
ezimg_read_u16(ezimg_stream *stream)
{
    unsigned int value;
    unsigned int lsb, msb;

    lsb = (unsigned int)ezimg_read_u8(stream);
    msb = (unsigned int)ezimg_read_u8(stream);

    value = (msb << 8) | (lsb << 0);
    return(value);
}

unsigned int
ezimg_read_u32(ezimg_stream *stream)
{
    unsigned int value;
    unsigned int lsb, mi1, mi2, msb;

    lsb = (unsigned int)ezimg_read_u8(stream);
    mi1 = (unsigned int)ezimg_read_u8(stream);
    mi2 = (unsigned int)ezimg_read_u8(stream);
    msb = (unsigned int)ezimg_read_u8(stream);

    value = (msb << 24) | (mi2 << 16) | (mi1 << 8) | (lsb << 0);
    return(value);
}

int
ezimg_read_i32(ezimg_stream *stream)
{
    int value;
    unsigned int lsb, mi1, mi2, msb;

    lsb = (unsigned int)ezimg_read_u8(stream);
    mi1 = (unsigned int)ezimg_read_u8(stream);
    mi2 = (unsigned int)ezimg_read_u8(stream);
    msb = (unsigned int)ezimg_read_u8(stream);

    value = (msb << 24) | (mi2 << 16) | (mi1 << 8) | (lsb << 0);
    return(value);
}

int
ezimg_bmp_check_signature(unsigned char sign1, unsigned sign2)
{
    if(sign1 == 'B' && sign2 == 'M')
    {
        return(1);
    }
    return(0);
}

unsigned int
ezimg_bmp_size(void *in, unsigned int in_size)
{
    unsigned char sign1, sign2;
    int width, height;
    unsigned int abs_width, abs_height;
    ezimg_stream stream = {0};

    if(in_size < 54)
    {
        return(0);
    }

    ezimg_init_stream(&stream, in, in_size);
    sign1 = ezimg_read_u8(&stream);
    sign2 = ezimg_read_u8(&stream);
    if(!ezimg_bmp_check_signature(sign1, sign2))
    {
        return(0);
    }

    ezimg_read_u32(&stream);
    ezimg_read_u32(&stream);
    ezimg_read_u32(&stream);
    ezimg_read_u32(&stream);
    width = ezimg_read_i32(&stream);
    height = ezimg_read_i32(&stream);

    abs_width = (unsigned int)EZIMG_ABS(width);
    abs_height = (unsigned int)EZIMG_ABS(height);

    return(abs_width*abs_height*4);
}

int
ezimg_bmp_load(
    void *in, unsigned int in_size,
    void *out, unsigned int out_size,
    unsigned int *width, unsigned int *height)
{
#define XY2OFF(X, Y) (((Y)*absw + (X))*4)
#define YX2OFF(Y, X) (((Y)*absw + (X))*4)
#define PADDING(V, P) ((V)%(P)==0)?(0):((P)-((V)%(P)))
    unsigned char sign1, sign2;
    int w, h;
    unsigned int absw, absh;
    unsigned int x, y, padding, i;
    unsigned int data_offset, data_size;
    unsigned int dib_header_size;
    unsigned int planes, bit_count, compression;
    unsigned int palette_offset;
    unsigned char *data, *palette;
    unsigned char *outp;
    unsigned char r, g, b, a;
    unsigned int rmask, gmask, bmask, amask;
    unsigned int rlssb, glssb, blssb, alssb;
    unsigned int pixel;
    ezimg_stream stream = {0};

    if(in_size < 54)
    {
        return(EZIMG_INVALID_IMAGE);
    }

    ezimg_init_stream(&stream, in, in_size);
    sign1 = ezimg_read_u8(&stream);
    sign2 = ezimg_read_u8(&stream);
    if(!ezimg_bmp_check_signature(sign1, sign2))
    {
        return(EZIMG_INVALID_IMAGE);
    }

    ezimg_read_u32(&stream);
    ezimg_read_u32(&stream);
    data_offset = ezimg_read_u32(&stream);
    dib_header_size = ezimg_read_u32(&stream);

    w = ezimg_read_i32(&stream);
    h = ezimg_read_i32(&stream);
    absw = (unsigned int)EZIMG_ABS(w);
    absh = (unsigned int)EZIMG_ABS(h);

    if(out_size < absw*absh*4)
    {
        return(EZIMG_NOT_ENOUGH_SPACE);
    }

    planes = ezimg_read_u16(&stream);
    bit_count = ezimg_read_u16(&stream);
    compression = ezimg_read_u32(&stream);

    if(planes != 1)
    {
        return(EZIMG_NOT_SUPPORTED);
    }

    if( bit_count != 4 && bit_count != 8 &&
        bit_count != 24 && bit_count != 32)
    {
        return(EZIMG_NOT_SUPPORTED);
    }

    if(compression != 0 && compression != 3)
    {
        return(EZIMG_NOT_SUPPORTED);
    }

    ezimg_read_u32(&stream);
    ezimg_read_u32(&stream);
    ezimg_read_u32(&stream);
    ezimg_read_u32(&stream);
    ezimg_read_u32(&stream);

    rmask = gmask = bmask = amask = 0;
    if(dib_header_size > 40)
    {
        rmask = ezimg_read_u32(&stream);
        gmask = ezimg_read_u32(&stream);
        bmask = ezimg_read_u32(&stream);
        amask = ezimg_read_u32(&stream);
    }

    palette_offset = 14 + dib_header_size;
    palette = (unsigned char *)in + palette_offset;

    data = (unsigned char *)in + data_offset;
    data_size = in_size - data_offset;
    ezimg_init_stream(&stream, data, data_size);

    outp = (unsigned char *)out;
    if(compression == 0 && bit_count == 4)
    {
        if(absw % 2 == 0)
        {
            padding = PADDING(absw/2, 4);
        }
        else
        {
            padding = PADDING(absw/2, 4) - 1;
        }

        for(y = 0;
            y < absh;
            ++y)
        {
            unsigned char i4 = 0;

            for(x = 0;
                x < absw;
                ++x)
            {
                if(x % 2 == 0)
                {
                    i4 = ezimg_read_u8(&stream);
                    i = ((unsigned int)i4 >> 4);
                }
                else
                {
                    i = ((unsigned int)i4 & 0x0f);
                }

                b = *(palette + i*4 + 0);
                g = *(palette + i*4 + 1);
                r = *(palette + i*4 + 2);
                a = *(palette + i*4 + 3);

                *outp++ = 0xff;
                *outp++ = r;
                *outp++ = g;
                *outp++ = b;
            }

            for(x = 0;
                x < padding;
                ++x)
            {
                ezimg_read_u8(&stream);
            }
        }
    }
    else if(compression == 0 && bit_count == 8)
    {
        padding = PADDING(1*absw, 4);
        for(y = 0;
            y < absh;
            ++y)
        {
            for(x = 0;
                x < absw;
                ++x)
            {
                i = (unsigned int)ezimg_read_u8(&stream);
                b = *(palette + i*4 + 0);
                g = *(palette + i*4 + 1);
                r = *(palette + i*4 + 2);
                a = *(palette + i*4 + 3);

                *outp++ = 0xff;
                *outp++ = r;
                *outp++ = g;
                *outp++ = b;
            }

            for(x = 0;
                x < padding;
                ++x)
            {
                ezimg_read_u8(&stream);
            }
        }
    }
    else if(compression == 0 && bit_count == 24)
    {
        padding = PADDING(3*absw, 4);
        for(y = 0;
            y < absh;
            ++y)
        {
            for(x = 0;
                x < absw;
                ++x)
            {
                b = ezimg_read_u8(&stream);
                g = ezimg_read_u8(&stream);
                r = ezimg_read_u8(&stream);

                *outp++ = 0xff;
                *outp++ = r;
                *outp++ = g;
                *outp++ = b;
            }

            for(x = 0;
                x < padding;
                ++x)
            {
                ezimg_read_u8(&stream);
            }
        }
    }
    else if(compression == 0 && bit_count == 32)
    {
        for(y = 0;
            y < absh;
            ++y)
        {
            for(x = 0;
                x < absw;
                ++x)
            {
                b = ezimg_read_u8(&stream);
                g = ezimg_read_u8(&stream);
                r = ezimg_read_u8(&stream);
                a = ezimg_read_u8(&stream);

                *outp++ = 0xff;
                *outp++ = r;
                *outp++ = g;
                *outp++ = b;
            }
        }
    }
    else if(compression == 3 && bit_count == 32)
    {
        rlssb = ezimg_least_significant_set_bit(rmask);
        glssb = ezimg_least_significant_set_bit(gmask);
        blssb = ezimg_least_significant_set_bit(bmask);
        alssb = ezimg_least_significant_set_bit(amask);

        for(y = 0;
            y < absh;
            ++y)
        {
            for(x = 0;
                x < absw;
                ++x)
            {
                a = ezimg_read_u8(&stream);
                b = ezimg_read_u8(&stream);
                g = ezimg_read_u8(&stream);
                r = ezimg_read_u8(&stream);

                pixel = (r << 24) | (g << 16) | (b <<  8) | (a <<  0);
                r = (unsigned char)(((pixel & rmask) >> rlssb) & 0xff);
                g = (unsigned char)(((pixel & gmask) >> glssb) & 0xff);
                b = (unsigned char)(((pixel & bmask) >> blssb) & 0xff);
                a = (unsigned char)(((pixel & amask) >> alssb) & 0xff);

                *outp++ = 0xff;
                *outp++ = r;
                *outp++ = g;
                *outp++ = b;
            }
        }
    }
    else
    {
        return(EZIMG_NOT_SUPPORTED);
    }

    if(h > 0)
    {
        unsigned int half_height = absh / 2;
        unsigned int r1, r2;

        outp = (unsigned char *)out;
        for(y = 0;
            y < half_height;
            ++y)
        {
            r1 = y;
            r2 = absh - 1 - y;

            for(x = 0;
                x < absw;
                ++x)
            {
                a = *(outp + YX2OFF(r2, x) + 0);
                r = *(outp + YX2OFF(r2, x) + 1);
                g = *(outp + YX2OFF(r2, x) + 2);
                b = *(outp + YX2OFF(r2, x) + 3);

                *(outp + YX2OFF(r2, x) + 0) = *(outp + YX2OFF(r1, x) + 0);
                *(outp + YX2OFF(r2, x) + 1) = *(outp + YX2OFF(r1, x) + 1);
                *(outp + YX2OFF(r2, x) + 2) = *(outp + YX2OFF(r1, x) + 2);
                *(outp + YX2OFF(r2, x) + 3) = *(outp + YX2OFF(r1, x) + 3);

                *(outp + YX2OFF(r1, x) + 0) = a;
                *(outp + YX2OFF(r1, x) + 1) = r;
                *(outp + YX2OFF(r1, x) + 2) = g;
                *(outp + YX2OFF(r1, x) + 3) = b;
            }
        }
    }

    if(w < 0)
    {
        unsigned int half_width = absw / 2;

        for(y = 0;
            y < absh;
            ++y)
        {
            for(x = 0;
                x < half_width;
                ++x)
            {
                unsigned int c1, c2;

                c1 = x;
                c2 = absw - 1 - x;

                a = *(outp + XY2OFF(c2, y) + 0);
                r = *(outp + XY2OFF(c2, y) + 1);
                g = *(outp + XY2OFF(c2, y) + 2);
                b = *(outp + XY2OFF(c2, y) + 3);

                *(outp + XY2OFF(c2, y) + 0) = *(outp + XY2OFF(c1, y) + 0);
                *(outp + XY2OFF(c2, y) + 1) = *(outp + XY2OFF(c1, y) + 1);
                *(outp + XY2OFF(c2, y) + 2) = *(outp + XY2OFF(c1, y) + 2);
                *(outp + XY2OFF(c2, y) + 3) = *(outp + XY2OFF(c1, y) + 3);

                *(outp + XY2OFF(c1, y) + 0) = a;
                *(outp + XY2OFF(c1, y) + 1) = r;
                *(outp + XY2OFF(c1, y) + 2) = g;
                *(outp + XY2OFF(c1, y) + 3) = b;
            }
        }
    }

    if(width)
    {
        *width = absw;
    }

    if(height)
    {
        *height = absh;
    }

    return(EZIMG_OK);

#undef PADDING
#undef YX2OFF
#undef XY2OFF
}

#undef EZIMG_ABS

#endif
#endif

#endif
