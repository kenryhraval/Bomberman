#!/usr/bin/env python3
import argparse
import socket
import struct
import subprocess
import sys
import time
import traceback
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

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
MSG_WINNER = 23
MSG_MOVE_ATTEMPT = 30
MSG_MOVED = 40
MSG_BOMB_ATTEMPT = 31
MSG_BOMB = 41
MSG_EXPLOSION_START = 42
MSG_EXPLOSION_END = 43
MSG_DEATH = 44
MSG_BONUS_AVAILABLE = 45
MSG_BONUS_RETRIEVED = 46
MSG_BLOCK_DESTROYED = 47
MSG_SYNC_BOARD = 100

GAME_LOBBY = 0
GAME_RUNNING = 1
GAME_END = 2

DIRS = {
    "U": (-1, 0),
    "D": (1, 0),
    "L": (0, -1),
    "R": (0, 1),
}

BLOCKED_CELLS = {"H", "S", "B"}

DEBUG = False
DEBUG_HEX_BYTES = 64


def dbg(msg: str) -> None:
    if DEBUG:
        ts = time.strftime("%H:%M:%S")
        print(f"[DBG {ts}] {msg}", file=sys.stderr, flush=True)


def hex_preview(data: bytes, limit: int = 64) -> str:
    shown = data[:limit]
    suffix = "" if len(data) <= limit else f"...(+{len(data) - limit}B)"
    return shown.hex(" ") + suffix


@dataclass
class ParsedMap:
    rows: int
    cols: int
    cells: List[str]
    start_positions: Dict[int, Tuple[int, int]]


def recv_exact(sock: socket.socket, size: int) -> bytes:
    dbg(f"recv_exact: waiting for {size} bytes")
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            dbg(
                "recv_exact: socket closed "
                f"after {len(data)}/{size} bytes, partial={hex_preview(bytes(data), DEBUG_HEX_BYTES)}"
            )
            raise ConnectionError(
                f"Socket closed while receiving data ({len(data)}/{size} bytes)"
            )
        data.extend(chunk)
        dbg(
            f"recv_exact: got chunk={len(chunk)} total={len(data)}/{size} "
            f"chunk_hex={hex_preview(chunk, DEBUG_HEX_BYTES)}"
        )
    return bytes(data)


def fixed_str_encode(value: str, field_len: int) -> bytes:
    raw = value.encode("utf-8")
    if len(raw) > field_len - 1:
        raw = raw[: field_len - 1]
    return raw + b"\x00" * (field_len - len(raw))


def fixed_str_decode(data: bytes) -> str:
    return data.split(b"\x00", 1)[0].decode("utf-8", errors="replace")


def send_header(sock: socket.socket, msg_type: int, sender_id: int, target_id: int) -> None:
    packet = struct.pack("!BBB", msg_type, sender_id, target_id)
    dbg(
        f"send_header: type={msg_type} sender={sender_id} target={target_id} "
        f"hex={hex_preview(packet, DEBUG_HEX_BYTES)}"
    )
    sock.sendall(packet)


def send_hello(sock: socket.socket, player_name: str, client_id: str = "bomb-client-0.1") -> None:
    send_header(sock, MSG_HELLO, SERVER, SERVER)
    payload = fixed_str_encode(client_id, MAX_CLIENT_ID_LEN + 1)
    payload += fixed_str_encode(player_name, MAX_NAME_LEN + 1)
    dbg(
        f"send_hello: client_id={client_id!r} player_name={player_name!r} "
        f"payload_len={len(payload)} hex={hex_preview(payload, DEBUG_HEX_BYTES)}"
    )
    sock.sendall(payload)


def send_set_ready(sock: socket.socket, my_id: int) -> None:
    dbg(f"send_set_ready: my_id={my_id}")
    send_header(sock, MSG_SET_READY, my_id, SERVER)


def send_move_attempt(sock: socket.socket, my_id: int, direction: str) -> None:
    if direction not in DIRS:
        raise ValueError(f"Invalid direction {direction}")
    send_header(sock, MSG_MOVE_ATTEMPT, my_id, SERVER)
    dbg(f"send_move_attempt: my_id={my_id} direction={direction}")
    sock.sendall(struct.pack("!B", ord(direction)))


def parse_map_file(path: Path) -> ParsedMap:
    text = path.read_text(encoding="utf-8").strip().splitlines()
    if not text:
        raise ValueError("Map file is empty")

    head = text[0].split()
    if len(head) < 2:
        raise ValueError("Invalid map header")

    rows = int(head[0])
    cols = int(head[1])

    if len(text[1:]) < rows:
        raise ValueError("Map has fewer rows than declared")

    cells: List[str] = []
    starts: Dict[int, Tuple[int, int]] = {}

    for r in range(rows):
        tokens = text[1 + r].split()
        if len(tokens) != cols:
            raise ValueError(f"Map row {r} has {len(tokens)} cols, expected {cols}")
        for c, token in enumerate(tokens):
            if len(token) != 1:
                raise ValueError(f"Invalid cell token '{token}' at ({r}, {c})")
            ch = token
            if "1" <= ch <= "8":
                starts[int(ch) - 1] = (r, c)
                cells.append(".")
            else:
                cells.append(ch)

    return ParsedMap(rows=rows, cols=cols, cells=cells, start_positions=starts)


def _sync_board_payload_size() -> int:
    return (
        1  # id
        + (MAX_NAME_LEN + 1)
        + 2  # row
        + 2  # col
        + 1  # alive
        + 1  # ready
        + 1  # bomb_count
        + 1  # bomb_radius
        + 2  # bomb_timer_ticks
        + 2  # speed
        + 8  # last_move_tick
    )


def recv_message(sock: socket.socket) -> dict:
    header = recv_exact(sock, 3)
    msg_type, sender_id, target_id = struct.unpack("!BBB", header)
    dbg(f"recv_message: header type={msg_type} sender={sender_id} target={target_id}")

    out = {
        "msg_type": msg_type,
        "sender_id": sender_id,
        "target_id": target_id,
        "payload": None,
    }

    if msg_type in {MSG_DISCONNECT, MSG_LEAVE, MSG_PING, MSG_PONG, MSG_SET_READY}:
        dbg(f"recv_message: short message type={msg_type}")
        return out

    if msg_type == MSG_HELLO:
        payload = recv_exact(sock, (MAX_CLIENT_ID_LEN + 1) + (MAX_NAME_LEN + 1))
        out["payload"] = {
            "client_id": fixed_str_decode(payload[: MAX_CLIENT_ID_LEN + 1]),
            "player_name": fixed_str_decode(payload[MAX_CLIENT_ID_LEN + 1 :]),
        }
        dbg(
            "recv_message: HELLO "
            f"client_id={out['payload']['client_id']!r} player_name={out['payload']['player_name']!r}"
        )
        return out

    if msg_type == MSG_WELCOME:
        others_entry_size = 1 + 1 + (MAX_NAME_LEN + 1)
        payload = recv_exact(
            sock,
            (MAX_CLIENT_ID_LEN + 1)
            + 1
            + 1
            + (MAX_PLAYERS * others_entry_size),
        )
        server_id = fixed_str_decode(payload[: MAX_CLIENT_ID_LEN + 1])
        game_status = payload[MAX_CLIENT_ID_LEN + 1]
        other_count = payload[MAX_CLIENT_ID_LEN + 2]

        others = []
        pos = MAX_CLIENT_ID_LEN + 3
        for _ in range(MAX_PLAYERS):
            player_id = payload[pos]
            ready = payload[pos + 1] != 0
            name = fixed_str_decode(payload[pos + 2 : pos + 2 + MAX_NAME_LEN + 1])
            pos += others_entry_size
            others.append({"player_id": player_id, "ready": ready, "name": name})

        out["payload"] = {
            "server_id": server_id,
            "game_status": game_status,
            "other_count": other_count,
            "others": others[:other_count],
        }
        dbg(
            "recv_message: WELCOME "
            f"server_id={server_id!r} status={game_status} other_count={other_count}"
        )
        return out

    if msg_type == MSG_ERROR:
        # Variable length by spec, but server currently does not send it in implemented scope.
        raise RuntimeError("MSG_ERROR payload is variable length and unsupported in this tester")

    if msg_type == MSG_SET_STATUS:
        payload = recv_exact(sock, 1)
        out["payload"] = {"game_status": payload[0]}
        dbg(f"recv_message: SET_STATUS status={payload[0]}")
        return out

    if msg_type == MSG_WINNER:
        payload = recv_exact(sock, 1)
        out["payload"] = {"winner_id": payload[0]}
        return out

    if msg_type == MSG_MAP:
        h, w = struct.unpack("!BB", recv_exact(sock, 2))
        cells = recv_exact(sock, h * w)
        out["payload"] = {"rows": h, "cols": w, "cells": cells}
        dbg(
            "recv_message: MAP "
            f"rows={h} cols={w} cells_len={len(cells)} cells_hex={hex_preview(cells, DEBUG_HEX_BYTES)}"
        )
        return out

    if msg_type == MSG_MOVE_ATTEMPT:
        d = recv_exact(sock, 1)
        out["payload"] = {"direction": chr(d[0])}
        return out

    if msg_type == MSG_BOMB_ATTEMPT:
        cell = struct.unpack("!H", recv_exact(sock, 2))[0]
        out["payload"] = {"cell": cell}
        return out

    if msg_type == MSG_MOVED:
        player_id = struct.unpack("!B", recv_exact(sock, 1))[0]
        cell = struct.unpack("!H", recv_exact(sock, 2))[0]
        out["payload"] = {"player_id": player_id, "cell": cell}
        dbg(f"recv_message: MOVED player_id={player_id} cell={cell}")
        return out

    if msg_type == MSG_BOMB:
        player_id = struct.unpack("!B", recv_exact(sock, 1))[0]
        cell = struct.unpack("!H", recv_exact(sock, 2))[0]
        out["payload"] = {"player_id": player_id, "cell": cell}
        return out

    if msg_type == MSG_EXPLOSION_START:
        radius = struct.unpack("!B", recv_exact(sock, 1))[0]
        cell = struct.unpack("!H", recv_exact(sock, 2))[0]
        out["payload"] = {"radius": radius, "cell": cell}
        return out

    if msg_type == MSG_EXPLOSION_END:
        cell = struct.unpack("!H", recv_exact(sock, 2))[0]
        out["payload"] = {"cell": cell}
        return out

    if msg_type == MSG_DEATH:
        player_id = struct.unpack("!B", recv_exact(sock, 1))[0]
        out["payload"] = {"player_id": player_id}
        return out

    if msg_type == MSG_BONUS_AVAILABLE:
        bonus = struct.unpack("!B", recv_exact(sock, 1))[0]
        cell = struct.unpack("!H", recv_exact(sock, 2))[0]
        out["payload"] = {"bonus_type": bonus, "cell": cell}
        return out

    if msg_type == MSG_BONUS_RETRIEVED:
        player_id = struct.unpack("!B", recv_exact(sock, 1))[0]
        cell = struct.unpack("!H", recv_exact(sock, 2))[0]
        out["payload"] = {"player_id": player_id, "cell": cell}
        return out

    if msg_type == MSG_BLOCK_DESTROYED:
        cell = struct.unpack("!H", recv_exact(sock, 2))[0]
        out["payload"] = {"cell": cell}
        return out

    if msg_type == MSG_SYNC_BOARD:
        payload = recv_exact(sock, _sync_board_payload_size())
        out["payload"] = {"raw": payload}
        dbg(
            "recv_message: SYNC_BOARD "
            f"payload_len={len(payload)} payload_hex={hex_preview(payload, DEBUG_HEX_BYTES)}"
        )
        return out

    raise RuntimeError(f"Unsupported message type: {msg_type}")


def recv_until(sock: socket.socket, wanted: int, timeout_s: float) -> dict:
    dbg(f"recv_until: waiting for msg_type={wanted} timeout={timeout_s}s")
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        left = max(0.01, deadline - time.time())
        sock.settimeout(left)
        try:
            msg = recv_message(sock)
        except socket.timeout:
            break

        if msg["msg_type"] == wanted:
            dbg(f"recv_until: matched msg_type={wanted}")
            return msg

    raise TimeoutError(f"Did not receive message type {wanted} within {timeout_s:.2f}s")


def recv_moved_for(sock: socket.socket, player_id: int, timeout_s: float) -> dict:
    dbg(f"recv_moved_for: waiting for MOVED player_id={player_id} timeout={timeout_s}s")
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        left = max(0.01, deadline - time.time())
        sock.settimeout(left)
        try:
            msg = recv_message(sock)
        except socket.timeout:
            break

        if msg["msg_type"] == MSG_MOVED and msg["payload"]["player_id"] == player_id:
            dbg("recv_moved_for: matched MOVED")
            return msg

    raise TimeoutError("Expected MOVED was not received")


def assert_no_moved_for(sock: socket.socket, player_id: int, timeout_s: float) -> None:
    dbg(f"assert_no_moved_for: player_id={player_id} timeout={timeout_s}s")
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        left = max(0.01, deadline - time.time())
        sock.settimeout(left)
        try:
            msg = recv_message(sock)
        except socket.timeout:
            return

        if msg["msg_type"] == MSG_MOVED and msg["payload"]["player_id"] == player_id:
            raise AssertionError("Received unexpected MOVED for blocked move")


def choose_walkable_move(
    row: int,
    col: int,
    rows: int,
    cols: int,
    cells: List[str],
    occupied: Dict[int, Tuple[int, int]],
    me_id: int,
) -> Optional[Tuple[str, int, int]]:
    for d, (dr, dc) in DIRS.items():
        nr = row + dr
        nc = col + dc
        if nr < 0 or nr >= rows or nc < 0 or nc >= cols:
            continue
        idx = nr * cols + nc
        if cells[idx] in BLOCKED_CELLS:
            continue

        blocked_by_player = False
        for pid, pos in occupied.items():
            if pid == me_id:
                continue
            if pos == (nr, nc):
                blocked_by_player = True
                break
        if blocked_by_player:
            continue

        return d, nr, nc
    return None


def choose_blocked_move(
    row: int,
    col: int,
    rows: int,
    cols: int,
    cells: List[str],
    occupied: Dict[int, Tuple[int, int]],
    me_id: int,
) -> Optional[str]:
    for d, (dr, dc) in DIRS.items():
        nr = row + dr
        nc = col + dc
        if nr < 0 or nr >= rows or nc < 0 or nc >= cols:
            return d
        idx = nr * cols + nc
        if cells[idx] in BLOCKED_CELLS:
            return d
        for pid, pos in occupied.items():
            if pid != me_id and pos == (nr, nc):
                return d
    return None


def run_test(args: argparse.Namespace) -> int:
    dbg(f"run_test args={args}")
    parsed_map = parse_map_file(Path(args.map))
    dbg(
        "parsed_map: "
        f"rows={parsed_map.rows} cols={parsed_map.cols} starts={parsed_map.start_positions}"
    )

    server_proc = None
    if args.spawn_server:
        server_cmd = [args.server_bin, "--map", args.map]
        dbg(f"spawning server: {' '.join(server_cmd)}")
        server_proc = subprocess.Popen(server_cmd)
        time.sleep(0.25)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.settimeout(args.timeout)
        dbg(f"connecting to {(args.host, args.port)}")
        sock.connect((args.host, args.port))

        print("[TEST] Connected to server")
        send_hello(sock, args.player_name, args.client_id)

        welcome_msg = recv_until(sock, MSG_WELCOME, timeout_s=args.timeout)
        if welcome_msg["sender_id"] != SERVER:
            my_id = welcome_msg["sender_id"]
        else:
            my_id = welcome_msg["target_id"]

        if welcome_msg["sender_id"] == SERVER:
            dbg(
                "WELCOME uses sender_id=SERVER and target_id as assigned id; "
                "accepted for compatibility"
            )

        welcome = welcome_msg["payload"]

        print(f"[TEST] WELCOME received: assigned id={my_id}, status={welcome['game_status']}")
        if welcome["server_id"] != args.expected_server_id:
            raise AssertionError(
                f"Unexpected server id: {welcome['server_id']} != {args.expected_server_id}"
            )

        if welcome["game_status"] not in (GAME_LOBBY, GAME_RUNNING, GAME_END):
            raise AssertionError("WELCOME has invalid game_status")

        # Connection test: HELLO->WELCOME passed. Continue with map/movement test.
        send_set_ready(sock, my_id)
        print("[TEST] SET_READY sent")

        # Wait for GAME_RUNNING and MAP in any order.
        got_running = welcome["game_status"] == GAME_RUNNING
        got_map = False
        server_map_rows = 0
        server_map_cols = 0
        server_map_cells = b""

        deadline = time.time() + args.timeout
        while time.time() < deadline and (not got_running or not got_map):
            left = max(0.01, deadline - time.time())
            sock.settimeout(left)
            try:
                msg = recv_message(sock)
            except socket.timeout:
                break

            if msg["msg_type"] == MSG_DISCONNECT:
                raise AssertionError("Server sent DISCONNECT after SET_READY")

            if msg["msg_type"] == MSG_SET_STATUS:
                got_running = msg["payload"]["game_status"] == GAME_RUNNING
            elif msg["msg_type"] == MSG_MAP:
                got_map = True
                server_map_rows = msg["payload"]["rows"]
                server_map_cols = msg["payload"]["cols"]
                server_map_cells = msg["payload"]["cells"]

        if not got_map:
            raise AssertionError("Did not receive MAP after starting game")

        print("[TEST] MAP received")

        if server_map_rows != parsed_map.rows or server_map_cols != parsed_map.cols:
            raise AssertionError(
                f"MAP size mismatch: got {server_map_rows}x{server_map_cols}, "
                f"expected {parsed_map.rows}x{parsed_map.cols}"
            )

        expected_cells = "".join(parsed_map.cells).encode("ascii")
        if server_map_cells != expected_cells:
            raise AssertionError("MAP cells mismatch against map file")

        print("[TEST] MAP payload matches map file")

        if my_id not in parsed_map.start_positions:
            raise AssertionError(
                f"Map does not have start position for assigned player id={my_id}"
            )

        me_row, me_col = parsed_map.start_positions[my_id]
        occupied = dict(parsed_map.start_positions)

        move = choose_walkable_move(
            me_row,
            me_col,
            parsed_map.rows,
            parsed_map.cols,
            parsed_map.cells,
            occupied,
            my_id,
        )
        if move is None:
            raise AssertionError("No valid movement direction from start position")

        direction, nr, nc = move
        expected_cell = nr * parsed_map.cols + nc

        send_move_attempt(sock, my_id, direction)
        print(f"[TEST] MOVE_ATTEMPT sent: {direction}")

        moved = recv_moved_for(sock, my_id, timeout_s=args.timeout)
        moved_cell = moved["payload"]["cell"]
        if moved_cell != expected_cell:
            raise AssertionError(
                f"MOVED mismatch: got cell={moved_cell}, expected={expected_cell}"
            )

        print("[TEST] MOVED received with expected cell index")

        occupied[my_id] = (nr, nc)
        blocked_dir = choose_blocked_move(
            nr,
            nc,
            parsed_map.rows,
            parsed_map.cols,
            parsed_map.cells,
            occupied,
            my_id,
        )

        if blocked_dir is not None:
            send_move_attempt(sock, my_id, blocked_dir)
            print(f"[TEST] MOVE_ATTEMPT (blocked) sent: {blocked_dir}")
            assert_no_moved_for(sock, my_id, timeout_s=0.35)
            print("[TEST] Blocked movement correctly produced no MOVED")
        else:
            print("[TEST] Blocked-move probe skipped (no blocked adjacent cell)")

        send_header(sock, MSG_LEAVE, my_id, SERVER)
        print("[PASS] Connection, MAP load, and movement checks passed")
        return 0

    finally:
        try:
            sock.close()
        except Exception:
            pass
        if server_proc is not None:
            server_proc.terminate()
            try:
                server_proc.wait(timeout=1.0)
            except subprocess.TimeoutExpired:
                server_proc.kill()


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Spec-compliant Bomberman protocol test client for connection/map/movement"
    )
    p.add_argument("--host", default="127.0.0.1", help="Server host")
    p.add_argument("--port", type=int, default=6969, help="Server TCP port")
    p.add_argument("--map", required=True, help="Path to map file used by server")
    p.add_argument("--player-name", default="spec_tester", help="Player name for HELLO")
    p.add_argument("--client-id", default="bomb-client-0.1", help="HELLO client id")
    p.add_argument("--expected-server-id", default="bomb-server-0.1", help="Expected server identifier in WELCOME")
    p.add_argument("--timeout", type=float, default=3.0, help="Timeout per stage (seconds)")
    p.add_argument("--spawn-server", action="store_true", help="Spawn server process for the test")
    p.add_argument("--server-bin", default="./build/server_app", help="Path to server binary (used with --spawn-server)")
    p.add_argument("--debug", action="store_true", help="Enable packet-level debug logs")
    p.add_argument(
        "--debug-hex-bytes",
        type=int,
        default=64,
        help="How many bytes to show in debug hex previews",
    )
    return p


def main() -> int:
    parser = build_arg_parser()
    args = parser.parse_args()

    global DEBUG, DEBUG_HEX_BYTES
    DEBUG = args.debug
    DEBUG_HEX_BYTES = max(8, args.debug_hex_bytes)

    try:
        return run_test(args)
    except Exception as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        if DEBUG:
            traceback.print_exc()
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
