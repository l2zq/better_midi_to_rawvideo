#include "util.h"

int text_init(ui32 *frame, ui16 frame_w, ui16 frame_h);
int text_free();

void text_drawChr(ui16 x, ui16 y, char ch);
void text_drawTxt(ui16 x, ui16 y, const char *text);