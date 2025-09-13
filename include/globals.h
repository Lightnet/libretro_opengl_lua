#ifndef GLOBALS_H
#define GLOBALS_H

#include <GL/gl.h>
// #include "libretro.h"
#include "stb_truetype.h"

extern unsigned width;
extern unsigned height;
extern GLuint prog;
extern GLuint vbo;
extern GLuint text_prog;
extern GLuint text_vbo;
extern GLuint font_texture;
extern stbtt_bakedchar cdata[96]; // ASCII 32..126
extern unsigned char *font_bitmap;
extern int font_bitmap_width;
extern int font_bitmap_height;

#endif