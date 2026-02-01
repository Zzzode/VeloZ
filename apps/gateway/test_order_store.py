import unittest
import runpy


class TestOrderStore(unittest.TestCase):
    def setUp(self):
        ns = runpy.run_path("/home/zzzode/Develop/VeloZ/apps/gateway/gateway.py")
        self.OrderStore = ns["OrderStore"]

    def test_apply_order_update_and_fill(self):
        store = self.OrderStore()
        store.apply_event(
            {
                "type": "order_update",
                "ts_ns": 1,
                "client_order_id": "c1",
                "symbol": "BTCUSDT",
                "status": "ACCEPTED",
                "venue_order_id": "v1",
            }
        )
        store.apply_event({"type": "fill", "ts_ns": 2, "client_order_id": "c1", "symbol": "BTCUSDT", "qty": 0.1, "price": 100.0})

        st = store.get("c1")
        self.assertEqual(st["client_order_id"], "c1")
        self.assertEqual(st["symbol"], "BTCUSDT")
        self.assertEqual(st["venue_order_id"], "v1")
        self.assertEqual(st["status"], "ACCEPTED")
        self.assertAlmostEqual(st["executed_qty"], 0.1)
        self.assertAlmostEqual(st["avg_price"], 100.0)
        self.assertEqual(st["last_ts_ns"], 2)


if __name__ == "__main__":
    unittest.main()

