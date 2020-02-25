/*
 * Copyright (C) 2020 Kurdyukov Ilya
 *
 * This file is part of gravity blur
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include <time.h>
#include <sys/time.h>
static int64_t get_time_usec()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (int64_t)time.tv_sec * 1000000 + time.tv_usec;
}

#include "bitmap.h"
#ifndef WASM
#include "pngio.h"
#include "jpegio.h"
#define stricmp stricmp_simple
static int stricmp(const char *s1, const char *s2)
{
    char a, b;
    do
    {
        a = *s1++;
        b = *s2++;
        a += (unsigned)(a - 'a') <= 'z' - 'a' ? 'A' - 'a' : 0;
        b += (unsigned)(b - 'a') <= 'z' - 'a' ? 'A' - 'a' : 0;
    }
    while (a == b && a);
    return a - b;
}
#endif

#include "gravityblur.h"

#define BITMAP_RGB 0
#define BITMAP_YUV 1

static bitmap_t* process_image(bitmap_t *in, gb_opts_t *opts)
{
    int w = in->width, h = in->height, bpp = in->bpp;
    bitmap_t *out;

    if (opts->type_in != opts->type)
    {
        if (opts->type == BITMAP_RGB) YCbCr2RGB(in);
        else RGB2YCbCr(in);
    }
    if (opts->ris)
    {
        bitmap_t *in2 = bitmap_ris_simple(in);
        if (in2)
        {
            int w2 = w + w, h2 = h + h;
            bitmap_t *out2 = bitmap_create(w2, h2, bpp);
            if (out2)
            {
                opts->range += opts->range;
                opts->range += opts->range;
                opts->range *= 256;
                opts->sharp = 1.0f / opts->sharp;
                if (bpp < 3 || opts->flags & 1) gravityblur(in2, out2, opts);
                if (bpp > 3 && opts->flags & 2) gravityblur4(in2, out2, opts);
                else gravityblur3(in2, out2, opts);

                bitmap_free(in2);
                out = bitmap_ris_reduce(out2);
                bitmap_free(out2);
            }
        }
    }
    else
    {
        out = bitmap_create(w, h, bpp);
        if (out)
        {
            opts->range *= 256;
            opts->sharp = 1.0f / opts->sharp;
            if (bpp < 3 || opts->flags & 1) gravityblur(in, out, opts);
            if (bpp > 3 && opts->flags & 2) gravityblur4(in, out, opts);
            else gravityblur3(in, out, opts);
        }
    }
    if (out)
    {
        if (opts->type != opts->type_out)
        {
            if (opts->type_out == BITMAP_RGB) YCbCr2RGB(out);
            else RGB2YCbCr(out);
        }
    }
    return out;
}

#ifndef LESS_CONV
#define LESS_CONV 1
#endif

#ifdef WASM
static char** make_argv(char *str, int *argc_ret)
{
    int i = 0, eol = 0, argc = 1;
    char **argv;
    for (;;)
    {
        char a = str[i++];
        if (!a) break;
        if (eol)
        {
            if (a == eol) eol = 0;
        }
        else
        {
            if (a != ' ')
            {
                eol = ' ';
                if (a == '"' || a == '\'') eol = a;
                argc++;
            }
        }
    }
    *argc_ret = argc;
    argv = (char**)malloc(argc * sizeof(char*));
    if (!argv) return argv;
    argv[0] = NULL;
    eol = 0;
    argc = 1;
    for (;;)
    {
        char a = *str++;
        if (!a) break;
        if (eol)
        {
            if (a == eol)
            {
                str[-1] = 0;
                eol = 0;
            }
        }
        else
        {
            if (a != ' ')
            {
                eol = ' ';
                if (a == '"' || a == '\'')
                {
                    eol = a;
                    str++;
                }
                argv[argc++] = str - 1;
            }
        }
    }
    return argv;
}

EMSCRIPTEN_KEEPALIVE
void* web_process(void *web_ptr, char *cmdline)
{
#else
int main(int argc, char **argv)
{
#endif
    int64_t time = 0;
    bitmap_t *in, *out;

    gb_opts_t opts = { 0 };
    opts.type_in = BITMAP_RGB;
    opts.type = BITMAP_RGB;
    opts.type_out = BITMAP_RGB;
    opts.flags = 0;
    opts.num_iter = 3;
    opts.info = 3;
    opts.range = 10.0f;
    opts.sharp = 50.0f;
    opts.ris = 0;
#ifdef WASM
    int argc = 0;
    char **argv_ptr = make_argv(cmdline, &argc), **argv = argv_ptr;
    in = (bitmap_t*)web_ptr;
#else
    const char *ifn = 0, *ofn = 0;
    const char *ext;
    int read_jpeg = 0, write_jpeg = 0;
    const char *progname = "gravblur";
#endif

    while (argc > 1)
    {
        const char *arg1 = argv[1], *arg2 = argc > 2 ? argv[2] : NULL, *arg = arg1;
        char c;
        if (arg[0] != '-' || !(c = arg[1])) break;
        if (c != '-') switch (c)
            {
            case 'S':
                arg = "--separate";
                c = 0;
                break;
            case 'a':
                arg = "--alpha";
                c = 0;
                break;
            case 'r':
                arg = "--range";
                break;
            case 's':
                arg = "--sharp";
                break;
            case 'n':
                arg = "--niter";
                break;
            case 'i':
                arg = "--info";
                break;
            case 'c':
                arg = "--colorspace";
                break;
            case 'x':
                arg = "--ris";
                c = 0;
                break;
            default:
                c = '-';
            }
        if (c != '-' && arg1[2])
        {
            if (!c) break;
            arg2 = arg1 + 2;
            argc++;
            argv--;
        }

#define CHECKNUM if ((unsigned)(arg2[0] - '0') > 9) break;
        if (!strcmp("--separate", arg))
        {
            opts.flags |= 1;
            argc--;
            argv++;
        }
        else if (!strcmp("--alpha", arg))
        {
            opts.flags |= 2;
            argc--;
            argv++;
        }
        else if (argc > 2 && !strcmp("--range", arg))
        {
            CHECKNUM
            opts.range *= atof(arg2);
            argc -= 2;
            argv += 2;
        }
        else if (argc > 2 && !strcmp("--sharp", arg))
        {
            CHECKNUM
            opts.sharp *= atof(arg2);
            argc -= 2;
            argv += 2;
        }
        else if (argc > 2 && !strcmp("--niter", arg))
        {
            CHECKNUM
            opts.num_iter = atoi(arg2);
            argc -= 2;
            argv += 2;
        }
        else if (argc > 2 && !strcmp("--info", arg))
        {
            CHECKNUM
            opts.info = atoi(arg2);
        }
        else if (!strcmp("--colorspace", arg))
        {
            if (!strcmp("YUV", arg2))
            {
                opts.type = BITMAP_YUV;
            }
            else
            {
                opts.type = BITMAP_RGB;
            }
            argc -= 2;
            argv += 2;
        }
        else if (!strcmp("--ris", arg))
        {
            opts.ris = 1;
            argc--;
            argv++;
        }
        else break;
#undef CHECKNUM
    }

#ifdef WASM
    free(argv_ptr);
    if (argc != 1)
    {
        printf("Unrecognized command line option.\n");
        return NULL;
    }
#else
    if (argc != 3)
    {
        printf(
            "Gravity Blur : Copyright (c) 2020 Ilya Kurdyukov\n"
            "Build date: " __DATE__ "\n"
            "\n"
            "Usage:\n"
            "  %s [options] input.[png|jpg|jpeg] output.[png|jpg|jpeg]\n"
            "\n"
            "Options:\n"
            "  -r, --range f       Gravity range (0.0-20.0)\n"
            "  -s, --sharp f       Sharpness (0.0-2.0)\n"
            "  -n, --niter n       Number of iterations (default is 3)\n"
            "  -c, --colorspace s  Process in s={RGB,YUV} colorspace\n"
            "  -S, --separate      Separate color components\n"
            "  -a, --alpha         Use alpha channel\n"
            "  -x, --ris           Use RIS shema\n"
            "  -i, --info n        Print gravblur debug output:\n"
            "                        0 - silent, 3 - all (default)\n"
            "\n", progname);
        return 1;
    }

    ifn = argv[1];
    ext = strrchr(ifn, '.');
    if (ext && (!stricmp(ext, ".jpg") || !stricmp(ext, ".jpeg"))) read_jpeg = 1;
    ofn = argv[2];
    ext = strrchr(ofn, '.');
    if (ext && (!stricmp(ext, ".jpg") || !stricmp(ext, ".jpeg"))) write_jpeg = 1;

    if (read_jpeg)
    {
        if (LESS_CONV && opts.type == BITMAP_YUV)
        {
            in = bitmap_read_jpeg_yuv(ifn);
            opts.type_in = BITMAP_YUV;
        }
        else
        {
            in = bitmap_read_jpeg(ifn);
        }
        if (!in)
        {
            printf("read_jpeg failed\n");
            return 1;
        }
    }
    else
    {
        in = bitmap_read_png(ifn);
        if (!in)
        {
            printf("read_png failed\n");
            return 1;
        }
    }

    if (LESS_CONV && write_jpeg) opts.type_out = opts.type;
#endif

    opts.range *= 0.01f;
    opts.sharp *= 0.01f;
    if (opts.range > 2.0f) opts.range = 2.0f;
    if (opts.range < 0.0f) opts.range = 0.0f;
    if (opts.sharp > 1.0f) opts.sharp = 1.0f;
    if (opts.sharp < 0.01f) opts.sharp = 0.01f;

    if (opts.info & 1) time = get_time_usec();
    out = process_image(in, &opts);
    if (opts.info & 1)
    {
        time = get_time_usec() - time;
        printf("gravblur: %.2f ms\n", time * 0.001);
    }

#ifdef WASM
    return out;
#else
    if (out)
    {
        if (opts.info & 2) time = get_time_usec();
        if (write_jpeg)
        {
            if (opts.type_out == BITMAP_YUV) bitmap_write_jpeg_yuv(out, ofn);
            else bitmap_write_jpeg(out, ofn);
        }
        else
        {
            bitmap_write_png(out, ofn);
        }
        if (opts.info & 2)
        {
            time = get_time_usec() - time;
            printf("writing %s: %.2f ms\n",
                   write_jpeg ? "jpeg" : "png", time * 0.001);
        }
        bitmap_free(out);
    }
    bitmap_free(in);
    return 0;
#endif
}
