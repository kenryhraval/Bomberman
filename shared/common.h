#pragma once
#include <stdint.h>
#include <stdbool.h>

#if defined(__GNUC__) || defined(__clang__)
#define PACKED __attribute__((packed))
#else
#define PACKED
#endif

// from LSP_game_specs_2026

#define MAX_PLAYERS 8
#define TICKS_PER_SECOND 20
#define MAX_NAME_LEN 29
#define MAX_CLIENT_ID_LEN 19
#define SERVER 255
#define BROADCAST 254
#define MAX_MAP_ROWS 255
#define MAX_MAP_COLS 255
#define TICK_RATE 20

// for map configuration sent to initiator
#define MAX_MAP_CHOICES 16
#define MAX_MAP_NAME_LEN 64

// timeout condition for draw
#define GAME_DRAW_TICK_TIMEOUT (5 * 60 * TICK_RATE)


typedef struct PACKED {
    uint8_t id;
    char name[MAX_MAP_NAME_LEN];
    uint8_t rows;
    uint8_t cols;
    uint8_t supported_players;
    uint16_t player_speed;
    uint16_t explosion_duration_ticks;
    uint8_t explosion_radius;
    uint16_t bomb_timer_ticks;
} client_map_choice_t;

// cell types for the map
typedef enum {
    HARD_BLOCK = 'H',
    SOFT_BLOCK = 'S',
    EMPTY = '.',
    BOMB = 'B',
    SPEED_BONUS = 'A',
    BOMB_RADIUS_BONUS = 'R',
    BOMB_TIMER_BONUS = 'T',
    BOMB_COUNT_BONUS = 'N',
    PLAYER_1 = '1',
    PLAYER_LAST = '1' + MAX_PLAYERS - 1,
} cell_type_t;


// map structure
typedef struct {
    uint8_t rows;
    uint8_t cols;
    uint8_t cells[MAX_MAP_ROWS * MAX_MAP_COLS];
} map_t;


// game statuses
typedef enum
{
    GAME_LOBBY = 0,
    GAME_RUNNING = 1,
    GAME_END = 2
} game_status_t;


// move directions
typedef enum
{
    DIR_UP = 'U',
    DIR_DOWN = 'D',
    DIR_LEFT = 'L',
    DIR_RIGHT = 'R'
} direction_t;


// bonus types
typedef enum
{
    BONUS_NONE = '\0',
    BONUS_SPEED = 'A',
    BONUS_RADIUS = 'R',
    BONUS_TIMER = 'T',
    BONUS_BOMB_COUNT = 'N'
} bonus_type_t;


// message types
typedef enum
{
    MSG_HELLO = 0,
    MSG_WELCOME = 1,
    MSG_DISCONNECT = 2,
    MSG_PING = 3,
    MSG_PONG = 4,
    MSG_LEAVE = 5,
    MSG_ERROR = 6,
    MSG_MAP = 7,
    MSG_SET_READY = 10,
    MSG_SET_STATUS = 20,
    MSG_WINNER = 23,
    MSG_MOVE_ATTEMPT = 30,
    MSG_BOMB_ATTEMPT = 31,
    MSG_MOVED = 40,
    MSG_BOMB = 41,
    MSG_EXPLOSION_START = 42,
    MSG_EXPLOSION_END = 43,
    MSG_DEATH = 44,
    MSG_BONUS_AVAILABLE = 45,
    MSG_BONUS_RETRIEVED = 46,
    MSG_BLOCK_DESTROYED = 47,
    MSG_STATISTICS = 100,
    MSG_MAP_CHOICES = 101,
    MSG_MAP_SELECTED = 102,
    MSG_TIMER_SYNC = 103,
    MSG_PLAYER_UPDATE = 104,
} msg_type_t;


// player structure
typedef struct PACKED
{
    uint8_t id;
    char name[MAX_NAME_LEN + 1];
    uint16_t row;
    uint16_t col;
    bool alive;
    bool ready;
    uint8_t bomb_count;
    uint8_t bomb_radius;
    uint16_t bomb_timer_ticks;
    uint16_t bomb_explosion_duration_ticks;
    uint16_t speed;

    // Used for rate-limiting moves based on player's speed
    // If client does not need this just leave it as 0
    uint64_t last_move_tick;
} player_t;


// bomb structure
typedef struct PACKED
{
    uint8_t owner_id;
    uint16_t row;
    uint16_t col;
    uint8_t radius;
    uint16_t timer_ticks;
} bomb_t;


// statistics for end of the game structure
typedef struct PACKED
{
    uint8_t kills;
    uint16_t blocks_destroyed;
    uint16_t bonuses_collected;
} statistics_t;


// helper function to convert row and col to cell index in the map's cells array
static inline uint16_t make_cell_index(uint16_t row, uint16_t col, uint16_t cols)
{
    return row * cols + col;
}
