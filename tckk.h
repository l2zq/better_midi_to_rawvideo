#pragma once
#include "util.h" // byte, uiXX, size_t, malloc
typedef struct TCKNote TCKNote;
typedef struct TCKList TCKList;
struct TCKNote {
  TCKNote *tck_next;
  TCKNote *kbd_next, *kbd_prev;
  ui16 trak;
  byte chan;
  no_t note_id;
};
struct TCKList {
  TCKNote head, *tail;
};
typedef TCKList TCKArray[16][128]; // clang-format off
extern TCKList  tckk_keys[128];
int  tckk_init (ui16 ntrk, size_t pool_size);
int  tckk_free (void);
void tckk_keydn(ui16 trak, byte chan, byte key);
no_t tckk_keyup(ui16 trak, byte chan, byte key); // return id of the up note