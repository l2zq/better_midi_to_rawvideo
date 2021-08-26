#include "util.h"
// separated, for queueing MPQN and NoteCounts "Number with Time"

// clang-format off
typedef struct TNum TNum;
struct TNum {
  TNum *next;
  tk_t  tick;
  si32  numb;
};
typedef struct TNumList {
  TNum head, *tail;
} TNumList;

int queu_init(size_t pool_size);
int queu_free(void);
void  TNL_ini (TNumList *lst);
void  TNL_clr (TNumList *lst); // should call this after using TNumList
TNum* TNL_pop (TNumList *lst); // just remove the first element and ret next
TNum* TNL_push(TNumList *lst); // new and push