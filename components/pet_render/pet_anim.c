#include "pet_render.h"

static uint32_t mix(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

uint32_t pet_anim_duration_ms(pet_anim_t a)
{
    switch (a) {
    case ANIM_BOUNCE: return 600;
    case ANIM_EAT:    return 1200;
    default:          return 0;
    }
}

void pet_pose(pet_view_t *v, pet_anim_t a, uint32_t t_ms)
{
    t_ms = t_ms / PET_FRAME_MS * PET_FRAME_MS;
    v->squash = false; v->dy = 0; v->look_x = 0; v->blink = false;
    v->mouth_open = 0; v->food = 0; v->zstep = 0;
    switch (a) {
    case ANIM_IDLE: {
        v->squash = (t_ms % 2400) >= 1200;                       /* respirar */
        uint32_t slot = t_ms / 5000, off = t_ms % 5000;
        uint32_t b = 1000 + mix(v->seed ^ slot) % 3600;           /* parpadeo cada 1-5 s */
        v->blink = off >= b && off < b + 120;
        uint32_t ls = t_ms / 2500;                                /* sacadas de mirada */
        uint32_t r = mix(v->seed ^ (ls * 7919U) ^ 0x55) % 4;
        v->look_x = r == 0 ? -1 : (r == 1 ? 1 : 0);
        break;
    }
    case ANIM_BOUNCE: {
        uint32_t t = t_ms % 600;
        int u = (int)(t * 100 / 600);                              /* 0..99 */
        int h = 4 * u * (100 - u) * 5 / 10000;                     /* parabola, 5 px */
        v->dy = (int8_t)-h;
        v->squash = u >= 88;                                       /* aplasta al caer */
        break;
    }
    case ANIM_EAT: {
        uint32_t t = t_ms % 1200;
        v->mouth_open = ((t / 200) % 2) == 0;                      /* abre/cierra 3 veces */
        v->food = (uint8_t)(4 - t / 300);                          /* el bocado se encoge */
        v->expr = EXPR_HAPPY;
        break;
    }
    case ANIM_SLEEP:
        v->expr = EXPR_SLEEP;
        v->squash = (t_ms % 3600) >= 1800;
        v->zstep = (uint8_t)((t_ms / 500) % 4);
        break;
    default: break;
    }
}
