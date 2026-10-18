# Tests de host de pet_core (C99 puro, sin ESP-IDF). `make test`
CC ?= cc
CORE = ../components/pet_core
CFLAGS = -std=c99 -O1 -g -Wall -Wextra -I$(CORE)/include -I../components/pet_render/include -I../components/pet_game/include
SRC = test_core.c $(wildcard $(CORE)/*.c) $(wildcard ../components/pet_render/*.c)

build/test_core: $(SRC) world.h $(CORE)/include/pet_core.h ../components/pet_render/include/pet_render.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $@ -lm

GSRC = test_game.c ../components/pet_game/pet_game.c $(wildcard $(CORE)/*.c) $(wildcard ../components/pet_render/*.c)

build/test_game: $(GSRC) ../components/pet_game/include/pet_game.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(GSRC) -o $@ -lm

test: build/test_core build/test_game
	build/test_core
	build/test_game

# Con sanitizadores: UB y desbordes en la serializacion y el fuzz.
asan:
	@mkdir -p build
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined $(SRC) -o build/test_core_asan -lm
	build/test_core_asan
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined $(GSRC) -o build/test_game_asan -lm
	build/test_game_asan
