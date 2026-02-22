#ifndef SIMULATE_H
#define SIMULATE_H

#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
    float lat;
    float lon;
} position_t;
#pragma pack(pop)

void simulate_init(float center_lat, float center_lon);
position_t simulate_step(void);

#endif
