#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#define DS_IMPLEMENTATION
#include "ds.h"

#define MAP_FILE "world.txt"

#define PLAYER_CH "@"
#define FLOOR_CH "."
#define WALL_CH "#"
#define TREE_CH "&"

#define PLAYER "\033[33m" PLAYER_CH "\033[m"
#define FLOOR "\033[30m" FLOOR_CH "\033[m"
#define WALL "\033[37m" WALL_CH "\033[m"
#define TREE "\033[32m" TREE_CH "\033[m"
#define CLEAR_SCREEN_ANSI "\e[1;1H\e[2J"

#define MOVE_UP 'w'
#define MOVE_LEFT 'a'
#define MOVE_DOWN 's'
#define MOVE_RIGHT 'd'
#define QUIT_KEY 'q'

#define WIDTH 16
#define HEIGHT 8

typedef enum tile_kind {
    floor,
    wall,
    tree
} tile_kind;

typedef struct world {
    unsigned int player_row;
    unsigned int player_col;
    unsigned int width;
    unsigned int height;
    ds_dynamic_array /* tile_kind */ tiles;
} world_t;

void world_print(world_t *world) {
    for (unsigned int index = 0; index < world->tiles.count; index++) {
        tile_kind kind;
        ds_dynamic_array_get(&world->tiles, index, &kind);

        unsigned int row = index / world->width;
        unsigned int col = index % world->width;

        if (row >= 1 && col == 0) {
            printf("\n");
        }

        if (row == world->player_row && col == world->player_col) {
            printf(PLAYER);
        } else {
            switch (kind) {
            case floor:
                printf(FLOOR);
                break;
            case wall:
                printf(WALL);
                break;
            case tree:
                printf(TREE);
              break;
            }
        }
    }

    printf("\n");
}

void parse_world(char *buffer, unsigned int length, world_t *world) {
    ds_dynamic_array_init(&world->tiles, sizeof(tile_kind));

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

            if (strncmp(line_str + col, PLAYER_CH, 1) == 0) {
                world->player_row = row;
                world->player_col = col;
                kind = floor;
            } else if (strncmp(line_str + col, FLOOR_CH, 1) == 0) {
                kind = floor;
            } else if (strncmp(line_str + col, WALL_CH, 1) == 0) {
                kind = wall;
            } else if (strncmp(line_str + col, TREE_CH, 1) == 0) {
                kind = tree;
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

char input;

void *input_thread() {
    for (;;) {
        input = getchar();

        if (input == QUIT_KEY) {
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

    pthread_t input_thread_id;
    pthread_create(&input_thread_id, NULL, input_thread, NULL);

    while (1) {
        // update
        if (input == MOVE_UP) {
            world.player_row--;
        } else if (input == MOVE_LEFT) {
            world.player_col--;
        } else if (input == MOVE_DOWN) {
            world.player_row++;
        } else if (input == MOVE_RIGHT) {
            world.player_col++;
        } else if (input == QUIT_KEY) {
            break;
        }
        input = 0;

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
