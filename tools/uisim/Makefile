# Host simulator of the real LVGL UI (pet_ui*.c). See README.md.
#   make            -> build/uisim (LVGL compiled once into build/liblvgl.a)
#   make shots      -> runs scripts/smoke.txt into out/
#   make run        -> interactive window
#   make clean      -> removes build/ (LVGL included); make clean-app keeps liblvgl.a
CC ?= cc
ROOT := ../..
LVGL_DIR := $(ROOT)/managed_components/lvgl__lvgl
BUILD := build
JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

ifeq ($(wildcard $(LVGL_DIR)/lvgl.h),)
$(error LVGL not found in $(LVGL_DIR): run idf.py reconfigure (or build) once in the repo root)
endif

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)

# ---- LVGL (warnings off: not our code) ----
LVGL_SRC := $(shell find $(LVGL_DIR)/src -name '*.c')
LVGL_OBJ := $(patsubst $(LVGL_DIR)/%.c,$(BUILD)/lvgl/%.o,$(LVGL_SRC))
LVGL_CFLAGS := -std=gnu99 -O2 -w -DLV_CONF_INCLUDE_SIMPLE -I. -I$(LVGL_DIR)

# ---- app sources (wildcards: new files are picked up) ----
CORE_SRC := $(wildcard $(ROOT)/components/pet_core/*.c) $(wildcard $(ROOT)/components/pet_render/*.c) $(wildcard $(ROOT)/components/pet_game/*.c)
UI_SRC := $(wildcard $(ROOT)/components/pet_app/pet_ui*.c)
PNG_SRC := $(ROOT)/tools/sim/png.c
CORE_OBJ := $(patsubst $(ROOT)/%.c,$(BUILD)/app/%.o,$(CORE_SRC) $(PNG_SRC))
UI_OBJ := $(patsubst $(ROOT)/%.c,$(BUILD)/app/%.o,$(UI_SRC))
SIM_OBJ := $(BUILD)/uisim.o

INC := -Ishim -I. -I$(LVGL_DIR) -I$(ROOT)/components/pet_core/include -I$(ROOT)/components/pet_render/include \
       -I$(ROOT)/components/pet_game/include \
       -I$(ROOT)/components/pet_app -I$(ROOT)/components/pet_app/include -I$(ROOT)/tools/sim
COMMON := -O1 -g -Wall -MMD -MP -DLV_CONF_INCLUDE_SIMPLE $(INC)
# pet_core/pet_render are C99 by contract; the UI builds like the firmware (gnu17).
CORE_CFLAGS := -std=c99 -Wextra $(COMMON)
UI_CFLAGS := -std=gnu17 $(COMMON) -Wno-unused-parameter

.PHONY: all lvgl shots run clean clean-app
all:
	@$(MAKE) --no-print-directory -j$(JOBS) $(BUILD)/uisim

lvgl:
	@$(MAKE) --no-print-directory -j$(JOBS) $(BUILD)/liblvgl.a

$(BUILD)/uisim: $(SIM_OBJ) $(UI_OBJ) $(CORE_OBJ) $(BUILD)/liblvgl.a
	$(CC) -g $^ $(SDL_LIBS) -lm -o $@

$(BUILD)/liblvgl.a: $(LVGL_OBJ)
	@rm -f $@
	@ar rcs $@ $^
	@echo "LVGL: $(words $(LVGL_OBJ)) objects -> $@"

$(BUILD)/lvgl/%.o: $(LVGL_DIR)/%.c lv_conf.h
	@mkdir -p $(dir $@)
	@$(CC) $(LVGL_CFLAGS) -c $< -o $@

$(CORE_OBJ): $(BUILD)/app/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CORE_CFLAGS) -c $< -o $@

$(UI_OBJ): $(BUILD)/app/%.o: $(ROOT)/%.c lv_conf.h
	@mkdir -p $(dir $@)
	$(CC) $(UI_CFLAGS) -c $< -o $@

$(SIM_OBJ): uisim.c lv_conf.h
	@mkdir -p $(dir $@)
	$(CC) $(UI_CFLAGS) $(SDL_CFLAGS) -DUISIM_DIR='"$(CURDIR)"' -c $< -o $@

shots: all
	@mkdir -p out
	$(BUILD)/uisim script scripts/smoke.txt out

run: all
	$(BUILD)/uisim run

clean:
	rm -rf $(BUILD)

clean-app:
	rm -rf $(BUILD)/app $(BUILD)/uisim.o $(BUILD)/uisim

-include $(UI_OBJ:.o=.d) $(CORE_OBJ:.o=.d) $(SIM_OBJ:.o=.d)
