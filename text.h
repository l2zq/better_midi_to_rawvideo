#include "util.h"

#define TEXT_LINE_HEIGHT 16

int text_init(ui32 *frame, ui16 frame_w, ui16 frame_h);
int text_free(void);

byte text_draw_char(ui16 rune, si16 dx, si16 dy);
void text_draw_utf8(const char *str, si16 dx, si16 dy);