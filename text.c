#include "text.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static struct {
  ui32 *offs;
  byte *data;
} font;

static struct {
  ui32 *dat;
  ui16 w, h;
} frame;

int text_free(void) {
  free(font.offs);
  return 0;
}
int text_init(ui32 *fptr, ui16 frame_w, ui16 frame_h) {
  frame.dat = fptr;
  frame.w = frame_w;
  frame.h = frame_h;
  int ret = -1, tmp_errno = 0;
  FILE *fp = fopen("unifont.bin", "rb");
  if (fp != NULL) {
    long length;
    if (fseek(fp, 0, SEEK_END) == 0 && (length = ftell(fp)) != -1 && fseek(fp, 0, SEEK_SET) == 0) {
      if ((font.offs = malloc(length)) != NULL) {
        if (fread(font.offs, 1, length, fp) == length) {
          font.data = ((byte *)font.offs) + (sizeof(ui32) * 65536);
          ret = 0;
        } else
          tmp_errno = errno, free(font.offs);
      } else
        tmp_errno = errno;
    } else
      tmp_errno = errno;
    fclose(fp);
  }
  errno = tmp_errno;
  return ret;
}

static void draw_pixel(ui16 x, ui16 y, byte value) {
  if (x > frame.w || y > frame.h)
    return;
  ui16 src = value ? 0xff : 0x80, dst;
  byte *tgt = (byte *)(frame.dat + y * frame.w + x);
  for (int i = 0; i < 3; i++) {
    dst = tgt[i];
    tgt[i] = dst / 8 + src * 7 / 8;
  }
}

byte text_draw_char(ui16 rune, si16 dx, si16 dy) {
  byte charWidth = 1;
  if (font.offs[rune] == 0xffffffff)
    rune = '?';
  ui32 offset = font.offs[rune];
  if (offset & (1 << 31)) {
    charWidth = 2;
    offset &= ~(1 << 31);
  }
  si16 x, y;
  byte *charData = font.data + offset, currByte;
  for (ui16 fy = 0; fy < TEXT_LINE_HEIGHT; fy++) {
    for (ui16 bi = 0, fx = 0; bi < charWidth; bi++) {
      currByte = *(charData++);
      for (ui16 bx = 0; bx < 8; bx++, fx++)
        if ((x = dx + fx) >= 0 && (y = dy + fy) >= 0)
          draw_pixel(x, y, currByte & (1 << (7 - bx)));
    }
  }
  return charWidth * 8;
}
void text_draw_utf8(const char *str, si16 dx, si16 dy) {
  byte *p = (byte *)str;
  ui16 rune = 0, left = dx;
  while (*p) {
    rune = 0;
    byte b1 = *p;
    p++;
    if ((b1 & 0x80) == 0) {
      rune = b1;
    } else {
      byte b2 = *p;
      p++;
      if (b2 == 0) {
        break;
      }
      if ((b1 & 0xE0) == 0xC0) {
        rune = ((b1 & 0x1F) << 6) | (b2 & 0x3F);
      } else {
        byte b3 = *p;
        p++;
        if (b3 == 0) {
          break;
        }
        if ((b1 & 0xf0) == 0xE0) {
          rune =
              ((b1 & 0x0F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
        } else {
          break;
        }
      }
    }
    if (rune == '\n')
      dx = left, dy += TEXT_LINE_HEIGHT;
    else {
      byte charWidth = text_draw_char(rune, dx, dy);
      dx += charWidth;
    }
  }
}