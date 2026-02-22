#include "simulate.h"
#include <math.h>

static float s_center_lat;
static float s_center_lon;
static float s_angle;

#define RADIUS 0.001f   /* ~111 meters in degrees */
#define STEP   0.05f    /* radians per call */

void simulate_init(float center_lat, float center_lon)
{
    s_center_lat = center_lat;
    s_center_lon = center_lon;
    s_angle = 0.0f;
}

position_t simulate_step(void)
{
    position_t pos;
    pos.lat = s_center_lat + RADIUS * cosf(s_angle);
    pos.lon = s_center_lon + RADIUS * sinf(s_angle);
    s_angle += STEP;
    if (s_angle >= 2.0f * M_PI)
        s_angle -= 2.0f * M_PI;
    return pos;
}
