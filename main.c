#include <limits.h>
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
#define DOOR_CH '|'
#define KEY_CH '-'
#define GOLD_CH '$'

#define RESET_COL "\033[m"
#define BLACK_COL "\033[30m"
#define RED_COL "\033[31m"
#define GREEN_COL "\033[32m"
#define YELLOW_COL "\033[33m"
#define BLUE_COL "\033[34m"
#define MAGENTA_COL "\033[35m"
#define CYAN_COL "\033[36m"
#define WHITE_COL "\033[37m"
#define CLEAR_SCREEN_ANSI "\e[1;1H\e[2J"

#define PLAYER_COL YELLOW_COL
#define ENEMY_COL RED_COL
#define FLOOR_COL BLACK_COL
#define WALL_COL WHITE_COL
#define TREE_COL GREEN_COL
#define DOOR_COL MAGENTA_COL
#define KEY_COL MAGENTA_COL
#define GOLD_COL YELLOW_COL

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
        printf("%s%c%s", FLOOR_COL, t, RESET_COL);
    } else if (t == WALL_CH) {
        printf("%s%c%s", WALL_COL, t, RESET_COL);
    } else if (t == TREE_CH) {
        printf("%s%c%s", TREE_COL, t, RESET_COL);
    } else if (t == DOOR_CH) {
        printf("%s%c%s", DOOR_COL, t, RESET_COL);
    } else if (t == KEY_CH) {
        printf("%s%c%s", KEY_COL, t, RESET_COL);
    } else if (t == GOLD_CH) {
        printf("%s%c%s", GOLD_COL, t, RESET_COL);
    } else {
        DS_PANIC("unreachable");
    }
}

int tile_kind_is_impassible(tile_kind t) {
    return (t == WALL_CH || t == DOOR_CH);
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

typedef struct inventory {
    unsigned int keys;
    unsigned int gold;
} inventory;

void inventory_print(inventory v) {
    printf(GREEN_COL "Inventory:\n" RESET_COL);
    printf(KEY_COL "- keys: %d\n" RESET_COL, v.keys);
    printf(GOLD_COL "- gold: %d\n" RESET_COL, v.gold);
}

typedef struct world {
    entity player;
    inventory inventory;
    unsigned int width;
    unsigned int height;
    ds_dynamic_array /* tile_kind */ tiles;
    ds_dynamic_array /* entity */ enemies;
} world_t;

void world_free(world_t *world) {
    ds_dynamic_array_free(&world->tiles);
    ds_dynamic_array_free(&world->enemies);
}

void handle_input(world_t *world, char input) {
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

    tile_kind *tile = NULL;
    unsigned int index = player_row * world->width + player_col;
    ds_dynamic_array_get_ref(&world->tiles, index, (void **)&tile);

    if (*tile == DOOR_CH && world->inventory.keys > 0) {
        world->inventory.keys -= 1;
        *tile = FLOOR_CH;
    }

    if (tile_kind_is_impassible(*tile) == 0) {
        world->player.position.y = player_row;
        world->player.position.x = player_col;
    }

    if (*tile == KEY_CH) {
        world->inventory.keys += 1;
        *tile = FLOOR_CH;
    } else if (*tile == GOLD_CH) {
        world->inventory.gold += 1;
        *tile = FLOOR_CH;
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

    inventory_print(world->inventory);

    printf("\n");
}

void world_parse(char *buffer, unsigned int length, world_t *world) {
    memset(&world->inventory, 0, sizeof(inventory));
    ds_dynamic_array_init(&world->tiles, sizeof(tile_kind));
    ds_dynamic_array_init(&world->enemies, sizeof(entity));

    ds_string_slice buffer_slice;
    ds_string_slice_init(&buffer_slice, buffer, length);

    ds_string_slice line_slice;
    unsigned int row = 0;
    while (ds_string_slice_tokenize(&buffer_slice, '\n', &line_slice) == 0) {
        char *line;
        if (ds_string_slice_to_owned(&line_slice, &line) != 0) {
            DS_PANIC("buy more ram");
        }

        unsigned int line_length = strlen(line);
        for (unsigned int col = 0; col < line_length; col++) {
            tile_kind kind;

            if (line[col] == PLAYER_CH) {
                world->player.position.y = row;
                world->player.position.x = col;
                world->player.symbol = PLAYER_CH;
                kind = FLOOR_CH;
            } else if (isalpha(line[col])) {
                entity e = { .position = { .x = col, .y = row }, .symbol = line[col] };
                if (ds_dynamic_array_append(&world->enemies, &e) != 0) {
                    DS_PANIC("buy more ram");
                }
                kind = FLOOR_CH;
            } else {
                kind = line[col];
            }

            if (ds_dynamic_array_append(&world->tiles, &kind) != 0) {
                DS_PANIC("buy more ram");
            }
        }

        world->width = line_length;
        row++;

        free(line);
    }

    world->height = row;
}

typedef struct astar_node {
    uvec2 p;
    int f;
} astar_node;

int astar_node_compare_min(const void *a, const void *b) {
    return ((astar_node *)b)->f - ((astar_node *)a)->f;
}

int manhattan_distance(uvec2 p1, uvec2 p2) {
    return abs((int)p1.x - (int)p2.x) + abs((int)p1.y - (int)p2.y);
}

int uvec2_hash(struct world *w, uvec2 p) {
    return p.y * w->width + p.x;
}

int uvec2_equals(uvec2 p1, uvec2 p2) {
    return p1.x == p2.x && p1.y == p2.y;
}

int reconstruct_path(struct world *w, int *came_from, uvec2 current,
                     ds_dynamic_array *p) {
    ds_dynamic_array_append(p, &current);
    int current_index = uvec2_hash(w, current);

    while (came_from[current_index] != -1) {
        current_index = came_from[current_index];
        uvec2 current = {current_index % w->width, current_index / w->width};
        ds_dynamic_array_append(p, &current);
    }

    return 0;
}

const uvec2 directions[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
const int num_directions = sizeof(directions) / sizeof(directions[0]);

int a_star(world_t *w, uvec2 start, uvec2 end, ds_dynamic_array /* uvec2 */ *p) {
    int result = 0;
    unsigned int num_nodes = w->width * w->height;

    // The set of discovered nodes that may need to be (re-)expanded.
    // Initially, only the start node is known.
    // This is usually implemented as a min-heap or priority queue rather than a
    // hash-set.
    ds_priority_queue open_set;
    ds_priority_queue_init(&open_set, astar_node_compare_min, sizeof(astar_node));

    struct astar_node start_node = { start, manhattan_distance(start, end) };
    ds_priority_queue_insert(&open_set, &start_node);

    // For node n, cameFrom[n] is the node immediately preceding it on the
    // cheapest path from the start to n currently known.
    int *came_from = (int *)malloc(num_nodes * sizeof(int));
    for (int i = 0; i < num_nodes; i++) {
        came_from[i] = -1;
    }

    // For node n, gScore[n] is the cost of the cheapest path from start to n
    // currently known.
    int *g_score = (int *)malloc(num_nodes * sizeof(int));
    for (int i = 0; i < num_nodes; i++) {
        g_score[i] = INT_MAX;
    }
    g_score[uvec2_hash(w, start)] = 0;

    // For node n, fScore[n] := gScore[n] + h(n). fScore[n] represents our
    // current best guess as to how cheap a path could be from start to finish
    // if it goes through n.
    int *f_score = (int *)malloc(num_nodes * sizeof(int));
    for (int i = 0; i < num_nodes; i++) {
        f_score[i] = INT_MAX;
    }
    f_score[uvec2_hash(w, start)] = manhattan_distance(start, end);

    astar_node current_node = {0};
    while (ds_priority_queue_empty(&open_set) == 0) {
        // This operation can occur in O(Log(N)) time if openSet is a min-heap
        // or a priority queue
        ds_priority_queue_pull(&open_set, (void *)&current_node);
        int current_index = uvec2_hash(w, current_node.p);

        uvec2 current = current_node.p;

        if (uvec2_equals(current_node.p, end)) {
            reconstruct_path(w, came_from, current_node.p, p);
            return_defer(1);
        }

        for (int i = 0; i < num_directions; i++) {
            uvec2 neighbor = {current.x + directions[i].x, current.y + directions[i].y};
            int neighbor_index = uvec2_hash(w, neighbor);

            tile_kind tile;
            ds_dynamic_array_get(&w->tiles, neighbor_index, &tile);

            if (neighbor.x < 0 || neighbor.x >= w->width || neighbor.y < 0 ||
                neighbor.y >= w->height || tile_kind_is_impassible(tile)) {
                continue;
            }

            // d(current,neighbor) is the weight of the edge from current to
            // neighbor tentative_gScore is the distance from start to the
            // neighbor through current
            int tentative_g_score = g_score[current_index] + 1;
            if (tentative_g_score < g_score[neighbor_index]) {
                // This path to neighbor is better than any previous one.
                came_from[neighbor_index] = current_index;
                g_score[neighbor_index] = tentative_g_score;
                f_score[neighbor_index] =
                    tentative_g_score + manhattan_distance(neighbor, end);

                int found = 0;
                for (unsigned int j = 0; j < open_set.items.count; j++) {
                    astar_node node;
                    ds_dynamic_array_get(&open_set.items, j, &node);

                    if (uvec2_equals(node.p, neighbor)) {
                        found = 1;
                        break;
                    }
                }

                if (found == 0) {
                    astar_node neighbor_node = { neighbor, f_score[neighbor_index] };
                    ds_priority_queue_insert(&open_set, &neighbor_node);
                }
            }
        }
    }

defer:
    ds_priority_queue_free(&open_set);
    free(came_from);
    free(g_score);
    free(f_score);

    return result;
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

    int length = ds_io_read_file(MAP_FILE, &buffer);
    if (length < 0) {
        DS_PANIC("failed to read the map!");
    }
    world_parse(buffer, length, &world);

    input_t input = { .last_key = 0 };
    pthread_t input_thread_id;
    pthread_create(&input_thread_id, NULL, input_thread, &input);

    while (1) {
        // update
        handle_input(&world, input.last_key);
        if (input.last_key == QUIT_KEY) {
            break;
        }
        if (input.last_key != 0) {
            for (unsigned int i = 0; i < world.enemies.count; i++) {
                ds_dynamic_array p;
                ds_dynamic_array_init(&p, sizeof(uvec2));

                entity *enemy;
                ds_dynamic_array_get_ref(&world.enemies, i, (void **)&enemy);

                a_star(&world, enemy->position, world.player.position, &p);

                if (p.count >= 2) {
                    unsigned int next = p.count - 2;
                    ds_dynamic_array_get(&p, next, &enemy->position);
                }

                ds_dynamic_array_free(&p);
            }
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

    world_free(&world);
    free(buffer);

    return 0;
}
