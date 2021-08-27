#include "text.h"

#include "font-8x16.c.txt"
// #include "font-16x32.c.txt"

#define BGRA(R, G, B) (0xFF000000 + ((R) << 16) + ((G) << 8) + (B))
// very dirty

static const int chr_gap = 1;
static const int font_w = 8, font_h = 16, font_cnt = 256; // should be times of 8
#define FONT_ARRAY console_font_8x16

static ui32 *frame_ptr, *frame_end;
static byte *font_data;
static ui32 frame_width, frame_height;
static ui32 char_size;

int text_font_w, text_font_h;

static const ui32 color_fg = BGRA(0xff, 0xff, 0xff);
static const ui32 color_bg = BGRA(0x80, 0x80, 0x80);

int text_free() {
  free(font_data);
  return 0;
}
int text_init(ui32 *frame, ui16 frame_w, ui16 frame_h) {
  frame_ptr = frame;
  frame_end = frame + frame_w * frame_h;
  frame_width = frame_w;
  frame_height = frame_h;
  char_size = font_w * font_h;

  text_font_w = font_w;
  text_font_h = font_h;

  font_data = malloc(char_size * font_cnt);
  if (font_data == NULL)
    return -1;
  for (int i = 0, off = 0; i < char_size * font_cnt / 8; i++)
    for (int j = 7; j >= 0; j--, off++)
      if (FONT_ARRAY[i] & (1 << j))
        font_data[off] = 1;
      else
        font_data[off] = 0;
  return 0;
}

static inline void mix_color(byte *dest, byte *source) {
  ui16 dst, src;
  for (int i = 0; i < 3; i++) {
    dst = dest[i];
    src = source[i];
    dest[i] = dst / 8 + src * 7 / 8;
  }
}
void text_fillGap(ui16 x, ui16 y, ui16 w) {
  ui32 *draw_ptr = frame_ptr + y * frame_width + x;
  for (ui16 dy = y, fy = 0; dy < frame_height && fy < font_h; dy++, fy++)
    for (ui16 dx = x, fx = 0; dx < frame_width && fx < w; dx++, fx++)
      // mix_color((byte *)(draw_ptr + fy * frame_width + fx), (byte *)&color_bg);
      draw_ptr[fy * frame_width + fx] = color_bg;
}
void text_drawChr(ui16 x, ui16 y, char ch) {
  ui32 color;
  ui32 *draw_ptr = frame_ptr + y * frame_width + x;
  byte *font_ptr = font_data + ch * char_size;
  for (ui16 dy = y, fy = 0; dy < frame_height && fy < font_h; dy++, fy++)
    for (ui16 dx = x, fx = 0; dx < frame_width && fx < font_w; dx++, fx++) {
      if (font_ptr[fy * font_w + fx])
        color = color_fg;
      else
        color = color_bg;
      // mix_color((byte *)(draw_ptr + fy * frame_width + fx), (byte *)&color);
      draw_ptr[fy * frame_width + fx] = color;
    }
}
void text_drawTxt(ui16 x, ui16 y, const char *text) {
  char ch;
  ui16 lx = x;
  // bool fol = true; // first of line
  while ((ch = *(text++)) != '\0') {
    switch (ch) {
    case '\n':
      // fol = true;
      y += font_h, x = lx;
      break;
    default:
      // if (!fol)
      // text_fillGap(x, y, chr_gap), x += chr_gap;
      text_drawChr(x, y, ch), x += font_w;
    }
  }
}
