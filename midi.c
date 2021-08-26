#include "midi.h"
struct midi_global midi;
int midi_free(void) {
  free(midi.trks);
  return 0;
}
int midi_init(byte *mem, size_t len) {
  byte *end = mem + len;
  ui32 type, size; // chunk
  if (mem + 8 > end)
    return MIDI_E_FTRUNC;
  if ((type = mem_be32(mem)) != MAGIC_MTHD)
    return MIDI_E_NOHEAD;
  if ((size = mem_be32(mem + 4)) < 6) // 6 = sizeof(ui16[3])
    return MIDI_E_HDRLEN;
  mem += 8;
  if (mem + size > end)
    return MIDI_E_FTRUNC;
  midi_track *trak;
  ui16 ntrk, itrk = 0;
  midi.ntrk = ntrk = mem_be16(mem + 2);
  midi.trks = trak = malloc(ntrk * sizeof(midi_track));
  if (trak == NULL && ntrk > 0)
    return MIDI_E_SEE_ERRNO;
  midi.type = mem_be16(mem);
  midi.divs = mem_be16(mem + 4);
  mem += size;
  while (mem < end) {
    if (mem + 8 > end)
      return MIDI_E_FTRUNC + midi_free();
    type = mem_be32(mem);
    size = mem_be32(mem + 4);
    mem += 8;
    if (type == MAGIC_MTHD)
      return MIDI_E_TWOHDR + midi_free();
    if (type == MAGIC_MTRK) {
      if (itrk >= ntrk)
        return MIDI_E_TRKCNT + midi_free();
      trak->ptr = mem;
      trak->end = mem += size;
      trak->track_left = size > 0;
      itrk++, trak++;
    } else
      mem += size;
  }
  if (mem > end || itrk < ntrk)
    return MIDI_E_FTRUNC + midi_free();
  midi.trk = NUL;
  return MIDI_OK;
}
ui32 mtrk_dt(void) {
  byte *p = midi.trk->ptr, b = *(p++);
  ui32 vl = b & 0x7f;
  for (int i = 0; i < 3; i++)
    if (b < 0x80)
      break;
    else
      vl = (vl << 7) | ((b = *(p++)) & 0x7f);
  midi.trk->ptr = p;
  return vl;
}
bool mtrk_evt(void) {
  byte *p = midi.trk->ptr, b = *(p++);
  if (b >= 0xf0) {
    if ((midi.evt.b = b) == 0xff)
      midi.evt.msys.type = *(p++);
    ui32 vl = (b = *(p++)) & 0x7f;
    for (int i = 0; i < 3; i++)
      if (b < 0x80)
        break;
      else
        vl = (vl << 7) | ((b = *(p++)) & 0x7f);
    midi.evt.msys.size = vl;
    midi.evt.msys.data = p;
    p += vl;
  } else {
    if (b < 0x80) {
      midi.evt.a1 = b;
      b = midi.trk->run_status;
    } else {
      midi.trk->run_status = b;
      midi.evt.a1 = *(p++);
    }
    midi.evt.b = b;
    if (b < 0xc0 || b >= 0xe0)
      midi.evt.a2 = *(p++);
  }
  midi.trk->ptr = p;
  return p < midi.trk->end;
}