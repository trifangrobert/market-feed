# temp/dummy_client.py
# Demo client:
#  1) connect to /tmp/demo.sock
#  2) send NEW (GTC)
#  3) read until ACK(new), printing any TRADEs
#  4) send CANCEL for that exch_order_id
#  5) read ACK(cancel)

import socket, struct

SOCK_PATH = "/tmp/demo.sock"

# MsgType constants (mirror C++)
RESERVED = 0
NEW = 1
CANCEL = 2
ACK = 3
TRADE = 4

PROTO_VER = 1  # kProtocolVersion

# ----------------- pack helpers -----------------

def pack_header(msg_type: int, body_size: int, seqno: int, ts_ns: int) -> bytes:
    # Header (24): <BBH xxxx QQ>
    return struct.pack("<BBHxxxxQQ", msg_type, PROTO_VER, 24 + body_size, seqno, ts_ns)

def pack_new_body(client_order_id: int, price_ticks: int, qty: int, instrument_id: int, side: int, flags: int) -> bytes:
    # OrderNewBody (32): <Q q i I B B H xxxx>
    return struct.pack("<Qq i I B B H xxxx", client_order_id, price_ticks, qty, instrument_id, side, flags, 0)

def pack_cancel_body(exch_order_id: int, client_order_id: int, instrument_id: int, reason_code: int) -> bytes:
    # OrderCancelBody (24): <Q Q I B xxx>
    return struct.pack("<QQ I B xxx", exch_order_id, client_order_id, instrument_id, reason_code)

# ----------------- recv helpers -----------------

def recv_exact(sock: socket.socket, n: int) -> bytes:
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise RuntimeError("EOF while reading")
        buf.extend(chunk)
    return bytes(buf)

def recv_frame(sock: socket.socket):
    hdr = recv_exact(sock, 24)
    msg_type, ver, size, seqno, ts_ns = struct.unpack("<BBHxxxxQQ", hdr)
    body_len = size - 24
    body = recv_exact(sock, body_len) if body_len > 0 else b""
    return (msg_type, ver, size, seqno, ts_ns), body

# ----------------- main -----------------

def main():
    # sanity: confirm sizes we assume
    assert struct.calcsize("<BBHxxxxQQ") == 24
    assert struct.calcsize("<Qq i I B B H xxxx") == 32
    assert struct.calcsize("<QQ I B xxx") == 24
    assert struct.calcsize("<QQBxxxxxxxQQ") == 40
    assert struct.calcsize("<qiBxxxQQIxxxx") == 40

    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(SOCK_PATH)

    # 1) NEW (Bid 30 @ 101, GTC flags=0)
    new_body = pack_new_body(
        client_order_id=42,
        price_ticks=101,
        qty=30,
        instrument_id=1,
        side=0,   # 0=Bid, 1=Ask
        flags=0   # 0=GTC (bit0 clear)
    )
    new_hdr = pack_header(NEW, len(new_body), seqno=1, ts_ns=123456789)
    s.sendall(new_hdr + new_body)

    # 2) Read frames until ACK(new)
    exch_order_id = 0
    while True:
        (t, ver, size, seq, ts), body = recv_frame(s)
        if t == ACK:
            # AckBody (40): <Q Q B xxxxxxx Q Q>
            client_id, exch_id, status, recv_ns, ack_ns = struct.unpack("<QQBxxxxxxxQQ", body)
            print(f"ACK(new): client_id={client_id} exch_id={exch_id} status={status}")
            exch_order_id = exch_id
            break
        elif t == TRADE:
            # TradeBody (40): <q i B xxx Q Q I xxxx>
            price_ticks, qty, liq, rest_id, taker_id, instr = struct.unpack("<qiBxxxQQIxxxx", body)
            print(f"TRADE: px={price_ticks} qty={qty} liq={liq} maker={rest_id} taker={taker_id} instr={instr}")
        else:
            print(f"unexpected type={t}; ignoring")

    # 3) CANCEL the order we just got back
    cancel_body = pack_cancel_body(
        exch_order_id=exch_order_id,
        client_order_id=43,  # a new client id for the cancel request
        instrument_id=1,
        reason_code=0
    )
    cancel_hdr = pack_header(CANCEL, len(cancel_body), seqno=2, ts_ns=123456790)
    s.sendall(cancel_hdr + cancel_body)

    # 4) Read CANCEL ACK
    while True:
        (t, ver, size, seq, ts), body = recv_frame(s)
        if t == ACK:
            client_id, exch_id, status, recv_ns, ack_ns = struct.unpack("<QQBxxxxxxxQQ", body)
            print(f"ACK(cancel): client_id={client_id} exch_id={exch_id} status={status}")
            break
        elif t == TRADE:
            # A cancel shouldn't generate trades; skip if any arrive due to prior book activity
            continue
        else:
            print(f"unexpected type={t}; ignoring")

    s.close()

if __name__ == "__main__":
    main()