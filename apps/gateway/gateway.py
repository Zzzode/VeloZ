#!/usr/bin/env python3
import json
import hmac
import hashlib
import base64
import os
import socket
import ssl
import subprocess
import threading
import time
import urllib.error
import urllib.request
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse, urlencode
from collections import deque
from dataclasses import dataclass, asdict


class EngineBridge:
    def __init__(self, engine_cmd, market_source, market_symbol, binance_base_url, order_store):
        self._proc = subprocess.Popen(
            engine_cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        self._lock = threading.Lock()
        self._cv = threading.Condition(self._lock)
        self._latest_market = {"symbol": "BTCUSDT", "price": None, "ts_ns": None}
        self._orders = []
        self._max_orders = 200
        self._events = deque(maxlen=500)
        self._next_event_id = 0
        self._market_source = market_source
        self._market_symbol = market_symbol
        self._binance_base_url = binance_base_url
        self._market_thread = None
        self._external_market_last_ok_ns = 0
        self._order_store = order_store

        self._stdout_thread = threading.Thread(target=self._read_stdout, daemon=True)
        self._stderr_thread = threading.Thread(target=self._drain_stderr, daemon=True)
        self._stdout_thread.start()
        self._stderr_thread.start()

        if self._market_source == "binance_rest":
            self._market_thread = threading.Thread(target=self._poll_binance_price, daemon=True)
            self._market_thread.start()

    def stop(self):
        try:
            if self._proc.poll() is None:
                self._proc.terminate()
        except Exception:
            pass

    def market(self):
        with self._lock:
            return dict(self._latest_market)

    def orders(self):
        with self._lock:
            return list(self._orders)

    def config(self):
        return {
            "market_source": self._market_source,
            "market_symbol": self._market_symbol,
            "binance_base_url": self._binance_base_url if self._market_source == "binance_rest" else None,
        }

    def pop_events(self, last_id):
        with self._lock:
            if last_id is None:
                return list(self._events), (
                    self._events[-1]["_id"] if self._events else 0
                )

            out = []
            for e in self._events:
                if e["_id"] > last_id:
                    out.append(e)
            new_last = out[-1]["_id"] if out else last_id
            return out, new_last

    def wait_for_event(self, last_id, timeout_s):
        end = time.time() + timeout_s
        with self._lock:
            while True:
                if self._events and (
                    last_id is None or self._events[-1]["_id"] > last_id
                ):
                    return True
                remaining = end - time.time()
                if remaining <= 0:
                    return False
                self._cv.wait(timeout=remaining)

    def place_order(self, side, symbol, qty, price, client_order_id):
        if self._proc.stdin is None:
            raise RuntimeError("engine stdin unavailable")
        cmd = f"ORDER {side} {symbol} {qty} {price} {client_order_id}\n"
        self._proc.stdin.write(cmd)
        self._proc.stdin.flush()

    def cancel_order(self, client_order_id):
        if self._proc.stdin is None:
            raise RuntimeError("engine stdin unavailable")
        cmd = f"CANCEL {client_order_id}\n"
        self._proc.stdin.write(cmd)
        self._proc.stdin.flush()

    def _read_stdout(self):
        if self._proc.stdout is None:
            return
        for line in self._proc.stdout:
            line = line.strip()
            if not line:
                continue
            try:
                evt = json.loads(line)
            except Exception:
                continue
            t = evt.get("type")
            with self._lock:
                if (
                    t == "market"
                    and self._market_source == "binance_rest"
                    and (time.time_ns() - self._external_market_last_ok_ns) < 5_000_000_000
                ):
                    continue
                self._next_event_id += 1
                evt["_id"] = self._next_event_id
                self._events.append(evt)
                if t == "market":
                    self._latest_market = {
                        "symbol": evt.get("symbol"),
                        "price": evt.get("price"),
                        "ts_ns": evt.get("ts_ns"),
                    }
                elif t in ("order_update", "fill", "error"):
                    self._orders.append(evt)
                    if len(self._orders) > self._max_orders:
                        self._orders = self._orders[-self._max_orders :]
                if t in ("order_update", "fill") and self._order_store is not None:
                    self._order_store.apply_event({k: v for k, v in evt.items() if k != "_id"})
                self._cv.notify_all()

    def _drain_stderr(self):
        if self._proc.stderr is None:
            return
        for _ in self._proc.stderr:
            pass

    def _emit_event(self, evt):
        with self._lock:
            self._next_event_id += 1
            evt["_id"] = self._next_event_id
            self._events.append(evt)
            if evt.get("type") == "market":
                self._latest_market = {
                    "symbol": evt.get("symbol"),
                    "price": evt.get("price"),
                    "ts_ns": evt.get("ts_ns"),
                }
            self._cv.notify_all()

    def _poll_binance_price(self):
        last_error_emit = 0.0
        while True:
            if self._proc.poll() is not None:
                return
            try:
                symbol = self._market_symbol
                base = self._binance_base_url.rstrip("/")
                url = f"{base}/api/v3/ticker/price?symbol={symbol}"
                req = urllib.request.Request(url, headers={"User-Agent": "VeloZ/0.1"})
                with urllib.request.urlopen(req, timeout=2.0) as resp:
                    raw = resp.read().decode("utf-8", errors="replace")
                data = json.loads(raw)
                price = float(data["price"])
                self._external_market_last_ok_ns = time.time_ns()
                self._emit_event({"type": "market", "symbol": symbol, "ts_ns": time.time_ns(), "price": price})
            except Exception:
                now = time.time()
                if now - last_error_emit > 5.0:
                    last_error_emit = now
                    self._emit_event({"type": "error", "ts_ns": time.time_ns(), "message": "market_source_error"})
            time.sleep(0.5)

    def emit_event(self, evt):
        t = evt.get("type")
        with self._lock:
            self._next_event_id += 1
            evt["_id"] = self._next_event_id
            self._events.append(evt)
            if t == "market":
                self._latest_market = {
                    "symbol": evt.get("symbol"),
                    "price": evt.get("price"),
                    "ts_ns": evt.get("ts_ns"),
                }
            elif t in ("order_update", "fill", "error"):
                self._orders.append(evt)
                if len(self._orders) > self._max_orders:
                    self._orders = self._orders[-self._max_orders :]
            self._cv.notify_all()

@dataclass
class BalanceState:
    asset: str
    free: float
    locked: float


class AccountStore:
    def __init__(self):
        self._mu = threading.Lock()
        self._balances: dict[str, BalanceState] = {}
        self._last_ts_ns: int | None = None

    def set_balances(self, balances: list[BalanceState], ts_ns: int | None):
        with self._mu:
            self._balances = {b.asset: b for b in balances}
            self._last_ts_ns = ts_ns

    def snapshot(self):
        with self._mu:
            return {
                "ts_ns": self._last_ts_ns,
                "balances": [asdict(b) for b in self._balances.values()],
            }


@dataclass
class OrderState:
    client_order_id: str
    symbol: str | None = None
    side: str | None = None
    order_qty: float | None = None
    limit_price: float | None = None
    venue_order_id: str | None = None
    status: str | None = None
    reason: str | None = None
    executed_qty: float = 0.0
    avg_price: float | None = None
    last_ts_ns: int | None = None


class OrderStore:
    def __init__(self):
        self._mu = threading.Lock()
        self._orders: dict[str, OrderState] = {}

    def note_order_params(self, *, client_order_id, symbol, side, qty, price):
        if not client_order_id:
            return
        with self._mu:
            st = self._orders.get(client_order_id)
            if st is None:
                st = OrderState(client_order_id=str(client_order_id))
                self._orders[client_order_id] = st
            if symbol:
                st.symbol = str(symbol)
            if side:
                st.side = str(side)
            try:
                if qty is not None:
                    st.order_qty = float(qty)
            except Exception:
                pass
            try:
                if price is not None:
                    st.limit_price = float(price)
            except Exception:
                pass

    def apply_event(self, evt: dict):
        t = evt.get("type")
        if t not in ("order_update", "fill"):
            return

        cid = evt.get("client_order_id")
        if not cid:
            return

        with self._mu:
            st = self._orders.get(cid)
            if st is None:
                st = OrderState(client_order_id=str(cid))
                self._orders[cid] = st

            if t == "order_update":
                if evt.get("symbol"):
                    st.symbol = evt.get("symbol")
                if evt.get("side"):
                    st.side = evt.get("side")
                if evt.get("qty") is not None:
                    try:
                        st.order_qty = float(evt.get("qty"))
                    except Exception:
                        pass
                if evt.get("price") is not None:
                    try:
                        st.limit_price = float(evt.get("price"))
                    except Exception:
                        pass
                if evt.get("venue_order_id"):
                    st.venue_order_id = evt.get("venue_order_id")
                if evt.get("status"):
                    st.status = evt.get("status")
                if evt.get("reason"):
                    st.reason = evt.get("reason")
                if evt.get("ts_ns"):
                    st.last_ts_ns = evt.get("ts_ns")
                return

            if t == "fill":
                if evt.get("symbol"):
                    st.symbol = evt.get("symbol")
                qty = evt.get("qty")
                price = evt.get("price")
                try:
                    if qty is not None:
                        st.executed_qty += float(qty)
                except Exception:
                    pass
                try:
                    if st.executed_qty > 0 and price is not None:
                        st.avg_price = float(price)
                except Exception:
                    pass
                if evt.get("ts_ns"):
                    st.last_ts_ns = evt.get("ts_ns")
                if st.order_qty is not None and st.order_qty > 0:
                    if st.executed_qty + 1e-12 >= st.order_qty:
                        if st.status not in ("CANCELLED", "REJECTED", "EXPIRED"):
                            st.status = "FILLED"
                    elif st.executed_qty > 0:
                        if st.status not in ("CANCELLED", "REJECTED", "EXPIRED"):
                            st.status = "PARTIALLY_FILLED"

    def list(self):
        with self._mu:
            return [asdict(v) for v in self._orders.values()]

    def get(self, client_order_id: str):
        with self._mu:
            v = self._orders.get(client_order_id)
            return asdict(v) if v else None


class BinanceSpotRestClient:
    def __init__(self, base_url, api_key, api_secret):
        self._base_url = (base_url or "").rstrip("/")
        self._api_key = api_key or ""
        self._api_secret = api_secret or ""

    def enabled(self):
        return bool(self._base_url and self._api_key and self._api_secret)

    def _sign(self, query_string):
        mac = hmac.new(self._api_secret.encode("utf-8"), query_string.encode("utf-8"), hashlib.sha256)
        return mac.hexdigest()

    def _request(self, method, path, params, signed, api_key_only=False):
        base_params = dict(params or {})
        headers = {"User-Agent": "VeloZ/0.1"}

        if api_key_only:
            headers["X-MBX-APIKEY"] = self._api_key
            query = urlencode(sorted(base_params.items()), doseq=True)
        elif signed:
            base_params.setdefault("timestamp", int(time.time() * 1000))
            base_params.setdefault("recvWindow", 5000)
            query = urlencode(sorted(base_params.items()), doseq=True)
            sig = self._sign(query)
            query = query + "&signature=" + sig
            headers["X-MBX-APIKEY"] = self._api_key
        else:
            query = urlencode(sorted(base_params.items()), doseq=True)

        url = f"{self._base_url}{path}"
        if query:
            url += "?" + query

        req = urllib.request.Request(url, method=method, headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=5.0) as resp:
                body = resp.read().decode("utf-8", errors="replace")
            return json.loads(body) if body else {}
        except urllib.error.HTTPError as e:
            raw = e.read().decode("utf-8", errors="replace")
            try:
                return {"_http_error": e.code, "_body": json.loads(raw) if raw else {}}
            except Exception:
                return {"_http_error": e.code, "_body": raw}
        except Exception as e:
            return {"_error": str(e)}

    def ping(self):
        return self._request("GET", "/api/v3/ping", {}, signed=False)

    def place_order(self, *, symbol, side, qty, price, client_order_id):
        params = {
            "symbol": symbol,
            "side": side,
            "type": "LIMIT",
            "timeInForce": "GTC",
            "quantity": qty,
            "price": price,
            "newClientOrderId": client_order_id,
        }
        return self._request("POST", "/api/v3/order", params, signed=True)

    def cancel_order(self, *, symbol, client_order_id):
        params = {"symbol": symbol, "origClientOrderId": client_order_id}
        return self._request("DELETE", "/api/v3/order", params, signed=True)

    def get_order(self, *, symbol, client_order_id):
        params = {"symbol": symbol, "origClientOrderId": client_order_id}
        return self._request("GET", "/api/v3/order", params, signed=True)

    def new_listen_key(self):
        return self._request("POST", "/api/v3/userDataStream", {}, signed=False, api_key_only=True)

    def keepalive_listen_key(self, listen_key):
        return self._request("PUT", "/api/v3/userDataStream", {"listenKey": listen_key}, signed=False, api_key_only=True)

    def close_listen_key(self, listen_key):
        return self._request("DELETE", "/api/v3/userDataStream", {"listenKey": listen_key}, signed=False, api_key_only=True)


class SimpleWebSocketClient:
    def __init__(self, ws_url, on_text, on_error):
        self._ws_url = ws_url
        self._on_text = on_text
        self._on_error = on_error
        self._sock = None
        self._mu = threading.Lock()
        self._closed = False

    def close(self):
        with self._mu:
            self._closed = True
            sock = self._sock
            self._sock = None
        try:
            if sock is not None:
                sock.close()
        except Exception:
            pass

    def run(self):
        try:
            self._connect()
            while True:
                with self._mu:
                    if self._closed:
                        return
                opcode, payload = self._recv_frame()
                if opcode is None:
                    return
                if opcode == 0x1:
                    try:
                        self._on_text(payload.decode("utf-8", errors="replace"))
                    except Exception:
                        pass
                elif opcode == 0x8:
                    return
                elif opcode == 0x9:
                    self._send_pong(payload)
        except Exception as e:
            try:
                self._on_error(str(e))
            except Exception:
                pass
            return
        finally:
            self.close()

    def _parse_ws_url(self):
        u = urlparse(self._ws_url)
        if u.scheme not in ("ws", "wss"):
            raise ValueError("bad_ws_url")
        host = u.hostname
        if not host:
            raise ValueError("bad_ws_url")
        port = u.port or (443 if u.scheme == "wss" else 80)
        path = u.path or "/"
        if u.query:
            path += "?" + u.query
        return u.scheme, host, port, path

    def _connect(self):
        scheme, host, port, path = self._parse_ws_url()
        raw = socket.create_connection((host, port), timeout=10.0)
        if scheme == "wss":
            ctx = ssl.create_default_context()
            s = ctx.wrap_socket(raw, server_hostname=host)
        else:
            s = raw

        key = base64.b64encode(os.urandom(16)).decode("ascii")
        req = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {host}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        ).encode("utf-8")
        s.sendall(req)

        buf = b""
        while b"\r\n\r\n" not in buf:
            chunk = s.recv(4096)
            if not chunk:
                raise RuntimeError("ws_handshake_failed")
            buf += chunk
            if len(buf) > 64 * 1024:
                raise RuntimeError("ws_handshake_too_large")

        head, rest = buf.split(b"\r\n\r\n", 1)
        if b" 101 " not in head.split(b"\r\n", 1)[0]:
            raise RuntimeError("ws_handshake_not_101")

        with self._mu:
            self._sock = s
        self._recv_buffer = rest

    def _recv_exact(self, n):
        out = b""
        while len(out) < n:
            if self._recv_buffer:
                take = min(n - len(out), len(self._recv_buffer))
                out += self._recv_buffer[:take]
                self._recv_buffer = self._recv_buffer[take:]
                continue
            with self._mu:
                s = self._sock
            if s is None:
                raise RuntimeError("ws_closed")
            chunk = s.recv(4096)
            if not chunk:
                raise RuntimeError("ws_closed")
            self._recv_buffer += chunk
        return out

    def _recv_frame(self):
        b1, b2 = self._recv_exact(2)
        fin = (b1 & 0x80) != 0
        opcode = b1 & 0x0F
        masked = (b2 & 0x80) != 0
        ln = b2 & 0x7F
        if ln == 126:
            ln = int.from_bytes(self._recv_exact(2), "big")
        elif ln == 127:
            ln = int.from_bytes(self._recv_exact(8), "big")
        mask_key = b""
        if masked:
            mask_key = self._recv_exact(4)
        payload = self._recv_exact(ln) if ln else b""
        if masked and payload:
            payload = bytes(payload[i] ^ mask_key[i % 4] for i in range(len(payload)))
        if not fin:
            raise RuntimeError("ws_fragments_unsupported")
        return opcode, payload

    def _send_frame(self, opcode, payload):
        with self._mu:
            s = self._sock
            closed = self._closed
        if closed or s is None:
            return

        fin_opcode = 0x80 | (opcode & 0x0F)
        payload = payload or b""
        ln = len(payload)
        mask_bit = 0x80
        if ln < 126:
            header = bytes([fin_opcode, mask_bit | ln])
        elif ln < (1 << 16):
            header = bytes([fin_opcode, mask_bit | 126]) + ln.to_bytes(2, "big")
        else:
            header = bytes([fin_opcode, mask_bit | 127]) + ln.to_bytes(8, "big")
        mask_key = os.urandom(4)
        masked = bytes(payload[i] ^ mask_key[i % 4] for i in range(ln))
        s.sendall(header + mask_key + masked)

    def _send_pong(self, payload):
        self._send_frame(0xA, payload)


class ExecutionRouter:
    def __init__(self, *, bridge, execution_mode, binance_client, order_store):
        self._bridge = bridge
        self._execution_mode = execution_mode
        self._binance = binance_client
        self._orders = order_store
        self._account = AccountStore()
        self._poll_mu = threading.Lock()
        self._poll: dict[str, dict] = {}
        self._poll_thread = None
        self._user_stream_mu = threading.Lock()
        self._user_stream_connected = False
        self._user_stream_thread = None
        if self._execution_mode == "binance_testnet_spot" and self._binance and self._binance.enabled():
            self._poll_thread = threading.Thread(target=self._poll_binance_orders, daemon=True)
            self._poll_thread.start()
            self._user_stream_thread = threading.Thread(target=self._run_binance_user_stream, daemon=True)
            self._user_stream_thread.start()

    def config(self):
        with self._user_stream_mu:
            ws_connected = self._user_stream_connected
        return {
            "execution_mode": self._execution_mode,
            "binance_trade_enabled": bool(self._binance and self._binance.enabled()),
            "binance_trade_base_url": self._binance._base_url if self._execution_mode == "binance_testnet_spot" else None,
            "binance_user_stream_connected": ws_connected if self._execution_mode == "binance_testnet_spot" else False,
        }

    def orders_state(self):
        return self._orders.list()

    def order_state(self, client_order_id):
        return self._orders.get(client_order_id)

    def account_state(self):
        return self._account.snapshot()

    def ping(self):
        if self._execution_mode == "binance_testnet_spot":
            if not (self._binance and self._binance.enabled()):
                return {"ok": False, "error": "binance_not_configured"}
            r = self._binance.ping()
            ok = not (isinstance(r, dict) and (r.get("_error") or r.get("_http_error")))
            return {"ok": ok, "result": r}
        return {"ok": True}

    def place_order(self, side, symbol, qty, price, client_order_id):
        self._orders.note_order_params(client_order_id=client_order_id, symbol=symbol, side=side, qty=qty, price=price)
        if self._execution_mode == "binance_testnet_spot":
            if not (self._binance and self._binance.enabled()):
                raise RuntimeError("binance_not_configured")
            r = self._binance.place_order(
                symbol=symbol, side=side, qty=qty, price=price, client_order_id=client_order_id
            )
            if isinstance(r, dict) and (r.get("_error") or r.get("_http_error")):
                evt = {"type": "order_update", "ts_ns": time.time_ns(), "client_order_id": client_order_id, "status": "REJECTED", "reason": "binance_error", "symbol": symbol}
                self._bridge.emit_event(evt)
                self._orders.apply_event(evt)
                return {"ok": False, "error": "binance_error", "details": r}

            venue_order_id = str(r.get("orderId")) if isinstance(r, dict) and "orderId" in r else None
            evt = {"type": "order_update", "ts_ns": time.time_ns(), "client_order_id": client_order_id, "venue_order_id": venue_order_id, "status": "ACCEPTED", "symbol": symbol}
            self._bridge.emit_event(evt)
            self._orders.apply_event(evt)
            with self._poll_mu:
                self._poll[client_order_id] = {"symbol": symbol, "last_exec_qty": 0.0, "last_status": "ACCEPTED"}
            return {"ok": True, "client_order_id": client_order_id, "venue_order_id": venue_order_id}

        self._bridge.place_order(side, symbol, qty, price, client_order_id)
        return {"ok": True, "client_order_id": client_order_id}

    def cancel_order(self, client_order_id, symbol=None):
        if self._execution_mode == "binance_testnet_spot":
            if not (self._binance and self._binance.enabled()):
                raise RuntimeError("binance_not_configured")
            market_symbol = (symbol or self._bridge.config().get("market_symbol") or "BTCUSDT").strip().upper()
            r = self._binance.cancel_order(symbol=market_symbol, client_order_id=client_order_id)
            if isinstance(r, dict) and (r.get("_error") or r.get("_http_error")):
                evt = {"type": "order_update", "ts_ns": time.time_ns(), "client_order_id": client_order_id, "status": "REJECTED", "reason": "binance_error"}
                self._bridge.emit_event(evt)
                self._orders.apply_event(evt)
                return {"ok": False, "error": "binance_error", "details": r}
            evt = {"type": "order_update", "ts_ns": time.time_ns(), "client_order_id": client_order_id, "status": "CANCELLED"}
            self._bridge.emit_event(evt)
            self._orders.apply_event(evt)
            with self._poll_mu:
                self._poll.pop(client_order_id, None)
            return {"ok": True, "client_order_id": client_order_id}

        self._bridge.cancel_order(client_order_id)
        return {"ok": True, "client_order_id": client_order_id}

    def _map_binance_status(self, status):
        s = (status or "").upper()
        if s == "NEW":
            return "ACCEPTED"
        if s == "PARTIALLY_FILLED":
            return "PARTIALLY_FILLED"
        if s == "FILLED":
            return "FILLED"
        if s == "CANCELED":
            return "CANCELLED"
        if s == "REJECTED":
            return "REJECTED"
        if s == "EXPIRED":
            return "EXPIRED"
        return s or "UNKNOWN"

    def _poll_binance_orders(self):
        last_error_emit = 0.0
        while True:
            time.sleep(0.5)
            if not (self._binance and self._binance.enabled()):
                continue
            with self._user_stream_mu:
                if self._user_stream_connected:
                    continue

            with self._poll_mu:
                items = list(self._poll.items())

            for cid, meta in items:
                symbol = meta.get("symbol") or (self._bridge.config().get("market_symbol") or "BTCUSDT")
                r = self._binance.get_order(symbol=symbol, client_order_id=cid)
                if isinstance(r, dict) and (r.get("_error") or r.get("_http_error")):
                    now = time.time()
                    if now - last_error_emit > 5.0:
                        last_error_emit = now
                        self._bridge.emit_event({"type": "error", "ts_ns": time.time_ns(), "message": "binance_poll_error"})
                    continue

                status = self._map_binance_status(r.get("status") if isinstance(r, dict) else None)
                try:
                    exec_qty = float(r.get("executedQty", 0.0))
                except Exception:
                    exec_qty = 0.0
                try:
                    orig_qty = float(r.get("origQty", 0.0))
                except Exception:
                    orig_qty = 0.0
                try:
                    cq = float(r.get("cummulativeQuoteQty", 0.0))
                except Exception:
                    cq = 0.0
                avg_price = (cq / exec_qty) if exec_qty > 0 else None
                if orig_qty > 0:
                    self._orders.note_order_params(client_order_id=cid, symbol=symbol, side=None, qty=orig_qty, price=None)

                last_exec = float(meta.get("last_exec_qty", 0.0) or 0.0)
                if exec_qty > last_exec:
                    delta = exec_qty - last_exec
                    fill_evt = {"type": "fill", "ts_ns": time.time_ns(), "client_order_id": cid, "symbol": symbol, "qty": delta, "price": avg_price}
                    self._bridge.emit_event(fill_evt)
                    self._orders.apply_event(fill_evt)
                    meta["last_exec_qty"] = exec_qty

                last_status = meta.get("last_status")
                if status and status != last_status:
                    upd = {"type": "order_update", "ts_ns": time.time_ns(), "client_order_id": cid, "symbol": symbol, "status": status}
                    self._bridge.emit_event(upd)
                    self._orders.apply_event(upd)
                    meta["last_status"] = status

                if status in ("FILLED", "CANCELLED", "REJECTED", "EXPIRED"):
                    with self._poll_mu:
                        self._poll.pop(cid, None)
                else:
                    with self._poll_mu:
                        if cid in self._poll:
                            self._poll[cid] = meta

    def _handle_binance_execution_report(self, msg: dict):
        try:
            cid = str(msg.get("c") or "")
            if not cid:
                return
            symbol = str(msg.get("s") or "")
            side = str(msg.get("S") or "")
            venue_order_id = str(msg.get("i")) if msg.get("i") is not None else None
            status = self._map_binance_status(msg.get("X"))
            ts_ns = int(msg.get("E", 0)) * 1_000_000 if msg.get("E") is not None else time.time_ns()

            try:
                orig_qty = float(msg.get("q", 0.0))
            except Exception:
                orig_qty = 0.0
            try:
                limit_price = float(msg.get("p", 0.0))
            except Exception:
                limit_price = 0.0
            if orig_qty > 0:
                self._orders.note_order_params(client_order_id=cid, symbol=symbol, side=side, qty=orig_qty, price=limit_price)

            upd = {
                "type": "order_update",
                "ts_ns": ts_ns,
                "client_order_id": cid,
                "venue_order_id": venue_order_id,
                "status": status,
                "symbol": symbol,
                "side": side,
                "qty": orig_qty if orig_qty > 0 else None,
                "price": limit_price if limit_price > 0 else None,
            }
            self._bridge.emit_event(upd)
            self._orders.apply_event({k: v for k, v in upd.items() if v is not None})

            try:
                last_fill_qty = float(msg.get("l", 0.0))
            except Exception:
                last_fill_qty = 0.0
            if last_fill_qty > 0:
                try:
                    last_fill_price = float(msg.get("L", 0.0))
                except Exception:
                    last_fill_price = 0.0
                fill_evt = {"type": "fill", "ts_ns": ts_ns, "client_order_id": cid, "symbol": symbol, "qty": last_fill_qty, "price": last_fill_price if last_fill_price > 0 else None}
                self._bridge.emit_event(fill_evt)
                self._orders.apply_event({k: v for k, v in fill_evt.items() if v is not None})
        except Exception:
            return

    def _handle_binance_account_update(self, msg: dict):
        try:
            ts_ns = int(msg.get("E", 0)) * 1_000_000 if msg.get("E") is not None else time.time_ns()
            balances = []
            for b in msg.get("B", []) or []:
                asset = str(b.get("a") or "")
                if not asset:
                    continue
                try:
                    free = float(b.get("f", 0.0))
                except Exception:
                    free = 0.0
                try:
                    locked = float(b.get("l", 0.0))
                except Exception:
                    locked = 0.0
                balances.append(BalanceState(asset=asset, free=free, locked=locked))
            self._account.set_balances(balances, ts_ns)
            self._bridge.emit_event({"type": "account", "ts_ns": ts_ns})
        except Exception:
            return

    def _run_binance_user_stream(self):
        ws_base = os.environ.get("VELOZ_BINANCE_WS_BASE_URL", "wss://testnet.binance.vision/ws").strip() or "wss://testnet.binance.vision/ws"
        last_error_emit = 0.0

        while True:
            time.sleep(0.2)
            if not (self._binance and self._binance.enabled()):
                with self._user_stream_mu:
                    self._user_stream_connected = False
                continue

            lk = self._binance.new_listen_key()
            listen_key = lk.get("listenKey") if isinstance(lk, dict) else None
            if not listen_key:
                now = time.time()
                if now - last_error_emit > 5.0:
                    last_error_emit = now
                    self._bridge.emit_event({"type": "error", "ts_ns": time.time_ns(), "message": "listen_key_error"})
                continue

            stop_flag = {"stop": False}

            def on_text(txt):
                try:
                    msg = json.loads(txt)
                except Exception:
                    return
                et = msg.get("e")
                if et == "executionReport":
                    self._handle_binance_execution_report(msg)
                elif et == "outboundAccountPosition":
                    self._handle_binance_account_update(msg)

            def on_error(err):
                now = time.time()
                if now - last_error_emit > 5.0:
                    last_error_emit = now
                    self._bridge.emit_event({"type": "error", "ts_ns": time.time_ns(), "message": "binance_ws_error"})

            ws_url = ws_base.rstrip("/") + "/" + listen_key
            ws = SimpleWebSocketClient(ws_url, on_text=on_text, on_error=on_error)

            def keepalive():
                while not stop_flag["stop"]:
                    time.sleep(25 * 60)
                    if stop_flag["stop"]:
                        return
                    self._binance.keepalive_listen_key(listen_key)

            ka = threading.Thread(target=keepalive, daemon=True)
            ka.start()

            with self._user_stream_mu:
                self._user_stream_connected = True
            ws.run()
            stop_flag["stop"] = True
            try:
                ws.close()
            except Exception:
                pass
            try:
                self._binance.close_listen_key(listen_key)
            except Exception:
                pass
            with self._user_stream_mu:
                self._user_stream_connected = False


class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/health":
            self._send_json(200, {"ok": True})
            return
        if parsed.path == "/api/config":
            out = dict(self.server.bridge.config())
            out.update(self.server.router.config())
            self._send_json(200, out)
            return
        if parsed.path == "/api/orders_state":
            self._send_json(200, {"items": self.server.router.orders_state()})
            return
        if parsed.path == "/api/order_state":
            qs = parse_qs(parsed.query or "")
            cid = (qs.get("client_order_id") or [""])[0].strip()
            if not cid:
                self._send_json(400, {"error": "bad_params"})
                return
            st = self.server.router.order_state(cid)
            if not st:
                self._send_json(404, {"error": "not_found"})
                return
            self._send_json(200, st)
            return
        if parsed.path == "/api/execution/ping":
            self._send_json(200, self.server.router.ping())
            return
        if parsed.path == "/api/stream":
            self._handle_sse(parsed)
            return
        if parsed.path == "/api/market":
            self._send_json(200, self.server.bridge.market())
            return
        if parsed.path == "/api/orders":
            self._send_json(200, {"items": self.server.bridge.orders()})
            return
        if parsed.path == "/api/account":
            self._send_json(200, self.server.router.account_state())
            return
        if parsed.path == "/":
            self.path = "/index.html"
        return super().do_GET()

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/cancel":
            self._handle_cancel()
            return
        if parsed.path != "/api/order":
            self.send_error(404)
            return

        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        data = None
        ct = (self.headers.get("Content-Type") or "").split(";")[0].strip().lower()
        if ct == "application/json":
            try:
                data = json.loads(body) if body else {}
            except Exception:
                self._send_json(400, {"error": "bad_json"})
                return
        else:
            data = {k: v[0] for k, v in parse_qs(body).items()}

        try:
            side = str(data.get("side", "BUY")).upper()
            symbol = str(data.get("symbol", "BTCUSDT"))
            qty = float(data.get("qty", 0.0))
            price = float(data.get("price", 0.0))
            client_order_id = str(
                data.get("client_order_id", f"web-{int(time.time()*1000)}")
            )
        except Exception:
            self._send_json(400, {"error": "bad_params"})
            return

        if side not in ("BUY", "SELL") or qty <= 0.0 or price <= 0.0 or not symbol:
            self._send_json(400, {"error": "bad_params"})
            return

        try:
            r = self.server.router.place_order(side, symbol, qty, price, client_order_id)
        except RuntimeError as e:
            if str(e) == "binance_not_configured":
                self._send_json(400, {"error": "binance_not_configured"})
                return
            self._send_json(500, {"error": "execution_unavailable"})
            return

        self._send_json(200, r)

    def _handle_cancel(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8", errors="replace")

        data = None
        ct = (self.headers.get("Content-Type") or "").split(";")[0].strip().lower()
        if ct == "application/json":
            try:
                data = json.loads(body) if body else {}
            except Exception:
                self._send_json(400, {"error": "bad_json"})
                return
        else:
            data = {k: v[0] for k, v in parse_qs(body).items()}

        client_order_id = str(data.get("client_order_id", "")).strip()
        if not client_order_id:
            self._send_json(400, {"error": "bad_params"})
            return
        symbol = str(data.get("symbol", "")).strip().upper() or None

        try:
            r = self.server.router.cancel_order(client_order_id, symbol=symbol)
        except RuntimeError as e:
            if str(e) == "binance_not_configured":
                self._send_json(400, {"error": "binance_not_configured"})
                return
            self._send_json(500, {"error": "execution_unavailable"})
            return

        self._send_json(200, r)

    def _handle_sse(self, parsed):
        qs = parse_qs(parsed.query or "")
        last_id = None
        try:
            if "last_id" in qs:
                last_id = int(qs["last_id"][0])
        except Exception:
            last_id = None

        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()

        try:
            while True:
                ok = self.server.bridge.wait_for_event(last_id, timeout_s=10.0)
                if not ok:
                    self.wfile.write(b": keep-alive\n\n")
                    self.wfile.flush()
                    continue

                events, last_id = self.server.bridge.pop_events(last_id)
                for e in events:
                    eid = e.get("_id", 0)
                    payload = json.dumps({k: v for k, v in e.items() if k != "_id"})
                    msg = f"id: {eid}\nevent: {e.get('type','event')}\ndata: {payload}\n\n"
                    self.wfile.write(msg.encode("utf-8"))
                self.wfile.flush()
        except Exception:
            return

    def _send_json(self, status, obj):
        payload = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, format, *args):
        return


def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    ui_dir = os.path.join(repo_root, "apps", "ui")
    os.chdir(ui_dir)

    preset = os.environ.get("VELOZ_PRESET", "dev")
    market_source = os.environ.get("VELOZ_MARKET_SOURCE", "sim").strip().lower()
    if market_source not in ("sim", "binance_rest"):
        market_source = "sim"
    market_symbol = os.environ.get("VELOZ_MARKET_SYMBOL", "BTCUSDT").strip().upper() or "BTCUSDT"
    binance_base_url = os.environ.get("VELOZ_BINANCE_BASE_URL", "https://api.binance.com").strip() or "https://api.binance.com"
    execution_mode = os.environ.get("VELOZ_EXECUTION_MODE", "sim_engine").strip().lower()
    if execution_mode not in ("sim_engine", "binance_testnet_spot"):
        execution_mode = "sim_engine"
    binance_trade_base_url = os.environ.get("VELOZ_BINANCE_TRADE_BASE_URL", "https://testnet.binance.vision").strip() or "https://testnet.binance.vision"
    binance_api_key = os.environ.get("VELOZ_BINANCE_API_KEY", "")
    binance_api_secret = os.environ.get("VELOZ_BINANCE_API_SECRET", "")
    engine_path = os.path.join(
        repo_root, "build", preset, "apps", "engine", "veloz_engine"
    )
    if not os.path.exists(engine_path):
        raise SystemExit(f"engine not found: {engine_path}")

    order_store = OrderStore()
    bridge = EngineBridge([engine_path, "--stdio"], market_source, market_symbol, binance_base_url, order_store)
    binance_client = BinanceSpotRestClient(binance_trade_base_url, binance_api_key, binance_api_secret)
    router = ExecutionRouter(bridge=bridge, execution_mode=execution_mode, binance_client=binance_client, order_store=order_store)

    host = os.environ.get("VELOZ_HOST", "0.0.0.0")
    port = int(os.environ.get("VELOZ_PORT", "8080"))

    httpd = ThreadingHTTPServer((host, port), Handler)
    httpd.bridge = bridge
    httpd.router = router

    try:
        httpd.serve_forever()
    finally:
        bridge.stop()


if __name__ == "__main__":
    main()
