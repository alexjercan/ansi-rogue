#include <ctype.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#define DS_IMPLEMENTATION
#include "ds.h"

#define MAP_FILE "world.txt"

#define PLAYER_CH '@'
#define FLOOR_CH '.'
#define WALL_CH '#'
#define TREE_CH '&'

#define RESET_COL "\033[m"
#define PLAYER_COL "\033[33m"
#define ENEMY_COL "\033[31m"
#define FLOOR_COL "\033[30m"
#define WALL_COL "\033[37m"
#define TREE_COL "\033[32m"
#define CLEAR_SCREEN_ANSI "\e[1;1H\e[2J"

#define MOVE_UP 'w'
#define MOVE_LEFT 'a'
#define MOVE_DOWN 's'
#define MOVE_RIGHT 'd'
#define QUIT_KEY 'q'

#define WIDTH 16
#define HEIGHT 8

typedef char tile_kind;

void tile_kind_print(tile_kind t) {
    if (t == FLOOR_CH) {
        printf("%s%c%s", FLOOR_COL, FLOOR_CH, RESET_COL);
    } else if (t == WALL_CH) {
        printf("%s%c%s", WALL_COL, WALL_CH, RESET_COL);
    } else if (t == TREE_CH) {
        printf("%s%c%s", TREE_COL, TREE_CH, RESET_COL);
    } else {
        DS_PANIC("unreachable");
    }
}

typedef struct uvec2 {
    unsigned int x;
    unsigned int y;
} uvec2;

typedef struct entity {
    uvec2 position;
    char symbol;
} entity;

void entity_print(entity e) {
    if (e.symbol == PLAYER_CH) {
        printf("%s%c%s", PLAYER_COL, PLAYER_CH, RESET_COL);
    } else {
        printf("%s%c%s", ENEMY_COL, e.symbol, RESET_COL);
    }
}

typedef struct world {
    entity player;
    unsigned int width;
    unsigned int height;
    ds_dynamic_array /* tile_kind */ tiles;
    ds_dynamic_array /* entity */ enemies;
} world_t;

void world_move_player(world_t *world, char input) {
    unsigned int player_row = world->player.position.y;
    unsigned int player_col = world->player.position.x;

    if (input == MOVE_UP) {
        player_row--;
    } else if (input == MOVE_LEFT) {
        player_col--;
    } else if (input == MOVE_DOWN) {
        player_row++;
    } else if (input == MOVE_RIGHT) {
        player_col++;
    }

    tile_kind kind;
    unsigned int index = player_row * world->width + player_col;
    ds_dynamic_array_get(&world->tiles, index, &kind);

    if (kind != WALL_CH) {
        world->player.position.y = player_row;
        world->player.position.x = player_col;
    }
}

void world_print(world_t *world) {
    for (unsigned int index = 0; index < world->tiles.count; index++) {
        tile_kind kind;
        ds_dynamic_array_get(&world->tiles, index, &kind);

        unsigned int row = index / world->width;
        unsigned int col = index % world->width;

        if (row >= 1 && col == 0) {
            printf("\n");
        }

        if (row == world->player.position.y && col == world->player.position.x) {
            entity_print(world->player);
        } else {
            int show_tile = 1;

            for (unsigned int j = 0; j < world->enemies.count; j++) {
                entity e;
                ds_dynamic_array_get(&world->enemies, j, &e);
                if (row == e.position.y && col == e.position.x) {
                    entity_print(e);
                    show_tile = 0;
                }
            }

            if (show_tile) {
                tile_kind_print(kind);
            }
        }
    }

    printf("\n");
}

void parse_world(char *buffer, unsigned int length, world_t *world) {
    ds_dynamic_array_init(&world->tiles, sizeof(tile_kind));
    ds_dynamic_array_init(&world->enemies, sizeof(entity));

    ds_string_slice slice;
    ds_string_slice_init(&slice, buffer, length);

    ds_string_slice line;
    unsigned int row = 0;
    while (ds_string_slice_tokenize(&slice, '\n', &line) == 0) {
        char *line_str;
        ds_string_slice_to_owned(&line, &line_str);

        unsigned int line_length = strlen(line_str);
        for (unsigned int col = 0; col < line_length; col++) {
            tile_kind kind;

            if (line_str[col] == PLAYER_CH) {
                world->player.position.y = row;
                world->player.position.x = col;
                world->player.symbol = PLAYER_CH;
                kind = FLOOR_CH;
            } else if (line_str[col] == FLOOR_CH) {
                kind = FLOOR_CH;
            } else if (line_str[col] == WALL_CH) {
                kind = WALL_CH;
            } else if (line_str[col] == TREE_CH) {
                kind = TREE_CH;
            } else if (isalpha(line_str[col])) {
                entity e = { .position = { .x = col, .y = row }, .symbol = line_str[col] };
                ds_dynamic_array_append(&world->enemies, &e);
                kind = FLOOR_CH;
            } else {
                continue;
            }

            ds_dynamic_array_append(&world->tiles, &kind);
        }

        world->width = line_length;
        row++;
    }

    world->height = row;
}

typedef struct input {
    char last_key;
} input_t;

void *input_thread(void *arg) {
    input_t *input = (input_t *)arg;

    for (;;) {
        input->last_key = getchar();

        if (input->last_key == QUIT_KEY) {
            break;
        }
    }

    return NULL;
}

int main(void) {
    world_t world;
    char *buffer = NULL;

    unsigned int length = ds_io_read_file(MAP_FILE, &buffer);
    parse_world(buffer, length, &world);

    input_t input;
    pthread_t input_thread_id;
    pthread_create(&input_thread_id, NULL, input_thread, &input);

    while (1) {
        // update
        world_move_player(&world, input.last_key);
        if (input.last_key == QUIT_KEY) {
            break;
        }
        input.last_key = 0;

        // render
        system("stty cooked");
        printf("%s", CLEAR_SCREEN_ANSI);
        world_print(&world);
        system("stty raw");

        usleep(160000);
    }

    pthread_join(input_thread_id, NULL);

    return 0;
}
