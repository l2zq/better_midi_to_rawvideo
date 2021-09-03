#include <errno.h>
#include "queu.h"
// clang-format off
static size_t pool_left;
static TNum  *pool_beg, *pool_end, **pool_items;
static inline TNum *TNL_new() {
  if (pool_left > 0)
    return pool_items[--pool_left];
  else
    return malloc(sizeof(TNum));
}
static inline void  TNL_del(TNum *item){
  if (pool_beg <= item && item < pool_end)
    pool_items[pool_left++] = item;
  else
    free(item);
}
// clang-format on
void TNL_ini(TNumList *lst) {
  lst->cnt = 0;
  lst->tail = &(lst->head);
}
void TNL_clr(TNumList *lst) {
  TNum *item, *next = lst->head.next;
  while ((item = next)) {
    next = item->next;
    TNL_del(item);
  }
  lst->tail = &(lst->head);
  lst->cnt = 0;
}
TNum *TNL_pop(TNumList *lst) {
  TNum *item = lst->head.next, *nex;
  if ((nex = lst->head.next = item->next) == NULL)
    lst->tail = &(lst->head);
  TNL_del(item);
  lst->cnt--;
  return nex;
}
TNum *TNL_push(TNumList *list) {
  TNum *item = TNL_new();
  list->cnt++;
  list->tail->next = item;
  list->tail = item;
  item->next = NULL;
  return item;
}
int queu_free() {
  free(pool_beg);
  free(pool_items);
  return 0;
}
int queu_init(size_t pool_size) {
  int tmp_errno;
  if ((pool_beg = malloc(sizeof(TNum) * pool_size)) == NULL && pool_size > 0)
    return -1;
  if ((pool_items = malloc(sizeof(TNum *) * pool_size)) == NULL && pool_size > 0) {
    tmp_errno = errno;
    free(pool_beg);
    errno = tmp_errno;
    return -1;
  }
  pool_end = pool_beg;
  pool_left = pool_size;
  for (size_t i = 0; i < pool_size; i++)
    pool_items[i] = pool_end++;
  return 0;
}