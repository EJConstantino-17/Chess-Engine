#include "../inc/history.h"
#include <stdint.h>
int history_gravity(int value, int bonus, int limit) {
    if (limit <= 0) return 0;
    if (value > limit) value = limit;
    if (value < -limit) value = -limit;
    if (bonus > limit) bonus = limit;
    if (bonus < -limit) bonus = -limit;
    int magnitude = bonus < 0 ? -bonus : bonus;
    return value + bonus - (int)((int64_t)value * magnitude / limit);
}
int history_lmr_adjustment(int value) {
    if (value >= 8192) return -2;
    if (value >= 4096) return -1;
    if (value <= -1024) return 1;
    return 0;
}
