#!/usr/bin/env python3
import argparse
import os
import queue
import socket
import struct
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional, Tuple

tk = None

MAX_PLAYERS = 8
MAX_CLIENT_ID_LEN = 19
MAX_NAME_LEN = 29

SERVER = 255
BROADCAST = 254

MSG_HELLO = 0
MSG_WELCOME = 1
MSG_DISCONNECT = 2
MSG_PING = 3
MSG_PONG = 4
MSG_LEAVE = 5
MSG_ERROR = 6
MSG_MAP = 7
MSG_SET_READY = 10
MSG_SET_STATUS = 20
MSG_MOVE_ATTEMPT = 30
MSG_MOVED = 40
MSG_SYNC_BOARD = 100

GAME_LOBBY = 0
GAME_RUNNING = 1
GAME_END = 2

CELL_SIZE = 36

DIRECTION_BY_KEY = {
    "Up": "U",
    "Down": "D",
    "Left": "L",
    "Right": "R",
    "w": "U",
    "s": "D",
    "a": "L",
    "d": "R",
}


@dataclass
class PlayerState:
    player_id: int
    row: int
    col: int
    alive: bool


class ProtocolError(Exception):
    pass


def recv_exact(sock: socket.socket, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("Socket closed")
        data.extend(chunk)
    return bytes(data)


def fixed_str_encode(value: str, field_len: int) -> bytes:
    raw = value.encode("utf-8")
    if len(raw) > field_len - 1:
        raw = raw[: field_len - 1]
    return raw + b"\x00" * (field_len - len(raw))


def fixed_str_decode(data: bytes) -> str:
    return data.split(b"\x00", 1)[0].decode("utf-8", errors="replace")


class NetworkClient:
    def __init__(self, host: str, port: int, player_name: str, client_id: str, event_queue: "queue.Queue[tuple]"):
        self.host = host
        self.port = port
        self.player_name = player_name
        self.client_id = client_id
        self.event_queue = event_queue

        self.sock: Optional[socket.socket] = None
        self.running = False
        self.my_id: Optional[int] = None
        self.server_id: str = ""
        self.game_status = GAME_LOBBY

    def send_header(self, msg_type: int, sender_id: int, target_id: int) -> None:
        if self.sock is None:
            raise ConnectionError("Not connected")
        self.sock.sendall(struct.pack("!BBB", msg_type, sender_id, target_id))

    def send_hello(self) -> None:
        if self.sock is None:
            raise ConnectionError("Not connected")

        self.send_header(MSG_HELLO, SERVER, SERVER)
        payload = fixed_str_encode(self.client_id, MAX_CLIENT_ID_LEN + 1)
        payload += fixed_str_encode(self.player_name, MAX_NAME_LEN + 1)
        self.sock.sendall(payload)

    def send_set_ready(self) -> None:
        if self.my_id is None:
            return
        self.send_header(MSG_SET_READY, self.my_id, SERVER)

    def send_move_attempt(self, direction: str) -> None:
        if self.sock is None:
            return
        if self.my_id is None:
            return

        self.send_header(MSG_MOVE_ATTEMPT, self.my_id, SERVER)
        self.sock.sendall(struct.pack("!B", ord(direction)))

    def close(self) -> None:
        self.running = False
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None

    def _read_sync_board_payload(self, payload: bytes) -> PlayerState:
        # player_t is PACKED on server side.
        # Layout: uint8 id; char name[30]; uint16 row; uint16 col; bool alive; bool ready;
        # uint8 bomb_count; uint8 bomb_radius; uint16 bomb_timer_ticks; uint16 speed; uint64 last_move_tick.
        if len(payload) != 51:
            raise ProtocolError(f"Invalid SYNC_BOARD payload size: {len(payload)}")

        player_id = payload[0]
        # name is payload[1:31]
        row = int.from_bytes(payload[31:33], byteorder="little", signed=False)
        col = int.from_bytes(payload[33:35], byteorder="little", signed=False)
        alive = payload[35] != 0

        return PlayerState(player_id=player_id, row=row, col=col, alive=alive)

    def recv_message(self) -> tuple:
        if self.sock is None:
            raise ConnectionError("Not connected")

        header = recv_exact(self.sock, 3)
        msg_type, sender_id, target_id = struct.unpack("!BBB", header)

        if msg_type in {MSG_DISCONNECT, MSG_LEAVE, MSG_PING, MSG_PONG, MSG_SET_READY}:
            return ("header_only", msg_type, sender_id, target_id)

        if msg_type == MSG_HELLO:
            payload = recv_exact(self.sock, (MAX_CLIENT_ID_LEN + 1) + (MAX_NAME_LEN + 1))
            client_id = fixed_str_decode(payload[: MAX_CLIENT_ID_LEN + 1])
            player_name = fixed_str_decode(payload[MAX_CLIENT_ID_LEN + 1 :])
            return ("hello", sender_id, target_id, client_id, player_name)

        if msg_type == MSG_WELCOME:
            others_entry_size = 1 + 1 + (MAX_NAME_LEN + 1)
            payload = recv_exact(
                self.sock,
                (MAX_CLIENT_ID_LEN + 1)
                + 1
                + 1
                + (MAX_PLAYERS * others_entry_size),
            )
            server_id = fixed_str_decode(payload[: MAX_CLIENT_ID_LEN + 1])
            game_status = payload[MAX_CLIENT_ID_LEN + 1]
            other_count = payload[MAX_CLIENT_ID_LEN + 2]

            return (
                "welcome",
                {
                    "sender_id": sender_id,
                    "target_id": target_id,
                    "server_id": server_id,
                    "game_status": game_status,
                    "other_count": other_count,
                },
            )

        if msg_type == MSG_SET_STATUS:
            payload = recv_exact(self.sock, 1)
            return ("set_status", payload[0])

        if msg_type == MSG_MAP:
            h, w = struct.unpack("!BB", recv_exact(self.sock, 2))
            cells = recv_exact(self.sock, h * w)
            return ("map", h, w, cells)

        if msg_type == MSG_MOVED:
            payload = recv_exact(self.sock, 3)
            player_id = payload[0]
            cell = struct.unpack("!H", payload[1:3])[0]
            return ("moved", player_id, cell)

        if msg_type == MSG_SYNC_BOARD:
            payload = recv_exact(self.sock, 51)
            player = self._read_sync_board_payload(payload)
            return ("sync_board", player)

        if msg_type == MSG_ERROR:
            raise ProtocolError("MSG_ERROR has variable payload and is not supported in GUI tester")

        raise ProtocolError(f"Unsupported msg type: {msg_type}")

    def run(self) -> None:
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5.0)
        self.sock.connect((self.host, self.port))
        self.sock.settimeout(None)

        self.send_hello()

        msg = self.recv_message()
        if msg[0] != "welcome":
            raise ProtocolError("Expected WELCOME as first response")

        welcome = msg[1]
        self.server_id = welcome["server_id"]
        self.game_status = welcome["game_status"]

        if welcome["sender_id"] != SERVER:
            self.my_id = welcome["sender_id"]
        else:
            self.my_id = welcome["target_id"]

        self.event_queue.put(("connected", self.my_id, self.server_id, self.game_status))

        self.send_set_ready()
        self.running = True

        while self.running:
            try:
                event = self.recv_message()
            except (ConnectionError, OSError) as exc:
                self.event_queue.put(("disconnected", str(exc)))
                break
            except Exception as exc:
                self.event_queue.put(("error", str(exc)))
                break

            self.event_queue.put(event)

        self.close()


class GuiApp:
    def __init__(self, root, host: str, port: int, player_name: str, client_id: str):
        self.root = root
        self.root.title(f"Bomberman Spec GUI Tester - {player_name}")

        self.event_queue: "queue.Queue[tuple]" = queue.Queue()
        self.client = NetworkClient(host, port, player_name, client_id, self.event_queue)

        self.rows = 0
        self.cols = 0
        self.cells: bytes = b""
        self.players: Dict[int, PlayerState] = {}

        self.my_id: Optional[int] = None
        self.status_text = tk.StringVar(value="Connecting...")

        self.status_label = tk.Label(root, textvariable=self.status_text, anchor="w")
        self.status_label.pack(fill="x", padx=8, pady=6)

        self.canvas = tk.Canvas(root, width=640, height=480, bg="#111111", highlightthickness=0)
        self.canvas.pack(fill="both", expand=True, padx=8, pady=8)

        self.help_label = tk.Label(
            root,
            text="Use Arrow keys or WASD to move",
            anchor="w",
        )
        self.help_label.pack(fill="x", padx=8, pady=(0, 8))

        self.root.bind("<KeyPress>", self.on_keypress)
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

        self.thread = threading.Thread(target=self._network_thread_main, daemon=True)
        self.thread.start()

        self.root.after(30, self.poll_events)

    def _network_thread_main(self) -> None:
        try:
            self.client.run()
        except Exception as exc:
            self.event_queue.put(("error", str(exc)))

    def on_close(self) -> None:
        self.client.close()
        self.root.destroy()

    def on_keypress(self, event) -> None:
        direction = DIRECTION_BY_KEY.get(event.keysym) or DIRECTION_BY_KEY.get(event.char)
        if direction is None:
            return
        self.client.send_move_attempt(direction)

    def poll_events(self) -> None:
        while True:
            try:
                evt = self.event_queue.get_nowait()
            except queue.Empty:
                break

            kind = evt[0]

            if kind == "connected":
                _, my_id, server_id, game_status = evt
                self.my_id = my_id
                self.root.title(f"Bomberman Spec GUI Tester - {self.client.player_name} (id={my_id})")
                self.status_text.set(
                    f"Connected: name={self.client.player_name}, my_id={my_id}, server={server_id}, status={game_status}. Waiting for MAP..."
                )

            elif kind == "set_status":
                _, game_status = evt
                self.status_text.set(f"Game status changed: {game_status}")

            elif kind == "map":
                _, h, w, cells = evt
                self.rows = h
                self.cols = w
                self.cells = cells
                self.resize_canvas()
                self.redraw()
                self.status_text.set(f"MAP received: {h}x{w}. Move with Arrow keys or WASD.")

            elif kind == "sync_board":
                _, player = evt
                self.players[player.player_id] = player
                self.redraw()

            elif kind == "moved":
                _, player_id, cell = evt
                if self.cols > 0:
                    row = cell // self.cols
                    col = cell % self.cols
                    prev = self.players.get(player_id)
                    alive = prev.alive if prev is not None else True
                    self.players[player_id] = PlayerState(player_id=player_id, row=row, col=col, alive=alive)
                    self.redraw()

            elif kind == "header_only":
                _, msg_type, sender_id, target_id = evt
                if msg_type == MSG_DISCONNECT:
                    self.status_text.set(f"Disconnected by server (sender={sender_id}, target={target_id})")

            elif kind == "hello":
                _, sender_id, target_id, _client_id, player_name = evt
                self.status_text.set(
                    f"Player joined: {player_name} (id={target_id}, announced_by={sender_id})"
                )

            elif kind == "disconnected":
                _, reason = evt
                self.status_text.set(f"Disconnected: {reason}")

            elif kind == "error":
                _, reason = evt
                self.status_text.set(f"Error: {reason}")

        self.root.after(30, self.poll_events)

    def resize_canvas(self) -> None:
        if self.rows <= 0 or self.cols <= 0:
            return

        width = self.cols * CELL_SIZE
        height = self.rows * CELL_SIZE
        self.canvas.config(width=width, height=height)

    def cell_color(self, ch: str) -> str:
        if ch == "H":
            return "#4A5568"
        if ch == "S":
            return "#A0AEC0"
        if ch == "B":
            return "#1A202C"
        if ch == "A":
            return "#38A169"
        if ch == "R":
            return "#3182CE"
        if ch == "T":
            return "#DD6B20"
        return "#E2E8F0"

    def redraw(self) -> None:
        self.canvas.delete("all")

        if self.rows <= 0 or self.cols <= 0 or len(self.cells) != self.rows * self.cols:
            return

        for r in range(self.rows):
            for c in range(self.cols):
                idx = r * self.cols + c
                ch = chr(self.cells[idx])

                x0 = c * CELL_SIZE
                y0 = r * CELL_SIZE
                x1 = x0 + CELL_SIZE
                y1 = y0 + CELL_SIZE

                self.canvas.create_rectangle(
                    x0,
                    y0,
                    x1,
                    y1,
                    fill=self.cell_color(ch),
                    outline="#CBD5E0",
                    width=1,
                )

                if ch in {"A", "R", "T"}:
                    self.canvas.create_text(
                        x0 + CELL_SIZE / 2,
                        y0 + CELL_SIZE / 2,
                        text=ch,
                        fill="#1A202C",
                        font=("TkDefaultFont", 10, "bold"),
                    )

        for pid, player in self.players.items():
            if not player.alive:
                continue
            if not (0 <= player.row < self.rows and 0 <= player.col < self.cols):
                continue

            x0 = player.col * CELL_SIZE + 6
            y0 = player.row * CELL_SIZE + 6
            x1 = (player.col + 1) * CELL_SIZE - 6
            y1 = (player.row + 1) * CELL_SIZE - 6

            fill = "#E53E3E" if pid == self.my_id else "#2B6CB0"
            self.canvas.create_oval(x0, y0, x1, y1, fill=fill, outline="#1A202C", width=1)
            self.canvas.create_text(
                (x0 + x1) / 2,
                (y0 + y1) / 2,
                text=str(pid),
                fill="#FFFFFF",
                font=("TkDefaultFont", 10, "bold"),
            )


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Simple GUI Bomberman protocol tester")
    p.add_argument("--host", default="127.0.0.1", help="Server host")
    p.add_argument("--port", type=int, default=6969, help="Server TCP port")
    p.add_argument(
        "--player-name",
        default=None,
        help="Player name (if omitted, a unique per-process name is generated)",
    )
    p.add_argument("--client-id", default="bomb-client-0.1", help="HELLO client id")
    return p


def try_import_tkinter():
    try:
        import tkinter as tk_mod
    except ImportError as exc:
        print("Tkinter is not available on this Python installation.", file=sys.stderr)
        print("Install Tk libraries, then retry.", file=sys.stderr)
        print("Ubuntu/Debian example: sudo apt install python3-tk tk8.6", file=sys.stderr)
        print(f"Details: {exc}", file=sys.stderr)
        return None
    return tk_mod


def main() -> int:
    args = build_parser().parse_args()

    if args.player_name is None:
        # Unique by default, so multiple GUI instances can connect as independent clients.
        args.player_name = f"gui_tester_{os.getpid()}"

    tk_mod = try_import_tkinter()
    if tk_mod is None:
        return 1

    global tk
    tk = tk_mod

    root = tk.Tk()
    app = GuiApp(root, args.host, args.port, args.player_name, args.client_id)
    _ = app

    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
