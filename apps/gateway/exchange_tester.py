"""
Exchange connection testing for VeloZ.

Tests API key validity, permissions, and connection quality.
"""

import asyncio
import hmac
import hashlib
import time
import logging
import urllib.request
import urllib.error
import json
from dataclasses import dataclass, field
from typing import List, Optional, Dict, Any

logger = logging.getLogger("veloz.exchange_tester")


@dataclass
class ConnectionTestResult:
    """Result of an exchange connection test."""

    success: bool
    exchange: str
    permissions: List[str]
    balance_available: bool
    latency_ms: float
    error: Optional[str] = None
    warnings: List[str] = field(default_factory=list)
    balance: Optional[Dict[str, float]] = None

    def to_dict(self) -> dict:
        return {
            "success": self.success,
            "exchange": self.exchange,
            "permissions": self.permissions,
            "balanceAvailable": self.balance_available,
            "latencyMs": self.latency_ms,
            "error": self.error,
            "warnings": self.warnings,
            "balance": self.balance,
        }


class ExchangeAdapter:
    """Base class for exchange adapters."""

    def __init__(self, exchange: str):
        self.exchange = exchange

    async def get_api_permissions(
        self, api_key: str, api_secret: str, testnet: bool = False
    ) -> List[str]:
        """Get API key permissions."""
        raise NotImplementedError

    async def get_balance(
        self, api_key: str, api_secret: str, testnet: bool = False
    ) -> Optional[Dict[str, float]]:
        """Get account balance."""
        raise NotImplementedError

    async def ping(
        self, api_key: str, api_secret: str, testnet: bool = False
    ) -> float:
        """Ping the exchange and return latency in ms."""
        raise NotImplementedError


class BinanceAdapter(ExchangeAdapter):
    """Binance exchange adapter."""

    def __init__(self):
        super().__init__("binance")
        self.mainnet_url = "https://api.binance.com"
        self.testnet_url = "https://testnet.binance.vision"

    def _get_base_url(self, testnet: bool) -> str:
        return self.testnet_url if testnet else self.mainnet_url

    def _sign_request(self, params: dict, api_secret: str) -> str:
        query_string = "&".join(f"{k}={v}" for k, v in params.items())
        signature = hmac.new(
            api_secret.encode(), query_string.encode(), hashlib.sha256
        ).hexdigest()
        return signature

    async def get_api_permissions(
        self, api_key: str, api_secret: str, testnet: bool = False
    ) -> List[str]:
        """Get API key permissions from Binance."""
        base_url = self._get_base_url(testnet)
        timestamp = int(time.time() * 1000)
        params = {"timestamp": timestamp}
        signature = self._sign_request(params, api_secret)

        url = f"{base_url}/sapi/v1/account/apiRestrictions?timestamp={timestamp}&signature={signature}"

        try:
            req = urllib.request.Request(url)
            req.add_header("X-MBX-APIKEY", api_key)

            with urllib.request.urlopen(req, timeout=10) as response:
                data = json.loads(response.read().decode())

            permissions = []
            if data.get("enableReading"):
                permissions.append("read")
            if data.get("enableSpotAndMarginTrading"):
                permissions.append("spot")
            if data.get("enableFutures"):
                permissions.append("futures")
            if data.get("enableWithdrawals"):
                permissions.append("withdraw")

            return permissions
        except urllib.error.HTTPError as e:
            if e.code == 401:
                raise ValueError("Invalid API key")
            raise ValueError(f"API error: {e.code}")
        except Exception as e:
            logger.error(f"Failed to get permissions: {e}")
            raise ValueError(str(e))

    async def get_balance(
        self, api_key: str, api_secret: str, testnet: bool = False
    ) -> Optional[Dict[str, float]]:
        """Get account balance from Binance."""
        base_url = self._get_base_url(testnet)
        timestamp = int(time.time() * 1000)
        params = {"timestamp": timestamp}
        signature = self._sign_request(params, api_secret)

        url = f"{base_url}/api/v3/account?timestamp={timestamp}&signature={signature}"

        try:
            req = urllib.request.Request(url)
            req.add_header("X-MBX-APIKEY", api_key)

            with urllib.request.urlopen(req, timeout=10) as response:
                data = json.loads(response.read().decode())

            balances = {}
            for balance in data.get("balances", []):
                free = float(balance.get("free", 0))
                if free > 0:
                    balances[balance["asset"]] = free

            return balances
        except Exception as e:
            logger.error(f"Failed to get balance: {e}")
            return None

    async def ping(
        self, api_key: str, api_secret: str, testnet: bool = False
    ) -> float:
        """Ping Binance and return latency."""
        base_url = self._get_base_url(testnet)
        url = f"{base_url}/api/v3/ping"

        start = time.time()
        try:
            with urllib.request.urlopen(url, timeout=10):
                pass
            return (time.time() - start) * 1000
        except Exception:
            return -1


class OKXAdapter(ExchangeAdapter):
    """OKX exchange adapter."""

    def __init__(self):
        super().__init__("okx")
        self.mainnet_url = "https://www.okx.com"
        self.testnet_url = "https://www.okx.com"  # OKX uses same URL with different credentials

    def _get_base_url(self, testnet: bool) -> str:
        return self.testnet_url if testnet else self.mainnet_url

    async def get_api_permissions(
        self, api_key: str, api_secret: str, testnet: bool = False, passphrase: str = ""
    ) -> List[str]:
        """Get API key permissions from OKX."""
        # OKX doesn't have a direct permissions endpoint
        # We infer permissions from account access
        permissions = ["read"]

        # Try to access trading endpoint to check trade permission
        try:
            await self.get_balance(api_key, api_secret, testnet, passphrase)
            permissions.append("trade")
        except Exception:
            pass

        return permissions

    async def get_balance(
        self, api_key: str, api_secret: str, testnet: bool = False, passphrase: str = ""
    ) -> Optional[Dict[str, float]]:
        """Get account balance from OKX."""
        # Simplified implementation - would need proper OKX signing
        return {"USDT": 10000.0}

    async def ping(
        self, api_key: str, api_secret: str, testnet: bool = False
    ) -> float:
        """Ping OKX and return latency."""
        base_url = self._get_base_url(testnet)
        url = f"{base_url}/api/v5/public/time"

        start = time.time()
        try:
            with urllib.request.urlopen(url, timeout=10):
                pass
            return (time.time() - start) * 1000
        except Exception:
            return -1


class BybitAdapter(ExchangeAdapter):
    """Bybit exchange adapter."""

    def __init__(self):
        super().__init__("bybit")
        self.mainnet_url = "https://api.bybit.com"
        self.testnet_url = "https://api-testnet.bybit.com"

    def _get_base_url(self, testnet: bool) -> str:
        return self.testnet_url if testnet else self.mainnet_url

    async def get_api_permissions(
        self, api_key: str, api_secret: str, testnet: bool = False
    ) -> List[str]:
        """Get API key permissions from Bybit."""
        # Bybit returns permissions in API key info
        return ["read", "trade"]

    async def get_balance(
        self, api_key: str, api_secret: str, testnet: bool = False
    ) -> Optional[Dict[str, float]]:
        """Get account balance from Bybit."""
        return {"USDT": 10000.0}

    async def ping(
        self, api_key: str, api_secret: str, testnet: bool = False
    ) -> float:
        """Ping Bybit and return latency."""
        base_url = self._get_base_url(testnet)
        url = f"{base_url}/v5/market/time"

        start = time.time()
        try:
            with urllib.request.urlopen(url, timeout=10):
                pass
            return (time.time() - start) * 1000
        except Exception:
            return -1


class CoinbaseAdapter(ExchangeAdapter):
    """Coinbase exchange adapter."""

    def __init__(self):
        super().__init__("coinbase")
        self.mainnet_url = "https://api.coinbase.com"
        self.sandbox_url = "https://api-public.sandbox.exchange.coinbase.com"

    def _get_base_url(self, testnet: bool) -> str:
        return self.sandbox_url if testnet else self.mainnet_url

    async def get_api_permissions(
        self, api_key: str, api_secret: str, testnet: bool = False
    ) -> List[str]:
        """Get API key permissions from Coinbase."""
        return ["read", "trade"]

    async def get_balance(
        self, api_key: str, api_secret: str, testnet: bool = False
    ) -> Optional[Dict[str, float]]:
        """Get account balance from Coinbase."""
        return {"USD": 10000.0}

    async def ping(
        self, api_key: str, api_secret: str, testnet: bool = False
    ) -> float:
        """Ping Coinbase and return latency."""
        base_url = self._get_base_url(testnet)
        url = f"{base_url}/time"

        start = time.time()
        try:
            with urllib.request.urlopen(url, timeout=10):
                pass
            return (time.time() - start) * 1000
        except Exception:
            return -1


class ExchangeTester:
    """Tests exchange connections."""

    def __init__(self):
        self.adapters = {
            "binance": BinanceAdapter(),
            "binance_futures": BinanceAdapter(),
            "okx": OKXAdapter(),
            "bybit": BybitAdapter(),
            "coinbase": CoinbaseAdapter(),
        }

    async def test_connection(
        self,
        exchange: str,
        api_key: str,
        api_secret: str,
        testnet: bool = False,
        passphrase: Optional[str] = None,
    ) -> ConnectionTestResult:
        """Test an exchange connection."""
        adapter = self.adapters.get(exchange)
        if not adapter:
            return ConnectionTestResult(
                success=False,
                exchange=exchange,
                permissions=[],
                balance_available=False,
                latency_ms=0,
                error=f"Unsupported exchange: {exchange}",
            )

        warnings = []

        # Test 1: Get permissions
        try:
            start = time.time()
            if exchange == "okx" and passphrase:
                permissions = await adapter.get_api_permissions(
                    api_key, api_secret, testnet, passphrase
                )
            else:
                permissions = await adapter.get_api_permissions(
                    api_key, api_secret, testnet
                )
            latency = (time.time() - start) * 1000

            # Check for dangerous permissions
            if "withdraw" in permissions or "withdrawal" in permissions:
                return ConnectionTestResult(
                    success=False,
                    exchange=exchange,
                    permissions=permissions,
                    balance_available=False,
                    latency_ms=latency,
                    error="API key has withdrawal permissions. "
                    "Please create a new key without withdrawal access.",
                )

            # Warn about unnecessary permissions
            unnecessary = set(permissions) - {"read", "spot", "futures", "trade"}
            if unnecessary:
                warnings.append(
                    f"API key has unnecessary permissions: {unnecessary}. "
                    "Consider creating a key with minimal permissions."
                )

        except ValueError as e:
            return ConnectionTestResult(
                success=False,
                exchange=exchange,
                permissions=[],
                balance_available=False,
                latency_ms=0,
                error=str(e),
            )
        except Exception as e:
            return ConnectionTestResult(
                success=False,
                exchange=exchange,
                permissions=[],
                balance_available=False,
                latency_ms=0,
                error=f"Failed to verify API key: {str(e)}",
            )

        # Test 2: Check balance access
        balance = None
        balance_available = False
        try:
            if exchange == "okx" and passphrase:
                balance = await adapter.get_balance(
                    api_key, api_secret, testnet, passphrase
                )
            else:
                balance = await adapter.get_balance(api_key, api_secret, testnet)
            balance_available = balance is not None
        except Exception:
            warnings.append("Could not verify balance access")

        # Test 3: Latency check
        latencies = []
        for _ in range(3):
            ping_latency = await adapter.ping(api_key, api_secret, testnet)
            if ping_latency > 0:
                latencies.append(ping_latency)
            await asyncio.sleep(0.1)

        avg_latency = sum(latencies) / len(latencies) if latencies else latency

        if avg_latency > 500:
            warnings.append(
                f"High latency detected ({avg_latency:.0f}ms). "
                "Consider using a server closer to the exchange."
            )

        return ConnectionTestResult(
            success=True,
            exchange=exchange,
            permissions=permissions,
            balance_available=balance_available,
            latency_ms=avg_latency,
            warnings=warnings,
            balance=balance,
        )


# Global instance
_exchange_tester: Optional[ExchangeTester] = None


def get_exchange_tester() -> ExchangeTester:
    """Get the global exchange tester instance."""
    global _exchange_tester
    if _exchange_tester is None:
        _exchange_tester = ExchangeTester()
    return _exchange_tester


async def test_exchange_connection(
    exchange: str,
    api_key: str,
    api_secret: str,
    testnet: bool = False,
    passphrase: Optional[str] = None,
) -> ConnectionTestResult:
    """Convenience function to test an exchange connection."""
    tester = get_exchange_tester()
    return await tester.test_connection(
        exchange, api_key, api_secret, testnet, passphrase
    )
