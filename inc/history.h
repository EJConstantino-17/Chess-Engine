#ifndef CCE_HISTORY_H
#define CCE_HISTORY_H
#define HISTORY_TUNED_LIMIT 16384
int history_gravity(int value, int bonus, int limit);
int history_lmr_adjustment(int value);
#endif
