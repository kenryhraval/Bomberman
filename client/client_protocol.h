#pragma once
#include "shared/protocol.h"

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

/*
Klients šo ziņu serverim nosūta, lai norādītu savu gatavību sākt spēli. Kad visi spēlētāji ir
iestatījuši sevi kā gatavus, tad tiek sākta spēle.
Serveris ir tiesīgs nosūtīt šo ziņu arī tad, ja attiecīgais klients nav tādu nosūtījis, tādējādi
“piespiedu kārtā” padarot to par spēlētāju. Tā var īstenot, piemēram, iespēju pieslēgties
pēc savienojuma pazušanas spēles laikā
*/
int send_set_ready(int fd, uint8_t sender_id, uint8_t target_id);

/*
Šo ziņu klients sūta serverim, ja viņš vēlas paiet kaut kādā virzienā. Viņš nosūta savu
spēlētāja ID un kustības virzienu, kurā viņš vēlas iet. Ja tur var iet, tad serveris nosūta
tālāk visiem klientiem “MOVE”.
Kustības virziens ir kodēts kā ASCII simbols: U-up, D-dowm, L-left, R-right.
*/
int send_move_attempt(int fd, uint8_t sender_id, uint8_t direction);

/*
Šo ziņu klients sūta serverim, ja viņš vēlas nolikt spridzekli. Serveris pārbauda, un, ja var,
nosūta visiem klientiem “BOMB” paketi.
*/
int send_bomb_attempt(int fd, uint8_t sender_id, const uint16_t row, const uint16_t col, const uint16_t map_cols);


/*
Šo ziņu klients sūta serverim, lai izvēlētos karti no piedāvātajām iespējām.
*/
int send_map_selected(int fd, uint8_t sender_id, uint8_t target_id, uint8_t map_id);
