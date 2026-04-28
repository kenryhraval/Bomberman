# Bomberman
Projekts ir vienkārša tīkla _Bomberman_ spēle ar servera un klienta komponentēm.

Satura kopsavilkums
- **Serveris**: pieņem klientu savienojumus un saņem un sūta ziņas pēc protokola, uztur spēles loģiku.
- **Klients**: savienojas ar serveri, nosūta lietotāja darbības un attēlo spēli terminālī (`ncurses`).
- **Shared**: kopīgās protokola struktūras, lasīšanas/rakstīšanas utilītfunkcijas un kopīgie tipi.

Komponentu pārskats – svarīgākie faili

Servera mape (`server/`):
- `main.c`: programmas ieejas punkts serverim; inicializē serveri un izsauc `serve_main()`.
- `server.c`: galvenais servera cikls – izveido _soketu_, klausās savienojumus, pieņem klientus, pievieno tos spēlei un startē klientu apstrādes pavedienus.
- `game.c`: spēles cikls un „tick” loģika – notikumu apstrāde, bumbu sprādzieni, spēlētāju nāves un uzvarētāja noteikšana.
- `handle_client.c`: klienta pavediena īstenojums – lasa ienākošās ziņas no klienta (MOVE, BOMB, PING, SET_READY, LEAVE), pārvalda watchdog/timeout mehānismus un izsauc apraides funkcijas.
- `map.c`: karšu ielādes un sagatavošanas funkcijas (`load_map`) – nolasīt kartes failu un iegūt sākuma pozīcijas un konfigurāciju.
- `broadcasts.c`: funkcijas, kas sūta apraides un atjauninājumu ziņojumus visiem klientiem (`MSG_MAP`, `MSG_MOVED`, `MSG_EXPLOSION_*` u. c.).
- `event_queue.c`: notikumu rinda, ko servera pavedieni izmanto, lai pievienotu/izņemtu spēles notikumus.
- `server_protocol.c` / `server_protocol.h`: servera puses utilītas protokola sūtīšanai/saņemšanai.

Klienta mape (`client/`):
- `main.c`: klienta programmas ieejas punkts — lietotāja vārda ievade, savienojuma izveide, `ncurses` cilpa un taustiņu apstrāde.
- `client.c`: tīkla savienojuma, rokasspiediena (`client_handshake`), savienojuma aizvēršanas un tīkla aptaujas loģika (`client_poll_network`).
- `client_protocol.c`: klienta puses palīgfunkcijas protokola ziņojumu sūtīšanai un saņemšanai (`send_move_attempt`, `send_bomb_attempt`, `recv_welcome` u. c.).
- `draw.c`: `ncurses` zīmēšanas funkcijas — priekšnama, spēles laukuma, sprādzienu un bonusu attēlošana.
- `helpers.c`: nelielas klienta puses palīgfunkcijas, piemēram, sprādzienu pārklājuma atzīmēšana un citas utilītas.

Kopīga daļa (`shared/`):
- `protocol.h` / `protocol.c`: zemā līmeņa I/O utilītas (`read_exact`, `write_exact`) un kopīgās funkcijas `send_header`, `send_hello`, `recv_hello` u.c., kā arī ziņojumu struktūras definīcijas atrodas `protocol.h`.
- `common.h`: kopīgas konstantes, tipu definīcijas un konfigurācijas, ko izmanto gan serveris, gan klients.

Kartes un konfigurācija
- Noklusējuma karte atrodas `map.txt`. Karte satur gan laukuma šūnu datus, gan sākuma pozīcijas/konfigurācijas lauciņus, kas tiek nolasīti ar `server/map.c`.


1. Kompilācija:

	```bash
	make
	```

2. Palaidiet serveri (no projekta saknes):

	```bash
	./build/server_app
	```

3. Palaidiet klientu (var atvērt vairākus klientus):

	```bash
	./build/client_app
	```

Protokola īss apraksts
- Pēc savienojuma serveris un klients apmainās ar `HELLO`/`WELCOME` ziņojumiem – identificē klienta versiju un spēlētāja vārdu.
- Klients nosūta ziņojumus: `MSG_SET_READY`, `MSG_MOVE_ATTEMPT`, `MSG_BOMB_ATTEMPT`, `MSG_PING`, `MSG_LEAVE`.
- Serveris pārraida atjauninājumus visiem klientiem: `MSG_MAP`, `MSG_MOVED`, `MSG_BOMB`, `MSG_EXPLOSION_START`, `MSG_EXPLOSION_END`, `MSG_DEATH`, `MSG_STATISTICS`, `MSG_WINNER` utt.
- Kopējās lasīšanas/ierakstīšanas utilītas atrodas `shared/protocol.c` un nodrošina drošu lasīšanu/ierakstīšanu uz _soketa_.

Projekta autori
- Skatīt `LICENSE` projekta saknē.

_Projekts izstrādāts kursa [„Linukss sistēmas programmēšana”](http://andromeda.df.lu.lv/wiki/index.php/LU-LSP-b) ietvaros._

