/* uisim: host simulator of the real LVGL UI (components/pet_app/pet_ui*.c).
 * Owns the simulated world (g_pet, clocks, battery, touch) and drives the UI only through
 * the interface in pet_priv.h. Two modes:
 *   uisim script <file> <outdir>   headless, scripted input, PNG screenshots
 *   uisim run [-z N] [seed]        SDL2 window, mouse = touch
 * See README.md for the script commands and keys. */
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "pet_priv.h"
#include "png.h"

/* ======================================================================================
 * Fields settable with `set <field> <value>`. The ONE place to add new pet_state_t fields.
 * stat = true: fixed point, the value is in points (multiplied by PET_ONE).
 * Integer fields of 1/2/4/8 bytes; the value is truncated to the field size.
 * ====================================================================================== */
typedef struct {
    const char *name;
    size_t off, size;
    bool stat;
} field_t;
#define FIELD(f, stat) {#f, offsetof(pet_state_t, f), sizeof(((pet_state_t *)0)->f), stat}
static const field_t FIELDS[] = {
    FIELD(stage, false),        FIELD(satiety, true),       FIELD(energy, true),
    FIELD(fun, true),           FIELD(hygiene, true),       FIELD(egg_active_s, false),
    FIELD(xp, false),           FIELD(level, false),        FIELD(coins, false),
    FIELD(branch, false),       FIELD(variant, false),      FIELD(poops, false),
    FIELD(gut, false),          FIELD(sick, false),         FIELD(sick_since_s, false),
    FIELD(total_scans, false),  FIELD(night_start_h, false), FIELD(night_end_h, false),
    FIELD(born_s, false),       FIELD(hatch_s, false),      FIELD(adult_s, false),
    FIELD(last_novel_s, false), FIELD(care_errors, false),  FIELD(view_seed, false),
    FIELD(signals, false),      FIELD(wish_kind, false),    FIELD(wish_arg, false),
    FIELD(wish_until_s, false), FIELD(next_wish_s, false),  FIELD(acc_owned, false),
    FIELD(acc_worn, false),     FIELD(album, false),        FIELD(tasted, false),
    FIELD(nap_until_s, false),  FIELD(streak, false),       FIELD(generation, false),
    FIELD(train[0], false),     FIELD(train[1], false),     FIELD(train[2], false),
    FIELD(inv[ITEM_TREAT], false), FIELD(inv[ITEM_BREAD], false), FIELD(best[0], false),
};
/* ====================================================================================== */

#define W 410
#define H 502
#define FRAME_MS 33                         /* firmware ui_task runs every 40 ms */
#define DAY 86400LL
#define T0 (20000LL * DAY + 10 * 3600)      /* day 20000, 10:00:00 local */
#define CORNER_R 100
#define CORNER_RGB 0x303030u

#ifndef UISIM_DIR
#define UISIM_DIR "."
#endif

/* ---------- simulated world ---------- */
pet_state_t g_pet;
volatile int g_batt_pct = 80;
volatile bool g_usb = false;

static int64_t s_now = T0;     /* pet_now_s(): local seconds */
static uint32_t s_mono = 1000; /* pet_mono_ms(): as if the UI started ~1 s after boot */
static uint32_t s_frac_ms;     /* sim-time remainder below 1 s */
static int s_mult = 1;         /* sim seconds per real second */
static uint32_t s_seed = 1;
static int s_dirty_count;
static uint32_t s_fresh_ap = 0x30000000u;

static uint16_t s_fb[W * H];
static lv_display_t *s_disp;
static int s_px, s_py;
static bool s_pressed;

void pet_lock(void) {}
void pet_unlock(void) {}
int64_t pet_now_s(void) { return s_now; }
uint32_t pet_mono_ms(void) { return s_mono; }
void pet_mark_dirty(void) { s_dirty_count++; }
int pet_time_speed(void) { return s_mult; }
void pet_set_time_speed(int mul) { s_mult = mul < 1 ? 1 : mul; }
void pet_skip_time(int64_t secs) { s_now += secs; }
bool pet_in_launcher(void) { return true; }
int64_t esp_timer_get_time(void) { return (int64_t)s_mono * 1000; }

bool bsp_display_lock(uint32_t timeout_ms) { (void)timeout_ms; return true; }
void bsp_display_unlock(void) {}

static bool log_on(void)
{
    const char *e = getenv("UISIM_LOG");
    return e && strcmp(e, "1") == 0;
}

void uisim_log(char level, const char *tag, const char *fmt, ...)
{
    if (!log_on()) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%c (%u) %s: ", level, (unsigned)s_mono, tag);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static void new_game(uint32_t seed)
{
    uint8_t key[PET_KEY_LEN];
    uint64_t x = seed;
    for (int i = 0; i < PET_KEY_LEN; i += 8) {
        uint64_t r = splitmix64(&x);
        memcpy(key + i, &r, 8);
    }
    pet_init(&g_pet, key, s_now);
}

void pet_reset_game(void)
{
    s_seed = s_seed * 2654435761u + s_mono; /* deterministic "random" new key */
    new_game(s_seed);
    s_dirty_count++;
}

/* ---------- LVGL glue ---------- */
static uint32_t tick_cb(void) { return s_mono; }

static void lv_log_cb(lv_log_level_t level, const char *buf)
{
    (void)level;
    fprintf(stderr, "lvgl: %s", buf);
}

static void flush_cb(lv_display_t *disp, const lv_area_t *a, uint8_t *px)
{
    const int w = lv_area_get_width(a);
    for (int y = a->y1; y <= a->y2; y++) {
        memcpy(&s_fb[y * W + a->x1], px + (size_t)(y - a->y1) * w * 2, (size_t)w * 2);
    }
    lv_display_flush_ready(disp);
}

/* Same rounder as core watch_display.c (panel windows on even/odd coordinates). */
static void rounder_cb(lv_event_t *e)
{
    lv_area_t *a = (lv_area_t *)lv_event_get_param(e);
    a->x1 &= ~1;
    a->y1 &= ~1;
    a->x2 |= 1;
    a->y2 |= 1;
}

static void indev_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point.x = s_px;
    data->point.y = s_py;
    data->state = s_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void sim_init(uint32_t seed)
{
    s_seed = seed;
    new_game(seed);
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_log_register_print_cb(lv_log_cb);
    s_disp = lv_display_create(W, H);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    const uint32_t sz = W * H * 2;
    void *buf = malloc(sz);
    if (!buf) {
        fprintf(stderr, "uisim: out of memory\n");
        exit(1);
    }
    lv_display_set_buffers(s_disp, buf, NULL, sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, flush_cb);
    lv_display_add_event_cb(s_disp, rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    lv_indev_t *in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, indev_cb);

    bsp_display_lock(0); /* as pet_app_start() */
    pet_ui_init();
    bsp_display_unlock();
    lv_timer_handler();
}

/* ---------- time stepping ---------- */
static void advance_ms(uint32_t dt)
{
    s_mono += dt;
    const uint64_t acc = s_frac_ms + (uint64_t)dt * (uint64_t)s_mult;
    s_now += (int64_t)(acc / 1000);
    s_frac_ms = (uint32_t)(acc % 1000);
}

static void frame(uint32_t dt)
{
    advance_ms(dt);
    pet_ui_tick(); /* without the LVGL lock, as ui_task */
    lv_timer_handler();
}

static void run_ms(int64_t ms)
{
    while (ms > 0) {
        const uint32_t dt = ms > FRAME_MS ? FRAME_MS : (uint32_t)ms;
        frame(dt);
        ms -= dt;
    }
}

/* Long runs (> 60 s): one pet_ui_tick per simulated second, LVGL every 10 s, then a few
 * normal frames so the screen is current. */
static void run_seconds(double secs)
{
    if (secs <= 60) {
        run_ms((int64_t)(secs * 1000));
        return;
    }
    const int64_t n = (int64_t)secs;
    for (int64_t i = 0; i < n; i++) {
        advance_ms(1000);
        pet_ui_tick();
        if (i % 10 == 9) {
            lv_timer_handler();
        }
    }
    run_ms((int64_t)((secs - (double)n) * 1000) + 5 * FRAME_MS);
}

/* Moves the pointer from its current position to (x, y) over ms, at least 10 reads. */
static void glide(int x, int y, int ms)
{
    int steps = ms / 20;
    if (steps < 10) {
        steps = 10;
    }
    uint32_t dt = (uint32_t)(ms / steps);
    if (dt < 16) {
        dt = 16; /* above LV_DEF_REFR_PERIOD: the indev is read every step */
    }
    const int x0 = s_px, y0 = s_py;
    for (int i = 1; i <= steps; i++) {
        s_px = x0 + (x - x0) * i / steps;
        s_py = y0 + (y - y0) * i / steps;
        frame(dt);
    }
}

static void do_boot(void)
{
    bsp_display_lock(0);
    lv_display_trigger_activity(NULL);
    pet_ui_boot_action();
    bsp_display_unlock();
    frame(FRAME_MS);
}

static void do_pwr(void)
{
    bsp_display_lock(0);
    lv_display_trigger_activity(NULL);
    const bool consumed = pet_ui_back();
    bsp_display_unlock();
    if (!consumed) {
        uisim_log('I', "uisim", "PWR not consumed (firmware: exit to launcher or screen off)");
    }
    frame(FRAME_MS);
}

static void do_scan(int n, uint32_t base, int open_n)
{
    pet_ap_t aps[64];
    if (n > 64) {
        n = 64;
    }
    for (int i = 0; i < n; i++) {
        const uint32_t id = base + (uint32_t)i;
        memset(&aps[i], 0, sizeof(aps[i]));
        aps[i].bssid[0] = 0x02;
        aps[i].bssid[1] = (uint8_t)(id >> 24);
        aps[i].bssid[2] = (uint8_t)(id >> 16);
        aps[i].bssid[3] = (uint8_t)(id >> 8);
        aps[i].bssid[4] = (uint8_t)id;
        aps[i].bssid[5] = 0x5A;
        aps[i].rssi = (int8_t)(-50 - (i * 7) % 40);
        aps[i].open = i < open_n;
    }
    pet_lock();
    const int added = pet_ingest_scan(&g_pet, aps, (size_t)n, pet_now_s());
    pet_unlock();
    uisim_log('I', "uisim", "scan: %d networks, %d bites added", n, added);
}

/* ---------- screenshots ---------- */
static bool corners_on(void)
{
    const char *e = getenv("UISIM_CORNERS");
    return !(e && strcmp(e, "0") == 0);
}

static bool outside_corner(int x, int y)
{
    const double r = CORNER_R;
    const double px = x + 0.5, py = y + 0.5;
    double cx, cy;
    if (px < r) cx = r;
    else if (px > W - r) cx = W - r;
    else return false;
    if (py < r) cy = r;
    else if (py > H - r) cy = H - r;
    else return false;
    return (px - cx) * (px - cx) + (py - cy) * (py - cy) > r * r;
}

static int write_shot(const char *path)
{
    lv_refr_now(s_disp);
    static uint32_t rgb[W * H];
    const bool corners = corners_on();
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            const uint16_t p = s_fb[y * W + x];
            const uint32_t r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, b = p & 0x1f;
            uint32_t c = ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
            if (corners && outside_corner(x, y)) {
                c = CORNER_RGB;
            }
            rgb[y * W + x] = c;
        }
    }
    return png_write_rgb(path, rgb, W, H);
}

/* ---------- script mode ---------- */
static const char *s_file;
static int s_line;

static void die(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
static void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s:%d: error: ", s_file, s_line);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(2);
}

static long long arg_int(char **tok, int ntok, int i, const char *what)
{
    if (i >= ntok) {
        die("missing argument <%s>", what);
    }
    char *end;
    errno = 0;
    const long long v = strtoll(tok[i], &end, 0);
    if (errno || *end) {
        die("bad integer for <%s>: '%s'", what, tok[i]);
    }
    return v;
}

static double arg_num(char **tok, int ntok, int i, const char *what)
{
    if (i >= ntok) {
        die("missing argument <%s>", what);
    }
    char *end;
    const double v = strtod(tok[i], &end);
    if (*end || v < 0) {
        die("bad number for <%s>: '%s'", what, tok[i]);
    }
    return v;
}

static void set_field(const char *name, long long v)
{
    for (size_t i = 0; i < sizeof(FIELDS) / sizeof(FIELDS[0]); i++) {
        const field_t *f = &FIELDS[i];
        if (strcmp(f->name, name) != 0) {
            continue;
        }
        if (f->stat) {
            v *= PET_ONE;
        }
        uint8_t *p = (uint8_t *)&g_pet + f->off;
        switch (f->size) {
        case 1: { uint8_t x = (uint8_t)v; memcpy(p, &x, 1); break; }
        case 2: { uint16_t x = (uint16_t)v; memcpy(p, &x, 2); break; }
        case 4: { uint32_t x = (uint32_t)v; memcpy(p, &x, 4); break; }
        case 8: { uint64_t x = (uint64_t)v; memcpy(p, &x, 8); break; }
        default: die("field '%s' has unsupported size %zu", name, f->size);
        }
        return;
    }
    fprintf(stderr, "%s:%d: error: unknown field '%s'; known:", s_file, s_line, name);
    for (size_t i = 0; i < sizeof(FIELDS) / sizeof(FIELDS[0]); i++) {
        fprintf(stderr, " %s", FIELDS[i].name);
    }
    fputc('\n', stderr);
    exit(2);
}

static int script_mode(const char *file, const char *outdir)
{
    FILE *f = fopen(file, "r");
    if (!f) {
        fprintf(stderr, "uisim: cannot open %s: %s\n", file, strerror(errno));
        return 2;
    }
    s_file = file;
    sim_init(1);
    char line[512];
    int dirty_mark = s_dirty_count;
    while (fgets(line, sizeof(line), f)) {
        s_line++;
        char *hash = strchr(line, '#');
        if (hash) {
            *hash = '\0';
        }
        char *tok[8];
        int n = 0;
        for (char *t = strtok(line, " \t\r\n"); t && n < 8; t = strtok(NULL, " \t\r\n")) {
            tok[n++] = t;
        }
        if (n == 0) {
            continue;
        }
        const char *c = tok[0];
        if (!strcmp(c, "wait")) {
            run_ms(arg_int(tok, n, 1, "ms"));
        } else if (!strcmp(c, "tap")) {
            s_px = (int)arg_int(tok, n, 1, "x");
            s_py = (int)arg_int(tok, n, 2, "y");
            s_pressed = true;
            run_ms(100);
            s_pressed = false;
            run_ms(4 * FRAME_MS);
        } else if (!strcmp(c, "swipe")) {
            s_px = (int)arg_int(tok, n, 1, "x1");
            s_py = (int)arg_int(tok, n, 2, "y1");
            const int x2 = (int)arg_int(tok, n, 3, "x2"), y2 = (int)arg_int(tok, n, 4, "y2");
            const int ms = n > 5 ? (int)arg_int(tok, n, 5, "ms") : 200;
            s_pressed = true;
            frame(20);
            glide(x2, y2, ms);
            s_pressed = false;
            run_ms(4 * FRAME_MS);
        } else if (!strcmp(c, "press")) {
            s_px = (int)arg_int(tok, n, 1, "x");
            s_py = (int)arg_int(tok, n, 2, "y");
            s_pressed = true;
            frame(FRAME_MS);
        } else if (!strcmp(c, "move")) {
            const int x = (int)arg_int(tok, n, 1, "x"), y = (int)arg_int(tok, n, 2, "y");
            if (n > 3) {
                glide(x, y, (int)arg_int(tok, n, 3, "ms"));
            } else {
                s_px = x;
                s_py = y;
                frame(FRAME_MS);
            }
        } else if (!strcmp(c, "release")) {
            s_pressed = false;
            run_ms(2 * FRAME_MS);
        } else if (!strcmp(c, "boot")) {
            do_boot();
        } else if (!strcmp(c, "pwr")) {
            do_pwr();
        } else if (!strcmp(c, "advance")) {
            s_now += arg_int(tok, n, 1, "seconds");
        } else if (!strcmp(c, "run")) {
            run_seconds(arg_num(tok, n, 1, "seconds"));
        } else if (!strcmp(c, "clock")) {
            int hh, mm;
            char extra;
            if (n < 2 || sscanf(tok[1], "%d:%d%c", &hh, &mm, &extra) != 2 || hh < 0 || hh > 23 || mm < 0 ||
                mm > 59) {
                die("clock expects HH:MM");
            }
            int64_t t = s_now - s_now % DAY + hh * 3600 + mm * 60;
            if (t < s_now) {
                t += DAY; /* forward only: next occurrence of HH:MM */
            }
            s_now = t;
            s_frac_ms = 0;
        } else if (!strcmp(c, "batt")) {
            g_batt_pct = (int)arg_int(tok, n, 1, "pct");
        } else if (!strcmp(c, "usb")) {
            g_usb = arg_int(tok, n, 1, "0|1") != 0;
        } else if (!strcmp(c, "speed")) {
            s_mult = (int)arg_int(tok, n, 1, "multiplier");
            if (s_mult < 1) {
                die("speed must be >= 1");
            }
        } else if (!strcmp(c, "seed")) {
            s_seed = (uint32_t)arg_int(tok, n, 1, "n");
            new_game(s_seed);
        } else if (!strcmp(c, "scan")) {
            const int cnt = (int)arg_int(tok, n, 1, "n");
            uint32_t base;
            if (n > 2) {
                base = (uint32_t)arg_int(tok, n, 2, "base");
            } else {
                base = s_fresh_ap;
                s_fresh_ap += (uint32_t)cnt;
            }
            do_scan(cnt, base, n > 3 ? (int)arg_int(tok, n, 3, "open") : 0);
        } else if (!strcmp(c, "set")) {
            if (n < 2) {
                die("set expects <field> <value>");
            }
            set_field(tok[1], arg_int(tok, n, 2, "value"));
        } else if (!strcmp(c, "shot")) {
            if (n < 2) {
                die("shot expects <name>");
            }
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s.png", outdir, tok[1]);
            if (write_shot(path) != 0) {
                die("cannot write %s", path);
            }
            printf("%s\n", path);
        } else if (!strcmp(c, "expect_dirty")) {
            if (s_dirty_count == dirty_mark) {
                die("expect_dirty: no pet_mark_dirty() since the last check");
            }
            dirty_mark = s_dirty_count;
        } else {
            die("unknown command '%s'", c);
        }
    }
    fclose(f);
    return 0;
}

/* ---------- interactive mode ---------- */
static void interactive_shot(void)
{
    char dir[1024], path[1100];
    snprintf(dir, sizeof(dir), "%s/out", UISIM_DIR);
    mkdir(dir, 0755); /* EEXIST is fine */
    for (int i = 1; i < 10000; i++) {
        snprintf(path, sizeof(path), "%s/shot-%d.png", dir, i);
        if (access(path, F_OK) != 0) {
            break;
        }
    }
    if (write_shot(path) == 0) {
        printf("%s\n", path);
        fflush(stdout);
    } else {
        fprintf(stderr, "uisim: cannot write %s\n", path);
    }
}

static int interactive_mode(int zoom, uint32_t seed)
{
    sim_init(seed);
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "uisim: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *win = SDL_CreateWindow("uisim", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W * zoom, H * zoom, 0);
    SDL_Renderer *ren = win ? SDL_CreateRenderer(win, -1, 0) : NULL;
    SDL_Texture *tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, W, H) : NULL;
    if (!tex) {
        fprintf(stderr, "uisim: SDL: %s\n", SDL_GetError());
        return 1;
    }
    printf("keys: b=BOOT p/Esc/Backspace=PWR h=+1h d=+1day 1/2/3=x1/x60/x600 s=shot q=quit\n");
    fflush(stdout);
    uint32_t next = SDL_GetTicks();
    int64_t last_title = -1;
    bool quit = false;
    while (!quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT: quit = true; break;
            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    s_px = ev.button.x / zoom;
                    s_py = ev.button.y / zoom;
                    s_pressed = true;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    s_px = ev.button.x / zoom;
                    s_py = ev.button.y / zoom;
                    s_pressed = false;
                }
                break;
            case SDL_MOUSEMOTION:
                s_px = ev.motion.x / zoom;
                s_py = ev.motion.y / zoom;
                break;
            case SDL_KEYDOWN:
                if (ev.key.repeat) {
                    break;
                }
                switch (ev.key.keysym.sym) {
                case SDLK_b: do_boot(); break;
                case SDLK_p: case SDLK_ESCAPE: case SDLK_BACKSPACE: do_pwr(); break;
                case SDLK_h: s_now += 3600; break;
                case SDLK_d: s_now += DAY; break;
                case SDLK_1: s_mult = 1; break;
                case SDLK_2: s_mult = 60; break;
                case SDLK_3: s_mult = 600; break;
                case SDLK_s: interactive_shot(); break;
                case SDLK_q: quit = true; break;
                default: break;
                }
                break;
            default: break;
            }
        }
        frame(FRAME_MS);
        SDL_UpdateTexture(tex, NULL, s_fb, W * 2);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
        if (s_now / 60 != last_title) {
            last_title = s_now / 60;
            char t[96];
            snprintf(t, sizeof(t), "uisim  day %lld  %02d:%02d  x%d", (long long)(s_now / DAY),
                     (int)(s_now % DAY / 3600), (int)(s_now % 3600 / 60), s_mult);
            SDL_SetWindowTitle(win, t);
        }
        next += FRAME_MS;
        const int32_t wait = (int32_t)(next - SDL_GetTicks());
        if (wait > 0) {
            SDL_Delay((uint32_t)wait);
        } else if (wait < -200) {
            next = SDL_GetTicks(); /* fell behind: do not try to catch up */
        }
    }
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

static int usage(void)
{
    fprintf(stderr, "usage: uisim script <file> <outdir>\n"
                    "       uisim run [-z zoom] [seed]\n");
    return 2;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "script")) {
        return argc == 4 ? script_mode(argv[2], argv[3]) : usage();
    }
    if (argc >= 2 && !strcmp(argv[1], "run")) {
        int zoom = 1;
        uint32_t seed = 1;
        for (int i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "-z") && i + 1 < argc) {
                zoom = atoi(argv[++i]);
                if (zoom < 1 || zoom > 4) {
                    return usage();
                }
            } else {
                seed = (uint32_t)strtoul(argv[i], NULL, 0);
            }
        }
        return interactive_mode(zoom, seed);
    }
    return usage();
}
