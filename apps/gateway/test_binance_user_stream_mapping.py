import unittest
import runpy
import os


class FakeBridge:
    def __init__(self):
        self.events = []

    def emit_event(self, evt):
        self.events.append(evt)

    def config(self):
        return {"market_symbol": "BTCUSDT"}


class TestBinanceUserStreamMapping(unittest.TestCase):
    def setUp(self):
        gateway_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "gateway.py")
        self.ns = runpy.run_path(gateway_path)

    def test_execution_report_maps_to_update_and_fill(self):
        OrderStore = self.ns["OrderStore"]
        ExecutionRouter = self.ns["ExecutionRouter"]

        bridge = FakeBridge()
        store = OrderStore()
        router = ExecutionRouter(bridge=bridge, execution_mode="sim_engine", binance_client=None, order_store=store)

        router._handle_binance_execution_report(
            {
                "e": "executionReport",
                "E": 123,
                "s": "BTCUSDT",
                "S": "BUY",
                "c": "cid-1",
                "i": 999,
                "X": "FILLED",
                "q": "0.5",
                "p": "100.0",
                "l": "0.5",
                "L": "100.0",
            }
        )

        types = [e.get("type") for e in bridge.events]
        self.assertIn("order_update", types)
        self.assertIn("fill", types)

        st = store.get("cid-1")
        self.assertEqual(st["status"], "FILLED")
        self.assertAlmostEqual(st["order_qty"], 0.5)
        self.assertAlmostEqual(st["executed_qty"], 0.5)

    def test_account_update_maps_to_balances(self):
        OrderStore = self.ns["OrderStore"]
        ExecutionRouter = self.ns["ExecutionRouter"]

        bridge = FakeBridge()
        store = OrderStore()
        router = ExecutionRouter(bridge=bridge, execution_mode="sim_engine", binance_client=None, order_store=store)

        router._handle_binance_account_update(
            {
                "e": "outboundAccountPosition",
                "E": 456,
                "B": [
                    {"a": "USDT", "f": "10.0", "l": "1.5"},
                    {"a": "BTC", "f": "0.1", "l": "0.0"},
                ],
            }
        )

        snap = router.account_state()
        self.assertIsNotNone(snap.get("ts_ns"))
        assets = {b["asset"]: (b["free"], b["locked"]) for b in snap["balances"]}
        self.assertEqual(assets["USDT"], (10.0, 1.5))
        self.assertEqual(assets["BTC"], (0.1, 0.0))


if __name__ == "__main__":
    unittest.main()

