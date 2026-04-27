#pragma once
#include "../shared/protocol.h"

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


/*
Ziņu nosūta serveris klientam, pirms close(socket). Pēc šīs ziņas saņemšanas klientam
vajadzētu aizvērt TCP savienojumu.
*/
int send_disconnect(int fd, uint8_t sender_id, uint8_t target_id);


/*
Šo ziņu serveris sūta klientiem ar tekošo kartes informāciju. W=width, H=height. Katrai
šūnai viens baits, līdzīgi kā kartes konfigurācijas failā.
*/
int send_map(int fd, uint8_t sender_id, uint8_t target_id, const msg_map_t *msg, const uint8_t *cells);


/*
Šo ziņu serveris sūta visiem klientiem, lai informētu, ka kāds spēlētājs ir pakustējies uz
jaunu šūnu.
*/
int send_moved(int fd, uint8_t sender_id, uint8_t target_id, const msg_moved_t *msg);

