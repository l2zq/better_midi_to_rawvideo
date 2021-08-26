#include <errno.h>
#include "tckk.h"
// clang-format off
static size_t   pool_left;
static TCKNote *pool_beg, *pool_end, **pool_items;
static inline void     tckk_del(TCKNote *item){
  if(pool_beg <= item && item < pool_end)
    pool_items[pool_left++] = item;
  else
    free(item);
}
static inline TCKNote *tckk_new(void) {
  if (pool_left > 0)
    return pool_items[--pool_left];
  else
    return malloc(sizeof(TCKNote));
}
no_t    last_id;
TCKList tckk_keys[128];
static TCKArray *tracks;
// clang-format on
int tckk_free(void) {
  int count = 0;
  for (int i = 0; i < 128; i++) {
    TCKNote *note, *next = tckk_keys[i].head.kbd_next;
    while ((note = next)) {
      count++, next = note->kbd_next;
      if (note < pool_beg || note >= pool_end)
        free(note);
    }
  }
  free(tracks), free(pool_beg), free(pool_items);
  return count;
}
int tckk_init(ui16 ntrk, size_t pool_size) {
  int tmp_errno;
  if ((pool_beg = malloc(sizeof(TCKNote) * pool_size)) == NULL && pool_size > 0)
    return -1;
  if ((pool_items = malloc(sizeof(TCKNote *) * pool_size)) == NULL && pool_size > 0) {
    tmp_errno = errno;
    free(pool_beg);
    errno = tmp_errno;
    return -1;
  }
  if ((tracks = malloc(sizeof(TCKArray) * ntrk)) == NULL && ntrk > 0) {
    tmp_errno = errno;
    free(pool_beg);
    free(pool_items);
    errno = tmp_errno;
    return -1;
  }
  last_id = 0;
  pool_end = pool_beg, pool_left = pool_size;
  for (size_t i = 0; i < pool_size; i++)
    pool_items[i] = pool_end++;
  TCKList *k = tckk_keys;
  for (int ik = 0; ik < 128; ik++, k++) {
    k->head.note_id = 0;
    k->head.tck_next = NULL;
    k->head.kbd_next = NULL;
    k->head.kbd_prev = NULL;
    k->tail = &(k->head);
  }
  for (int xt = 0; xt < ntrk; xt++)
    for (int ic = 0, jk; ic < 16; ic++)
      for (jk = 0, k = tracks[xt][ic]; jk < 128; jk++, k++) {
        k->head.tck_next = NULL;
        k->head.kbd_next = NULL;
        k->head.kbd_prev = NULL;
        k->tail = &(k->head);
      }
  return 0;
}
void tckk_keydn(ui16 trak, byte chan, byte key) {
  TCKNote *note = tckk_new();
  TCKList *kkey = tckk_keys + key, *tkey = tracks[trak][chan] + key;
  ((note->kbd_prev = kkey->tail)->kbd_next = note)->kbd_next = NULL;
  kkey->tail = note;
  (tkey->tail->tck_next = note)->tck_next = NULL;
  tkey->tail = note;
  note->trak = trak;
  note->chan = chan;
  note->note_id = ++last_id;
}
no_t tckk_keyup(ui16 trak, byte chan, byte key) {
  TCKList *tkey = tracks[trak][chan] + key, *kkey;
  TCKNote *note = tkey->head.tck_next, *nkprev;
  if (note == NULL)
    return 0;
  kkey = tckk_keys + key, nkprev = note->kbd_prev;
  if ((nkprev->kbd_next = note->kbd_next) == NULL)
    kkey->tail = nkprev;
  else
    note->kbd_next->kbd_prev = nkprev;
  if ((tkey->head.tck_next = note->tck_next) == NULL)
    tkey->tail = &(tkey->head);
  no_t nid = note->note_id;
  tckk_del(note);
  return nid;
}