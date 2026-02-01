import hmac
import hashlib
import runpy
import unittest


class TestBinanceSigning(unittest.TestCase):
    def test_sign_matches_expected_hmac(self):
        ns = runpy.run_path("/home/zzzode/Develop/VeloZ/apps/gateway/gateway.py")
        Client = ns["BinanceSpotRestClient"]

        client = Client("https://example.invalid", "k", "secret")
        query = "recvWindow=5000&side=BUY&symbol=BTCUSDT&timestamp=123"
        expected = hmac.new(b"secret", query.encode("utf-8"), hashlib.sha256).hexdigest()
        self.assertEqual(client._sign(query), expected)


if __name__ == "__main__":
    unittest.main()

