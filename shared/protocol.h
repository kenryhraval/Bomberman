#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "common.h"

// from LSP_game_specs_2026

typedef struct {
    uint8_t msg_type; // ziņas tips, kas nosaka datu struktūru
    uint8_t sender_id;
    uint8_t target_id; // adresāta ID. 255=server. 254=broadcast.
} msg_generic_t;

// payload structs for simple messages

typedef struct {
    char client_id[MAX_CLIENT_ID_LEN + 1];
    char player_name[MAX_NAME_LEN + 1];
} msg_hello_t;

typedef struct {
    uint8_t ready;
} msg_set_ready_t;

typedef struct {
    uint8_t game_status;
} msg_set_status_t;

typedef struct {
    uint8_t winner_id;
} msg_winner_t;

typedef struct {
    uint8_t direction;   /* 'U', 'D', 'L', 'R' */
} msg_move_attempt_t;

typedef struct {
    uint8_t player_id;
    uint16_t cell;
} msg_moved_t;

typedef struct {
    uint16_t cell;
} msg_bomb_attempt_t;

typedef struct {
    uint8_t player_id;
    uint16_t cell;
} msg_bomb_t;

typedef struct {
    uint8_t radius;
    uint16_t cell;
} msg_explosion_start_t;

typedef struct {
    uint16_t cell;
} msg_explosion_end_t;

typedef struct {
    uint8_t player_id;
} msg_death_t;

typedef struct {
    uint8_t bonus_type;
    uint16_t cell;
} msg_bonus_available_t;

typedef struct {
    uint8_t player_id;
    uint16_t cell;
} msg_bonus_retrieved_t;

typedef struct {
    uint16_t cell;
} msg_block_destroyed_t;

// WELCOME

typedef struct {
    uint8_t player_id;
    uint8_t ready;
    char name[MAX_NAME_LEN + 1];
} welcome_client_entry_t;

typedef struct {
    char server_id[MAX_CLIENT_ID_LEN + 1];
    uint8_t game_status;
    uint8_t other_count;
    welcome_client_entry_t others[8];
} msg_welcome_t;


// ERROR
// DISCONNECT
// LEAVE
// MAP


/*
 * Reads exactly `count` bytes from file descriptor `fd` into buffer `buf`.
 *
 * Repeatedly calls read() until:
 * - exactly `count` bytes have been read, in which case returns 0
 * - the peer closes the connection, in which case returns -1
 * - a non-recoverable read error occurs, in which case returns -1
 *
 * If read() is interrupted by a signal (errno == EINTR), the call is retried.
 *
 * This is useful because a single read() call is not guaranteed to return
 * all requested bytes at once.
 */
int read_exact(int fd, void *buf, size_t count);


/*
 * Writes exactly `count` bytes from buffer `buf` to file descriptor `fd`.
 *
 * Repeatedly calls write() until:
 * - exactly `count` bytes have been written, in which case returns 0
 * - a non-recoverable write error occurs, in which case returns -1
 *
 * If write() is interrupted by a signal (errno == EINTR), the call is retried.
 *
 * This is useful because a single write() call is not guaranteed to send
 * all requested bytes at once.
 */
int write_exact(int fd, const void *buf, size_t count);


/*
Šo ziņu klients nosūta serverim tūlīt pēc pieslēgšanās. Klienta identifikators ir simbolu
virkne, kas identificē klienta programmas nosaukumu un versiju, savukārt spēlētāja vārds
unikāli identificē pašu spēlētāju.
Kad serveris šo ziņu saņem, tas izvērtē, vai ir gatavs pieņemt klientu. Ja nav, tad sūta
DISCONNECT ziņu, ja ir, tad šo ziņu pārsūta pārējiem klientiem un attiecīgajam klientam
nosūta WELCOME ziņu.
*/
int send_hello(int fd, uint8_t sender_id, uint8_t target_id, const msg_hello_t *msg);
int recv_hello(int fd, msg_generic_t *header, msg_hello_t *msg);


/*
Šī ziņa vienmēr tiek sūtīta kā atbilde uz HELLO ziņu. Ja klients, kas mēģina pieslēgties, 30
sekunžu laikā nesaņem ne šo ziņu, ne DISCONNECT ziņu, tam vajadzētu aizvērt TCP
savienojumu. Pirms šī ziņa ir saņemta, citas ziņas klients sūtīt nedrīkst.
● Servera identifikators, tāpat kā klienta identifikators, identificē servera programmas
nosaukumu un versiju.
● Spēles statuss ir viens no skaitļiem augstāk redzamajā statusu tabulā. Tas norāda
to, vai spēle ir lobijā vai arī ir sākusies.
● Visbeidzot, tiek nosūtīts masīvs ar pārējo klientu ID, spēlētāju gatavību un
spēlētāju vārdiem. Ja spēle nav lobija stāvoklī, tad klients ir gatavs tad un tikai tad,
ja tas piedalās spēlē.
Ja statuss ir 1, tad serverim vajadzētu arī nosūtīt SYNC_BOARD ziņu par katru spēlētāju,
savukārt, ja statuss ir 3, tad serverim vajadzētu nosūtīt WINNER ziņu.
Kā ziņas avots tiek norādīts piešķirtais spēlētāja ID.
*/
int send_welcome(int fd, uint8_t sender_id, uint8_t target_id, const msg_welcome_t *msg);
int recv_welcome(int fd, msg_generic_t *header, msg_welcome_t *msg);


#endif

