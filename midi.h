#pragma once
#include "util.h" // bool, byte, uiXX, size_t, endian, malloc
// this library is not MT-safe
#define MIDI_OK 0             // no error
#define MIDI_E_FTRUNC 1       // error: file truncated
#define MIDI_E_NOHEAD 2       // error: first chunk is not MThd
#define MIDI_E_HDRLEN 3       // error: MThd chunk length < 6
#define MIDI_E_TWOHDR 4       // error: more than one MThd chunk
#define MIDI_E_TRKCNT 5       // error: more MTrk chunk than defined in MThd
#define MIDI_E_SEE_ERRNO -1   // error: see errno
#define MIDI_MAXDT 0x0fffffff // 4 * 7 = 28
#define MAGIC_MTHD 0x4d546864 // 'M' 'T' 'h' 'd'
#define MAGIC_MTRK 0x4d54726b // 'M' 'T' 'r' 'k'
typedef struct midi_track {
  byte *ptr, *end;
  byte run_status;
  ui32 last_delta; // user set - last delta-time read
  bool track_left; // user set - if data left in track
} midi_track;
typedef struct midi_file {
  ui16 type, ntrk, divs;
  midi_track *trks;
} midi_file;
typedef union midi_event {
  struct { // normal
    byte b, a1, a2;
  };
  struct { // meta OR system-exclusive
    byte _, type;
    ui32 size;
    byte *data;
  } msys;
} midi_event;
// clang-format off
struct midi_global {
  ui16 type, ntrk, divs;  // loaded MIDI file info (\trks)
  midi_track *trks,*trk;  // trk:= to read with mtrk_???()
  midi_event  evt;        // last event read by mtrk_evt()
};
extern struct midi_global midi;
int  midi_init(byte *mem, size_t len);
int  midi_free(void);
ui32 mtrk_dt  (void);
bool mtrk_evt (void); // return ptr < end after read