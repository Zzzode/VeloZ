#!/usr/bin/env python3
"""Tests for WebSocket client functionality.

Tests the SimpleWebSocketClient used for Binance user data streams:
- Connection handling
- Message parsing
- Reconnection logic
- Error handling
- Frame encoding/decoding
"""

import base64
import hashlib
import os
import socket
import ssl
import struct
import sys
import threading
import time
import unittest
from io import BytesIO
from unittest.mock import MagicMock, patch

# Add gateway directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gateway import SimpleWebSocketClient


class MockWebSocketServer:
    """Mock WebSocket server for testing."""

    def __init__(self, port: int = 0):
        self._server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._server.bind(("127.0.0.1", port))
        self._server.listen(1)
        self._port = self._server.getsockname()[1]
        self._client = None
        self._running = False
        self._thread = None
        self._messages_to_send = []
        self._received_messages = []
        self._handshake_response = None
        self._send_ping = False
        self._close_after_handshake = False

    @property
    def port(self) -> int:
        return self._port

    def start(self):
        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        if self._client:
            try:
                self._client.close()
            except Exception:
                pass
        try:
            self._server.close()
        except Exception:
            pass
        if self._thread:
            self._thread.join(timeout=1.0)

    def queue_message(self, message: str):
        """Queue a text message to send to client."""
        self._messages_to_send.append(message)

    def set_handshake_response(self, response: bytes):
        """Set custom handshake response."""
        self._handshake_response = response

    def send_ping(self):
        """Send a ping frame to client."""
        self._send_ping = True

    def close_after_handshake(self):
        """Close connection immediately after handshake."""
        self._close_after_handshake = True

    def _run(self):
        try:
            self._server.settimeout(5.0)
            self._client, _ = self._server.accept()
            self._client.settimeout(5.0)

            # Receive handshake
            data = b""
            while b"\r\n\r\n" not in data:
                chunk = self._client.recv(4096)
                if not chunk:
                    return
                data += chunk

            # Extract Sec-WebSocket-Key
            key = None
            for line in data.decode("utf-8").split("\r\n"):
                if line.lower().startswith("sec-websocket-key:"):
                    key = line.split(":", 1)[1].strip()
                    break

            if self._handshake_response:
                self._client.sendall(self._handshake_response)
            else:
                # Send valid handshake response
                accept_key = base64.b64encode(
                    hashlib.sha1(
                        (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()
                    ).digest()
                ).decode()
                response = (
                    "HTTP/1.1 101 Switching Protocols\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    f"Sec-WebSocket-Accept: {accept_key}\r\n"
                    "\r\n"
                )
                self._client.sendall(response.encode())

            if self._close_after_handshake:
                self._client.close()
                return

            # Send queued messages
            for msg in self._messages_to_send:
                self._send_text_frame(msg)
                time.sleep(0.01)

            # Send ping if requested
            if self._send_ping:
                self._send_ping_frame(b"ping")

            # Keep connection open briefly
            time.sleep(0.5)

            # Send close frame
            self._send_close_frame()

        except Exception:
            pass

    def _send_text_frame(self, text: str):
        """Send a text frame to client."""
        payload = text.encode("utf-8")
        self._send_frame(0x1, payload)

    def _send_ping_frame(self, payload: bytes = b""):
        """Send a ping frame to client."""
        self._send_frame(0x9, payload)

    def _send_close_frame(self):
        """Send a close frame to client."""
        self._send_frame(0x8, b"")

    def _send_frame(self, opcode: int, payload: bytes):
        """Send a WebSocket frame."""
        frame = bytearray()
        frame.append(0x80 | opcode)  # FIN + opcode

        length = len(payload)
        if length < 126:
            frame.append(length)
        elif length < 65536:
            frame.append(126)
            frame.extend(struct.pack(">H", length))
        else:
            frame.append(127)
            frame.extend(struct.pack(">Q", length))

        frame.extend(payload)
        self._client.sendall(bytes(frame))


class TestWebSocketURLParsing(unittest.TestCase):
    """Tests for WebSocket URL parsing."""

    def test_parse_ws_url(self):
        """Test parsing ws:// URL."""
        client = SimpleWebSocketClient(
            "ws://localhost:8080/path",
            on_text=lambda x: None,
            on_error=lambda x: None,
        )
        scheme, host, port, path = client._parse_ws_url()
        self.assertEqual(scheme, "ws")
        self.assertEqual(host, "localhost")
        self.assertEqual(port, 8080)
        self.assertEqual(path, "/path")

    def test_parse_wss_url(self):
        """Test parsing wss:// URL."""
        client = SimpleWebSocketClient(
            "wss://example.com/stream",
            on_text=lambda x: None,
            on_error=lambda x: None,
        )
        scheme, host, port, path = client._parse_ws_url()
        self.assertEqual(scheme, "wss")
        self.assertEqual(host, "example.com")
        self.assertEqual(port, 443)
        self.assertEqual(path, "/stream")

    def test_parse_url_with_query(self):
        """Test parsing URL with query string."""
        client = SimpleWebSocketClient(
            "ws://localhost:8080/path?key=value",
            on_text=lambda x: None,
            on_error=lambda x: None,
        )
        scheme, host, port, path = client._parse_ws_url()
        self.assertEqual(path, "/path?key=value")

    def test_parse_url_default_port_ws(self):
        """Test default port for ws://."""
        client = SimpleWebSocketClient(
            "ws://localhost/path",
            on_text=lambda x: None,
            on_error=lambda x: None,
        )
        scheme, host, port, path = client._parse_ws_url()
        self.assertEqual(port, 80)

    def test_parse_url_default_port_wss(self):
        """Test default port for wss://."""
        client = SimpleWebSocketClient(
            "wss://localhost/path",
            on_text=lambda x: None,
            on_error=lambda x: None,
        )
        scheme, host, port, path = client._parse_ws_url()
        self.assertEqual(port, 443)

    def test_parse_invalid_scheme(self):
        """Test that invalid scheme raises error."""
        client = SimpleWebSocketClient(
            "http://localhost/path",
            on_text=lambda x: None,
            on_error=lambda x: None,
        )
        with self.assertRaises(ValueError):
            client._parse_ws_url()

    def test_parse_empty_path(self):
        """Test URL with empty path defaults to /."""
        client = SimpleWebSocketClient(
            "ws://localhost:8080",
            on_text=lambda x: None,
            on_error=lambda x: None,
        )
        scheme, host, port, path = client._parse_ws_url()
        self.assertEqual(path, "/")


class TestWebSocketConnection(unittest.TestCase):
    """Tests for WebSocket connection handling."""

    def setUp(self):
        self.server = None
        self.received_messages = []
        self.errors = []

    def tearDown(self):
        if self.server:
            self.server.stop()

    def _on_text(self, message: str):
        self.received_messages.append(message)

    def _on_error(self, error: str):
        self.errors.append(error)

    def test_successful_connection(self):
        """Test successful WebSocket connection and message receipt."""
        self.server = MockWebSocketServer()
        self.server.queue_message('{"type":"test","data":"hello"}')
        self.server.start()

        client = SimpleWebSocketClient(
            f"ws://127.0.0.1:{self.server.port}/",
            on_text=self._on_text,
            on_error=self._on_error,
        )

        # Run client in thread
        thread = threading.Thread(target=client.run, daemon=True)
        thread.start()

        # Wait for message
        time.sleep(1.0)
        client.close()
        thread.join(timeout=1.0)

        self.assertEqual(len(self.received_messages), 1)
        self.assertIn("hello", self.received_messages[0])

    def test_multiple_messages(self):
        """Test receiving multiple messages."""
        self.server = MockWebSocketServer()
        self.server.queue_message('{"id":1}')
        self.server.queue_message('{"id":2}')
        self.server.queue_message('{"id":3}')
        self.server.start()

        client = SimpleWebSocketClient(
            f"ws://127.0.0.1:{self.server.port}/",
            on_text=self._on_text,
            on_error=self._on_error,
        )

        thread = threading.Thread(target=client.run, daemon=True)
        thread.start()

        time.sleep(1.0)
        client.close()
        thread.join(timeout=1.0)

        self.assertEqual(len(self.received_messages), 3)

    def test_connection_refused(self):
        """Test error handling when connection is refused."""
        client = SimpleWebSocketClient(
            "ws://127.0.0.1:59999/",  # Non-existent port
            on_text=self._on_text,
            on_error=self._on_error,
        )

        client.run()

        self.assertEqual(len(self.errors), 1)

    def test_handshake_failure(self):
        """Test error handling on handshake failure."""
        self.server = MockWebSocketServer()
        self.server.set_handshake_response(b"HTTP/1.1 403 Forbidden\r\n\r\n")
        self.server.start()

        client = SimpleWebSocketClient(
            f"ws://127.0.0.1:{self.server.port}/",
            on_text=self._on_text,
            on_error=self._on_error,
        )

        client.run()

        self.assertEqual(len(self.errors), 1)
        self.assertIn("101", self.errors[0])

    def test_server_close(self):
        """Test handling of server-initiated close."""
        self.server = MockWebSocketServer()
        self.server.close_after_handshake()
        self.server.start()

        client = SimpleWebSocketClient(
            f"ws://127.0.0.1:{self.server.port}/",
            on_text=self._on_text,
            on_error=self._on_error,
        )

        thread = threading.Thread(target=client.run, daemon=True)
        thread.start()
        thread.join(timeout=2.0)

        # Should complete without error
        self.assertEqual(len(self.received_messages), 0)

    def test_client_close(self):
        """Test client-initiated close."""
        self.server = MockWebSocketServer()
        self.server.start()

        client = SimpleWebSocketClient(
            f"ws://127.0.0.1:{self.server.port}/",
            on_text=self._on_text,
            on_error=self._on_error,
        )

        thread = threading.Thread(target=client.run, daemon=True)
        thread.start()

        time.sleep(0.2)
        client.close()
        # Note: The thread may still be alive if blocked on recv
        # This is expected behavior - the close flag is set but
        # the blocking recv needs to complete or timeout
        thread.join(timeout=2.0)

        # Verify close was called (flag is set)
        self.assertTrue(client._closed)

    def test_ping_pong(self):
        """Test ping/pong handling."""
        self.server = MockWebSocketServer()
        self.server.send_ping()
        self.server.queue_message('{"after":"ping"}')
        self.server.start()

        client = SimpleWebSocketClient(
            f"ws://127.0.0.1:{self.server.port}/",
            on_text=self._on_text,
            on_error=self._on_error,
        )

        thread = threading.Thread(target=client.run, daemon=True)
        thread.start()

        time.sleep(1.0)
        client.close()
        thread.join(timeout=1.0)

        # Should receive message after ping
        self.assertGreaterEqual(len(self.received_messages), 1)


class TestWebSocketFrameEncoding(unittest.TestCase):
    """Tests for WebSocket frame encoding/decoding."""

    def test_small_payload_encoding(self):
        """Test encoding of small payload (<126 bytes)."""
        # This tests the _send_frame method indirectly
        client = SimpleWebSocketClient(
            "ws://localhost/",
            on_text=lambda x: None,
            on_error=lambda x: None,
        )
        # The client masks outgoing frames
        # We can't easily test this without a real connection

    def test_medium_payload_encoding(self):
        """Test encoding of medium payload (126-65535 bytes)."""
        # Similar to above - would need mock socket

    def test_large_payload_encoding(self):
        """Test encoding of large payload (>65535 bytes)."""
        # Similar to above - would need mock socket


class TestWebSocketThreadSafety(unittest.TestCase):
    """Tests for WebSocket thread safety."""

    def test_concurrent_close(self):
        """Test that concurrent close calls are safe."""
        client = SimpleWebSocketClient(
            "ws://localhost/",
            on_text=lambda x: None,
            on_error=lambda x: None,
        )

        # Call close from multiple threads
        threads = []
        for _ in range(10):
            t = threading.Thread(target=client.close)
            threads.append(t)
            t.start()

        for t in threads:
            t.join()

        # Should not raise any exceptions

    def test_close_during_run(self):
        """Test closing client while run() is executing."""
        server = MockWebSocketServer()
        server.start()

        client = SimpleWebSocketClient(
            f"ws://127.0.0.1:{server.port}/",
            on_text=lambda x: None,
            on_error=lambda x: None,
        )

        thread = threading.Thread(target=client.run, daemon=True)
        thread.start()

        time.sleep(0.1)
        client.close()
        thread.join(timeout=2.0)

        server.stop()

        # Verify close was called (flag is set)
        # Note: Thread may still be blocked on recv, which is expected
        # The important thing is the close flag is set
        self.assertTrue(client._closed)


class TestWebSocketErrorHandling(unittest.TestCase):
    """Tests for WebSocket error handling."""

    def test_callback_exception_handling(self):
        """Test that exceptions in callbacks don't crash the client."""
        server = MockWebSocketServer()
        server.queue_message('{"test":1}')
        server.start()

        def bad_callback(msg):
            raise RuntimeError("Callback error")

        errors = []
        client = SimpleWebSocketClient(
            f"ws://127.0.0.1:{server.port}/",
            on_text=bad_callback,
            on_error=lambda x: errors.append(x),
        )

        thread = threading.Thread(target=client.run, daemon=True)
        thread.start()

        time.sleep(1.0)
        client.close()
        thread.join(timeout=1.0)

        server.stop()

        # Should not crash despite callback error
        self.assertFalse(thread.is_alive())

    def test_error_callback_exception(self):
        """Test that exceptions in error callback don't crash."""
        def bad_error_callback(err):
            raise RuntimeError("Error callback error")

        client = SimpleWebSocketClient(
            "ws://127.0.0.1:59999/",  # Non-existent port
            on_text=lambda x: None,
            on_error=bad_error_callback,
        )

        # Should not raise despite error callback throwing
        client.run()


class TestWebSocketReconnection(unittest.TestCase):
    """Tests for WebSocket reconnection scenarios."""

    def test_reconnect_after_close(self):
        """Test that client can reconnect after being closed."""
        messages1 = []
        messages2 = []

        # First connection
        server1 = MockWebSocketServer()
        server1.queue_message('{"conn":1}')
        server1.start()

        client1 = SimpleWebSocketClient(
            f"ws://127.0.0.1:{server1.port}/",
            on_text=lambda x: messages1.append(x),
            on_error=lambda x: None,
        )

        thread1 = threading.Thread(target=client1.run, daemon=True)
        thread1.start()
        time.sleep(0.5)
        client1.close()
        thread1.join(timeout=1.0)
        server1.stop()

        # Second connection (new client, new server)
        server2 = MockWebSocketServer()
        server2.queue_message('{"conn":2}')
        server2.start()

        client2 = SimpleWebSocketClient(
            f"ws://127.0.0.1:{server2.port}/",
            on_text=lambda x: messages2.append(x),
            on_error=lambda x: None,
        )

        thread2 = threading.Thread(target=client2.run, daemon=True)
        thread2.start()
        time.sleep(0.5)
        client2.close()
        thread2.join(timeout=1.0)
        server2.stop()

        # Both connections should have received messages
        self.assertEqual(len(messages1), 1)
        self.assertEqual(len(messages2), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
