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
[x] BMP
[x] PNG
    [ ] Transparency
        [x] RGBA
        [ ] RGB (info stored in tRNS chunk)
    [ ] Palette
[ ] QOI
[ ] PPM

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

unsigned int ezimg_png_size(void *in, unsigned int in_size);
int ezimg_png_load(
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
    int big_endian;
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
    stream->big_endian = 0;
}

void
ezimg_init_stream_big(
    ezimg_stream *stream,
    unsigned char *buffer,
    unsigned int buffer_size)
{
    stream->buffer = buffer;
    stream->buffer_size = buffer_size;
    stream->ptr = buffer;
    stream->ptr_end = buffer + buffer_size;
    stream->big_endian = 1;
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

    if(stream->big_endian)
    {
        value = (lsb << 8) | (msb << 0);
    }
    else
    {
        value = (msb << 8) | (lsb << 0);
    }
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

    if(stream->big_endian)
    {
        value = (lsb << 24) | (mi1 << 16) | (mi2 << 8) | (msb << 0);
    }
    else
    {
        value = (msb << 24) | (mi2 << 16) | (mi1 << 8) | (lsb << 0);
    }
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

    if(stream->big_endian)
    {
        value = (lsb << 24) | (mi1 << 16) | (mi2 << 8) | (msb << 0);
    }
    else
    {
        value = (msb << 24) | (mi2 << 16) | (mi1 << 8) | (lsb << 0);
    }

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

int
ezimg_png_check_signature(unsigned char sign[])
{
    if( sign[0] == 137 && sign[1] == 80 &&
        sign[2] == 78 && sign[3] == 71 &&
        sign[4] == 13 && sign[5] == 10 &&
        sign[6] == 26 && sign[7] == 10)
    {
        return(1);
    }
    return(0);
}

unsigned int
ezimg_png_decomp_data_max_size(unsigned int width, unsigned int height)
{
    return((width*height*4) + (1*height));
}

#define EZIMG_CHUNK_START 0x49484452
#define EZIMG_CHUNK_END 0x49454e44
#define EZIMG_CHUNK_IDAT 0x49444154

unsigned int
ezimg_png_size(void *in, unsigned int in_size)
{
    unsigned char signature[8] = {0};
    ezimg_stream stream = {0};
    unsigned int width, height, i;
    unsigned int len, type;

    ezimg_init_stream_big(&stream, in, in_size);

    if(in_size < 8)
    {
        return(0);
    }

    for(i = 0;
        i < 8;
        ++i)
    {
        signature[i] = ezimg_read_u8(&stream);
    }

    if(!ezimg_png_check_signature(signature))
    {
        return(0);
    }

    len = ezimg_read_u32(&stream);
    type = ezimg_read_u32(&stream);

    if(type != EZIMG_CHUNK_START)
    {
        return(0);
    }

    width = ezimg_read_u32(&stream);
    height = ezimg_read_u32(&stream);

    return(ezimg_png_decomp_data_max_size(width, height));
}

#define EZIMG_CHUNK_MAX_ENTRIES 50

typedef struct
ezimg_cstream
{
    unsigned char *chunks[EZIMG_CHUNK_MAX_ENTRIES];
    unsigned int lens[EZIMG_CHUNK_MAX_ENTRIES];

    unsigned int num_chunks;
    unsigned int current_chunk;
    unsigned int current_pos;

    unsigned char buff;
    unsigned char mask;
    int end;
} ezimg_cstream;

unsigned char
ezimg_cread_u8(ezimg_cstream *stream)
{
    unsigned char byte_read;

    if(stream->end)
    {
        return(0);
    }

    if(stream->current_chunk >= stream->num_chunks)
    {
        stream->end = 1;
        return(0);
    }

    if(stream->current_pos >= stream->lens[stream->current_chunk])
    {
        stream->current_chunk += 1;
        stream->current_pos = 0;
    }

    if(stream->current_chunk >= stream->num_chunks)
    {
        stream->end = 1;
        return(0);
    }

    byte_read = *(stream->chunks[stream->current_chunk] + stream->current_pos);
    stream->current_pos += 1;

    return(byte_read);
}

void
ezimg_init_cstream(
    ezimg_cstream *stream,
    unsigned int num_chunks)
{
    stream->num_chunks = num_chunks;
    stream->current_chunk = 0;
    stream->current_pos = 0;

    stream->end = 0;
    stream->mask = 1;
    stream->buff = ezimg_cread_u8(stream);
}

unsigned char
ezimg_next_bit(ezimg_cstream *stream)
{
    unsigned char bit = 0;

    if(stream->end)
    {
        return(0);
    }

    bit = (stream->buff & stream->mask) ? 1 : 0;
    stream->mask <<= 1;
    if(stream->mask == 0)
    {
        stream->mask = 1;
        stream->buff = ezimg_cread_u8(stream);
    }

    return(bit);
}

unsigned int
ezimg_cread_bits(ezimg_cstream *stream, unsigned int count)
{
    unsigned int bits_val, i, bit;

    bits_val = 0;
    for(i = 0;
        i < count;
        ++i)
    {
        bit = (unsigned int)ezimg_next_bit(stream);
        bits_val |= (bit << i);
    }

    return(bits_val);
}

void
ezimg_cflush(ezimg_cstream *stream)
{
    if(stream->mask != 1)
    {
        stream->mask = 1;
        stream->buff = ezimg_cread_u8(stream);
    }
}

/**
       Extra               Extra               Extra
  Code Bits Length(s) Code Bits Lengths   Code Bits Length(s)
  ---- ---- ------     ---- ---- -------   ---- ---- -------
   257   0     3       267   1   15,16     277   4   67-82
   258   0     4       268   1   17,18     278   4   83-98
   259   0     5       269   2   19-22     279   4   99-114
   260   0     6       270   2   23-26     280   4  115-130
   261   0     7       271   2   27-30     281   5  131-162
   262   0     8       272   2   31-34     282   5  163-194
   263   0     9       273   3   35-42     283   5  195-226
   264   0    10       274   3   43-50     284   5  227-257
   265   1  11,12      275   3   51-58     285   0    258
   266   1  13,14      276   3   59-66
*/

int ezimg_deflate_len_table[] = {
    11, 13, 15, 17, 19, 23, 27,
    31, 35, 43, 51, 59, 67, 83,
    99, 115, 131, 163, 195, 227
};

int
ezimg_deflate_len(int code, ezimg_cstream *stream)
{
    int extra;

    if(code < 257 || code > 285)
    {
        return(-1);
    }

    if(code == 285)
    {
        return(258);
    }

    if(code < 265)
    {
        return(code - 254);
    }

    extra = (int)ezimg_cread_bits(stream, (code - 261) / 4);
    return(extra + ezimg_deflate_len_table[code - 265]);
}

/**
        Extra           Extra               Extra
   Code Bits Dist  Code Bits   Dist     Code Bits Distance
   ---- ---- ----  ---- ----  ------    ---- ---- --------
     0   0    1     10   4     33-48    20    9   1025-1536
     1   0    2     11   4     49-64    21    9   1537-2048
     2   0    3     12   5     65-96    22   10   2049-3072
     3   0    4     13   5     97-128   23   10   3073-4096
     4   1   5,6    14   6    129-192   24   11   4097-6144
     5   1   7,8    15   6    193-256   25   11   6145-8192
     6   2   9-12   16   7    257-384   26   12  8193-12288
     7   2  13-16   17   7    385-512   27   12 12289-16384
     8   3  17-24   18   8    513-768   28   13 16385-24576
     9   3  25-32   19   8   769-1024   29   13 24577-32768
*/

int ezimg_deflate_dist_table[] = {
    4, 6, 8, 12, 16, 24, 32, 48,
    64, 96, 128, 192, 256, 384,
    512, 768, 1024, 1536, 2048,
    3072, 4096, 6144, 8192,
    12288, 16384, 24576
};

int
ezimg_deflate_dist(int code, ezimg_cstream *stream)
{
    int extra;

    if(code < 0 || code > 29)
    {
        return(-1);
    }

    if(code < 4)
    {
        return(code + 1);
    }

    extra = ezimg_cread_bits(stream, (code - 2) / 2);
    return(extra + ezimg_deflate_dist_table[code - 4] + 1);
}

#define EZIMG_HTABLE_MAX_ENTRIES 290

typedef struct
ezimg_huff_entry
{
    unsigned int code;
    unsigned int len;
} ezimg_huff_entry;

typedef struct
ezimg_huff
{
    unsigned int max_len;
    unsigned int count;
    ezimg_huff_entry entries[EZIMG_HTABLE_MAX_ENTRIES];
} ezimg_huff;

void
ezimg_reset_huff(ezimg_huff *huff)
{
    unsigned int i;

    for(i = 0;
        i < EZIMG_HTABLE_MAX_ENTRIES;
        ++i)
    {
        huff->entries[i].code = 0;
        huff->entries[i].len = 0;
    }

    huff->count = 0;
}

int
ezimg_huff_decode(ezimg_huff *huff, ezimg_cstream *chunk_stream)
{
    int result;
    unsigned int code, len;
    int i;

    result = -1;
    code = ezimg_cread_bits(chunk_stream, 1) & 0xff;
    len = 1;
    while(len <= huff->max_len)
    {
        for(i = 0;
            i < (int)huff->count;
            ++i)
        {
            if(huff->entries[i].len == len &&  huff->entries[i].code == code)
            {
                result = i;
                break;
            }
        }

        if(result >= 0 || len >= huff->max_len)
        {
            break;
        }

        code <<=  1;
        code |= (ezimg_cread_bits(chunk_stream, 1) & 0xff);
        len += 1;
    }

    return(result);
}

int
ezimg_compute_htable(
    ezimg_cstream *chunk_stream, ezimg_huff *clen_huff,
    unsigned int *htable, unsigned int hcount, unsigned int hmax)
{
    unsigned int i;

    for(i = 0;
        i < hmax;
        ++i)
    {
        htable[i] = 0;
    }

    i = 0;
    while(i < hcount)
    {
        int encoded_len;
        unsigned int rep_count, rep_val;

        encoded_len = ezimg_huff_decode(clen_huff, chunk_stream);
        if(encoded_len < 0 || encoded_len > 18)
        {
            return(0);
        }

        if(encoded_len <= 15)
        {
            rep_val = (unsigned int)encoded_len;
            rep_count = 1;
        }
        else if(encoded_len == 16)
        {
            if(i == 0)
            {
                return(0);
            }

            rep_val = htable[i - 1];
            rep_count = 3 + ezimg_cread_bits(chunk_stream, 2);
        }
        else if(encoded_len == 17)
        {
            rep_val = 0;
            rep_count = 3 + ezimg_cread_bits(chunk_stream, 3);
        }
        else if(encoded_len == 18)
        {
            rep_val = 0;
            rep_count = 11 + ezimg_cread_bits(chunk_stream, 7);
        }
        else
        {
            return(0);
        }

        while(rep_count > 0)
        {
            htable[i++] = rep_val;
            rep_count -= 1;
        }
    }

    if(i != hcount)
    {
        return(0);
    }

    return(1);
}

int
ezimg_compute_huff(
    unsigned int *htable,
    unsigned int hcount,
    ezimg_huff *huff)
{
#define MAX_CODE_LEN 16
    unsigned int codes[MAX_CODE_LEN] = {0};
    unsigned int len_count[MAX_CODE_LEN] = {0};
    unsigned int code, max_len, code_len;
    unsigned int i;

    if(hcount >= EZIMG_HTABLE_MAX_ENTRIES)
    {
        return(0);
    }

    max_len = 0;
    for(i = 0;
        i < hcount;
        ++i)
    {
        code_len = htable[i];
        if(code_len >= EZIMG_HTABLE_MAX_ENTRIES)
        {
            return(0);
        }

        len_count[code_len] += 1;

        if(code_len > max_len)
        {
            max_len = code_len;
        }
    }

    if(max_len > MAX_CODE_LEN)
    {
        return(0);
    }

    code = 0;
    len_count[0] = 0;
    codes[0] = 0;
    for(i = 1;
        i <= max_len;
        ++i)
    {
        code = (code + len_count[i - 1]) << 1;
        codes[i] = code;
    }

    ezimg_reset_huff(huff);

    huff->count = hcount;
    huff->max_len = max_len;
    for(i = 0;
        i < hcount;
        ++i)
    {
        code_len = htable[i];
        huff->entries[i].len = code_len;
        if(code_len == 0)
        {
            huff->entries[i].code = 0;
        }
        else
        {
            huff->entries[i].code = codes[code_len];
        }
        codes[code_len] += 1;
    }

    return(1);

#undef MAX_CODE_LEN
}

unsigned char *
ezimg_decompress_idat(
    ezimg_cstream *chunk_stream,
    unsigned int width,
    unsigned int height,
    unsigned char *buff,
    unsigned int buff_size)
{
#define HLIT_MAX 288
#define HDIST_MAX 32
#define HCLEN_MAX 20
#define HCLEN_ORD_MAX 19

#define EMIT(b)\
    if(outp >= outp_end)\
    {\
        return(0);\
    }\
    *outp++ = (unsigned char)(b);

    unsigned int decomp_data_max_size;
    unsigned char *decomp_data;

    unsigned int comp_method, comp_info, fdict;
    unsigned int is_last, btype;

    unsigned char *outp, *outp_end;

    decomp_data_max_size = ezimg_png_decomp_data_max_size(width, height);
    if(buff_size < decomp_data_max_size)
    {
        return(0);
    }

    decomp_data = buff;
#endif
    outp = decomp_data;
    outp_end = outp + decomp_data_max_size;

    if(!decomp_data)
    {
        return(0);
    }

    comp_method = ezimg_cread_bits(chunk_stream, 4);
    comp_info = ezimg_cread_bits(chunk_stream, 4);

    ezimg_cread_bits(chunk_stream, 5);
    fdict = ezimg_cread_bits(chunk_stream, 1);
    ezimg_cread_bits(chunk_stream, 2);

    if(comp_method != 8 || comp_info > 7 || fdict != 0)
    {
        return(0);
    }

    is_last = 0;
    while(!is_last)
    {
        int to_decode = 0;
        ezimg_huff lit_len_huff = {0};
        ezimg_huff dist_huff = {0};

        is_last = ezimg_cread_bits(chunk_stream, 1);
        btype = ezimg_cread_bits(chunk_stream, 2);

        if(btype == 0)
        {
            /* Uncompressed block */
            unsigned int b0len;
            unsigned int b0nlen;

            to_decode = 0;
            ezimg_cflush(chunk_stream);
            b0len = ezimg_cread_bits(chunk_stream, 16);
            b0nlen = ezimg_cread_bits(chunk_stream, 16);

            if(~b0len != b0nlen)
            {
                return(0);
            }

            while(b0len > 0)
            {
                EMIT(ezimg_cread_bits(chunk_stream, 8));
                --b0len;
            }
        }
        else if(btype == 1)
        {
            /* Block compressed with fixed Huffman tables */
            unsigned int hlit_table[HLIT_MAX] = {0};
            unsigned int hdist_table[HDIST_MAX] = {0};

            unsigned int i;

            to_decode = 1;

            for(i = 0;
                i <= 143;
                ++i)
            {
                hlit_table[i] = 8;
            }

            for(i = 144;
                i <= 255;
                ++i)
            {
                hlit_table[i] = 9;
            }

            for(i = 256;
                i <= 279;
                ++i)
            {
                hlit_table[i] = 7;
            }

            for(i = 280;
                i < HLIT_MAX;
                ++i)
            {
                hlit_table[i] = 8;
            }

            for(i = 0;
                i < HDIST_MAX;
                ++i)
            {
                hdist_table[i] = 5;
            }

            if(!ezimg_compute_huff(hlit_table, HLIT_MAX, &lit_len_huff))
            {
                return(0);
            }

            if(!ezimg_compute_huff(hdist_table, HDIST_MAX, &dist_huff))
            {
                return(0);
            }
        }
        else if(btype == 2)
        {
            /* Block compressed with dynamic Huffman tables */

            unsigned int hlit, hdist, hclen;
            unsigned int hclen_ord[HCLEN_ORD_MAX] = {
                16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
            };

            unsigned int hclen_table[HCLEN_MAX] = {0};
            unsigned int hlit_table[HLIT_MAX] = {0};
            unsigned int hdist_table[HDIST_MAX] = {0};

            ezimg_huff clen_huff;

            unsigned int i;

            to_decode = 1;

            hlit = ezimg_cread_bits(chunk_stream, 5);
            hdist = ezimg_cread_bits(chunk_stream, 5);
            hclen = ezimg_cread_bits(chunk_stream, 4);

            if(hclen > HCLEN_ORD_MAX)
            {
                return(0);
            }

            hlit += 257;
            hdist += 1;
            hclen += 4;

            for(i = 0;
                i < hclen;
                ++i)
            {
                hclen_table[hclen_ord[i]] = ezimg_cread_bits(chunk_stream, 3);
            }

            if(!ezimg_compute_huff(hclen_table, HCLEN_MAX, &clen_huff))
            {
                return(0);
            }

            if(!ezimg_compute_htable(
                chunk_stream, &clen_huff,
                hlit_table, hlit, HLIT_MAX))
            {
                return(0);
            }

            if(!ezimg_compute_huff(hlit_table, hlit, &lit_len_huff))
            {
                return(0);
            }

            if(!ezimg_compute_htable(
                chunk_stream, &clen_huff,
                hdist_table, hdist, HDIST_MAX))
            {
                return(0);
            }

            if(!ezimg_compute_huff(hdist_table, hdist, &dist_huff))
            {
                return(0);
            }
        }
        else
        {
            return(0);
        }

        if(to_decode)
        {
            int lit_len;

            lit_len = ezimg_huff_decode(&lit_len_huff, chunk_stream);
            while(lit_len != 256)
            {
                if(lit_len < 0)
                {
                    return(0);
                }
                else if(lit_len < 256)
                {
                    unsigned int value;
                    value = (unsigned int)lit_len;
                    EMIT(value);
                }
                else if(lit_len > 256)
                {
                    int len, dist;
                    unsigned char *backp;

                    len = ezimg_deflate_len(lit_len, chunk_stream);
                    dist = ezimg_huff_decode(&dist_huff, chunk_stream);
                    if(dist < 0)
                    {
                        return(0);
                    }

                    dist = ezimg_deflate_dist(dist, chunk_stream);
                    if(dist < 0 || len <= 0)
                    {
                        return(0);
                    }

                    backp = outp - dist;
                    if(backp < decomp_data)
                    {
                        return(0);
                    }

                    while(len > 0)
                    {
                        EMIT(*backp);
                        ++backp;
                        len -= 1;
                    }
                }
            
                lit_len = ezimg_huff_decode(&lit_len_huff, chunk_stream);
            }
        }
    }

    return(decomp_data);

#undef EMIT

#undef HCLEN_ORD_MAX
#undef HLIT_MAX
#undef HDIST_MAX
#undef HCLEN_MAX
}

unsigned char
ezimg_png_filter1(
    unsigned char *src,
    unsigned char *a, 
    unsigned int channel)
{
    unsigned char result = 0;

    result = (unsigned char)src[channel] + (unsigned char)a[channel];

    return(result);
}

unsigned char
ezimg_png_filter2(
    unsigned char *src,
    unsigned char *b,
    unsigned int channel)
{
    unsigned char result = 0;

    result = (unsigned char)src[channel] + (unsigned char)b[channel];

    return(result);
}

unsigned char
ezimg_png_filter3(
    unsigned char *src,
    unsigned char *a, 
    unsigned char *b,
    unsigned int channel)
{
    unsigned char result = 0;
    unsigned int avg;

    avg = (((unsigned int)a[channel] + (unsigned int)b[channel]) / 2);
    result = (unsigned char)src[channel] + (unsigned char)avg;

    return(result);
}

unsigned char
ezimg_png_filter4(
    unsigned char *src,
    unsigned char *a, 
    unsigned char *b,
    unsigned char *c,
    unsigned int channel)
{
    unsigned char result = 0;
    int p, pa, pb, pc;
    unsigned char pr;

    p = (int)a[channel] + (int)b[channel] - (int)c[channel];
    pa = p - (int)a[channel];
    pb = p - (int)b[channel];
    pc = p - (int)c[channel];

    if(pa < 0) pa = -pa;
    if(pb < 0) pb = -pb;
    if(pc < 0) pc = -pc;

    pr = 0;
    if((pa <= pb) && (pa <= pc)) pr = a[channel];
    else if(pb <= pc) pr = b[channel];
    else pr = c[channel];

    result = (unsigned char)src[channel] + (unsigned char)pr;
    return(result);
}

int
ezimg_png_filter(
    unsigned char *decomp_data,
    unsigned int width,
    unsigned int height)
{
    unsigned char *src, *dst, *prev_row, *curr_row;
    unsigned int x, y;
    unsigned char filter; 
    unsigned int a_pixel;
    unsigned char *b_pixel;
    unsigned int c_pixel;
    unsigned char zero_pixel[4] = {0};
    unsigned int prev_row_advance;

    prev_row = (unsigned char *)&(zero_pixel[0]);
    src = decomp_data;
    dst = decomp_data;

    prev_row_advance = 0;
    for(y = 0;
        y < height;
        ++y)
    {
        filter = *src++;
        curr_row = dst;

        if(y == 7)
        {
            curr_row = 0;
            curr_row = dst;
        }

        if(filter == 0)
        {
            for(x = 0;
                x < width;
                ++x)
            {
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
                *dst++ = *src++;
            }
        }
        else if(filter == 1)
        {
            a_pixel = 0;
            for(x = 0;
                x < width;
                ++x)
            {
                dst[0] = ezimg_png_filter1(src, (unsigned char *)&a_pixel, 0);
                dst[1] = ezimg_png_filter1(src, (unsigned char *)&a_pixel, 1);
                dst[2] = ezimg_png_filter1(src, (unsigned char *)&a_pixel, 2);
                dst[3] = ezimg_png_filter1(src, (unsigned char *)&a_pixel, 3);

                a_pixel = (*((unsigned int *)dst) & 0xffffffff);

                dst += 4;
                src += 4;
            }
        }
        else if(filter == 2)
        {
            b_pixel = prev_row;
            for(x = 0;
                x < width;
                ++x)
            {
                dst[0] = ezimg_png_filter2(src, b_pixel, 0);
                dst[1] = ezimg_png_filter2(src, b_pixel, 1);
                dst[2] = ezimg_png_filter2(src, b_pixel, 2);
                dst[3] = ezimg_png_filter2(src, b_pixel, 3);

                b_pixel += prev_row_advance;
                dst += 4;
                src += 4;
            }
        }
        else if(filter == 3)
        {
            a_pixel = 0;
            b_pixel = prev_row;
            for(x = 0;
                x < width;
                ++x)
            {
                dst[0] = ezimg_png_filter3(src, (unsigned char *)&a_pixel, b_pixel, 0);
                dst[1] = ezimg_png_filter3(src, (unsigned char *)&a_pixel, b_pixel, 1);
                dst[2] = ezimg_png_filter3(src, (unsigned char *)&a_pixel, b_pixel, 2);
                dst[3] = ezimg_png_filter3(src, (unsigned char *)&a_pixel, b_pixel, 3);

                a_pixel = (*((unsigned int *)dst) & 0xffffffff);

                b_pixel += prev_row_advance;
                dst += 4;
                src += 4;
            }
        }
        else if(filter == 4)
        {
            a_pixel = 0;
            b_pixel = prev_row;
            c_pixel = 0;
            for(x = 0;
                x < width;
                ++x)
            {
                dst[0] = ezimg_png_filter4(src, (unsigned char *)&a_pixel, b_pixel, (unsigned char *)&c_pixel, 0);
                dst[1] = ezimg_png_filter4(src, (unsigned char *)&a_pixel, b_pixel, (unsigned char *)&c_pixel, 1);
                dst[2] = ezimg_png_filter4(src, (unsigned char *)&a_pixel, b_pixel, (unsigned char *)&c_pixel, 2);
                dst[3] = ezimg_png_filter4(src, (unsigned char *)&a_pixel, b_pixel, (unsigned char *)&c_pixel, 3);

                c_pixel = (*((unsigned int *)b_pixel) & 0xffffffff);
                a_pixel = (*((unsigned int *)dst) & 0xffffffff);

                b_pixel += prev_row_advance;
                dst += 4;
                src += 4;
            }
        }
        else
        {
            return(0);
        }

        prev_row = curr_row;
        prev_row_advance = 4;
    }

    return(1);
}

int
ezimg_png_load(
    void *in, unsigned int in_size,
    void *out, unsigned int out_size,
    unsigned int *width, unsigned int *height)
{
    unsigned char signature[8] = {0};
    unsigned int w = 0, h = 0;
    unsigned int bit_count, color_type = 0, compression, filter, interlace;
    int first_chunk, last_chunk;
    unsigned char *chunk_data, *next_chunk;
    unsigned int i, x, y;
    unsigned int idat_chunk_index;
    unsigned int decomp_data_max_size;

    unsigned char *decomp_data;

    ezimg_stream stream = {0};
    ezimg_cstream cstream = {0};

    ezimg_init_stream_big(&stream, in, in_size);

    if(in_size < 8)
    {
        return(EZIMG_INVALID_IMAGE);
    }

    for(i = 0;
        i < 8;
        ++i)
    {
        signature[i] = ezimg_read_u8(&stream);
    }

    if(!ezimg_png_check_signature(signature))
    {
        return(EZIMG_INVALID_IMAGE);
    }

    idat_chunk_index = 0;
    first_chunk = 1;
    last_chunk = 0;
    while(!last_chunk)
    {
        unsigned int len, type;

        len = ezimg_read_u32(&stream);
        type = ezimg_read_u32(&stream);

        chunk_data = (unsigned char *)stream.ptr;
        next_chunk = chunk_data + len + 4;

        if(first_chunk && type != EZIMG_CHUNK_START)
        {
            return(EZIMG_INVALID_IMAGE);
        }

        if(type == EZIMG_CHUNK_START)
        {
            w = ezimg_read_u32(&stream);
            h = ezimg_read_u32(&stream);
            bit_count = ezimg_read_u8(&stream);
            color_type = ezimg_read_u8(&stream);
            compression = ezimg_read_u8(&stream);
            filter = ezimg_read_u8(&stream);
            interlace = ezimg_read_u8(&stream);

            if(w == 0 || h == 0)
            {
                return(EZIMG_INVALID_IMAGE);
            }

            if( bit_count != 8 || (color_type != 2 && color_type != 6) ||
                compression != 0 || filter != 0 ||
                interlace != 0)
            {
                return(EZIMG_NOT_SUPPORTED);
            }
        }
        else if(type == EZIMG_CHUNK_END)
        {
            last_chunk = 1;
        }
        else if(type == EZIMG_CHUNK_IDAT)
        {
            if(idat_chunk_index >= EZIMG_CHUNK_MAX_ENTRIES)
            {
                return(EZIMG_NOT_SUPPORTED);
            }

            cstream.chunks[idat_chunk_index] = chunk_data;
            cstream.lens[idat_chunk_index] = len;
            idat_chunk_index += 1;
        }

        first_chunk = 0;

        ezimg_init_stream_big(
            &stream, next_chunk,
            in_size - (unsigned int)(next_chunk - (unsigned char *)in));
    }

    if(idat_chunk_index == 0)
    {
        return(EZIMG_INVALID_IMAGE);
    }

    /* Decompress IDAT chunk(s) */
    ezimg_init_cstream(&cstream, idat_chunk_index);
    decomp_data = ezimg_decompress_idat(&cstream, w, h, out, out_size);
    if(!decomp_data)
    {
        return(EZIMG_INVALID_IMAGE);
    }
    decomp_data_max_size = ezimg_png_decomp_data_max_size(w, h);

    /* RGB to RGBA */
    if(color_type == 2)
    {
        for(y = 0;
            y < h;
            ++y)
        {
            for(x = 0;
                x < w;
                ++x)
            {
                unsigned int pos = 1 + x*4 + 3;
                for(i = decomp_data_max_size - 1;
                    i > pos;
                    --i)
                {
                    decomp_data[i] = decomp_data[i - 1];
                }
                decomp_data[pos] = 0;
            }
        }
    }

    /* Reconstruct filters */
    if(!ezimg_png_filter(decomp_data, w, h))
    {
        return(EZIMG_INVALID_IMAGE);
    }

    if(width)
    {
        *width = w;
    }

    if(height)
    {
        *height = h;
    }

    return(EZIMG_OK);
}

#undef EZIMG_CHUNK_IDAT
#undef EZIMG_CHUNK_END
#undef EZIMG_CHUNK_START

#undef EZIMG_HTABLE_MAX_ENTRIES

#undef EZIMG_CHUNK_MAX_ENTRIES

#undef EZIMG_ABS

#endif
#endif
