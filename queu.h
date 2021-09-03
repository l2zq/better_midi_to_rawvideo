#include "util.h"
// separated, for queueing MPQN and NoteCounts "Number with Time"

// clang-format off
typedef struct TNum TNum;
struct TNum {
  TNum *next;
  tk_t  tick;
  byte  type;

  byte  numb_byte;
  ui16  numb_ui16;
  ui32  numb_ui32;
  ui64  numb_ui64;
};
typedef struct TNumList {
  ui32 cnt;
  TNum head, *tail;
} TNumList;

int queu_init(size_t pool_size);
int queu_free(void);
void  TNL_ini (TNumList *lst);
void  TNL_clr (TNumList *lst); // should call this after using TNumList
TNum* TNL_pop (TNumList *lst); // just remove the first element and ret next
TNum* TNL_push(TNumList *lst); // new and push