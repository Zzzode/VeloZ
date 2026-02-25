"""
Strategy Template Service

Manages loading, caching, and querying of strategy templates.
"""

import json
import logging
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional

import yaml

from .validation_service import ValidationService, ValidationResult

logger = logging.getLogger(__name__)


@dataclass
class StrategyTemplate:
    """Represents a loaded strategy template."""

    id: str
    name: str
    version: str
    category: str
    strategy_type: str
    difficulty: str = "intermediate"
    risk_level: str = "medium"
    description: dict = field(default_factory=dict)
    parameters: list = field(default_factory=list)
    presets: dict = field(default_factory=dict)
    risks: list = field(default_factory=list)
    supported_symbols: list = field(default_factory=list)
    supported_exchanges: list = field(default_factory=list)
    min_capital: float = 0.0
    tags: list = field(default_factory=list)
    author: str = "VeloZ Team"
    created_at: str = ""
    updated_at: str = ""
    _raw: dict = field(default_factory=dict, repr=False)

    @classmethod
    def from_dict(cls, data: dict) -> "StrategyTemplate":
        """Create a StrategyTemplate from a dictionary."""
        return cls(
            id=data["id"],
            name=data["name"],
            version=data["version"],
            category=data["category"],
            strategy_type=data["strategy_type"],
            difficulty=data.get("difficulty", "intermediate"),
            risk_level=data.get("risk_level", "medium"),
            description=data.get("description", {}),
            parameters=data.get("parameters", []),
            presets=data.get("presets", {}),
            risks=data.get("risks", []),
            supported_symbols=data.get("supported_symbols", []),
            supported_exchanges=data.get("supported_exchanges", []),
            min_capital=data.get("min_capital", 0.0),
            tags=data.get("tags", []),
            author=data.get("author", "VeloZ Team"),
            created_at=data.get("created_at", ""),
            updated_at=data.get("updated_at", ""),
            _raw=data,
        )

    def to_dict(self) -> dict:
        """Convert to dictionary for API responses."""
        return {
            "id": self.id,
            "name": self.name,
            "version": self.version,
            "category": self.category,
            "strategy_type": self.strategy_type,
            "difficulty": self.difficulty,
            "risk_level": self.risk_level,
            "description": self.description,
            "parameters": self.parameters,
            "presets": self.presets,
            "risks": self.risks,
            "supported_symbols": self.supported_symbols,
            "supported_exchanges": self.supported_exchanges,
            "min_capital": self.min_capital,
            "tags": self.tags,
            "author": self.author,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
        }

    def to_summary(self) -> dict:
        """Return a summary for list views."""
        return {
            "id": self.id,
            "name": self.name,
            "category": self.category,
            "difficulty": self.difficulty,
            "risk_level": self.risk_level,
            "description_short": self.description.get("short", ""),
            "tags": self.tags,
            "min_capital": self.min_capital,
        }

    def get_preset(self, preset_name: str) -> Optional[dict]:
        """Get parameter values for a preset."""
        preset = self.presets.get(preset_name)
        if preset:
            return preset.get("values", {})
        return None

    def get_default_values(self) -> dict:
        """Get default values for all parameters."""
        return {p["name"]: p.get("default") for p in self.parameters if "default" in p}


class TemplateService:
    """
    Service for managing strategy templates.

    Responsibilities:
    - Load templates from YAML files
    - Validate templates against schema
    - Cache templates for performance
    - Provide query/filter capabilities
    """

    def __init__(self, templates_dir: Optional[Path] = None):
        self._templates_dir = templates_dir or (
            Path(__file__).parent.parent / "templates"
        )
        self._templates: dict[str, StrategyTemplate] = {}
        self._validation_service = ValidationService()
        self._loaded = False

    def load_templates(self, force_reload: bool = False) -> int:
        """
        Load all templates from the templates directory.

        Args:
            force_reload: If True, reload even if already loaded

        Returns:
            Number of templates loaded
        """
        if self._loaded and not force_reload:
            return len(self._templates)

        self._templates.clear()
        loaded_count = 0

        if not self._templates_dir.exists():
            logger.warning(f"Templates directory not found: {self._templates_dir}")
            return 0

        for yaml_file in self._templates_dir.glob("*.yaml"):
            try:
                template = self._load_template_file(yaml_file)
                if template:
                    self._templates[template.id] = template
                    loaded_count += 1
                    logger.info(f"Loaded template: {template.id} ({template.name})")
            except Exception as e:
                logger.error(f"Failed to load template {yaml_file}: {e}")

        self._loaded = True
        logger.info(f"Loaded {loaded_count} strategy templates")
        return loaded_count

    def _load_template_file(self, file_path: Path) -> Optional[StrategyTemplate]:
        """Load and validate a single template file."""
        with open(file_path, "r") as f:
            data = yaml.safe_load(f)

        if not data:
            logger.warning(f"Empty template file: {file_path}")
            return None

        # Basic validation
        required_fields = [
            "id",
            "name",
            "version",
            "category",
            "strategy_type",
            "parameters",
        ]
        for field in required_fields:
            if field not in data:
                logger.error(f"Template {file_path} missing required field: {field}")
                return None

        return StrategyTemplate.from_dict(data)

    def get_template(self, template_id: str) -> Optional[StrategyTemplate]:
        """Get a template by ID."""
        self.load_templates()
        return self._templates.get(template_id)

    def list_templates(
        self,
        category: Optional[str] = None,
        difficulty: Optional[str] = None,
        risk_level: Optional[str] = None,
        search: Optional[str] = None,
        tags: Optional[list[str]] = None,
    ) -> list[StrategyTemplate]:
        """
        List templates with optional filtering.

        Args:
            category: Filter by category
            difficulty: Filter by difficulty level
            risk_level: Filter by risk level
            search: Search in name and description
            tags: Filter by tags (any match)

        Returns:
            List of matching templates
        """
        self.load_templates()
        results = list(self._templates.values())

        if category:
            results = [t for t in results if t.category == category]

        if difficulty:
            results = [t for t in results if t.difficulty == difficulty]

        if risk_level:
            results = [t for t in results if t.risk_level == risk_level]

        if search:
            search_lower = search.lower()
            results = [
                t
                for t in results
                if search_lower in t.name.lower()
                or search_lower in t.description.get("short", "").lower()
                or search_lower in t.description.get("long", "").lower()
            ]

        if tags:
            results = [t for t in results if any(tag in t.tags for tag in tags)]

        return results

    def get_categories(self) -> list[dict]:
        """Get list of categories with counts."""
        self.load_templates()
        category_counts: dict[str, int] = {}

        for template in self._templates.values():
            cat = template.category
            category_counts[cat] = category_counts.get(cat, 0) + 1

        return [
            {"id": cat, "name": cat.replace("_", " ").title(), "count": count}
            for cat, count in sorted(category_counts.items())
        ]

    def validate_configuration(
        self, template_id: str, parameters: dict[str, Any]
    ) -> ValidationResult:
        """
        Validate a strategy configuration.

        Args:
            template_id: Template ID
            parameters: User-provided parameter values

        Returns:
            ValidationResult with validation status
        """
        template = self.get_template(template_id)
        if not template:
            from .validation_service import ValidationError

            return ValidationResult(
                valid=False,
                errors=[
                    ValidationError(
                        parameter="template_id",
                        message=f"Template not found: {template_id}",
                        code="not_found",
                    )
                ],
            )

        return self._validation_service.validate_parameters(template._raw, parameters)

    def apply_preset(
        self, template_id: str, preset_name: str
    ) -> Optional[dict[str, Any]]:
        """
        Get parameter values for a preset.

        Args:
            template_id: Template ID
            preset_name: Preset name (e.g., "conservative", "moderate", "aggressive")

        Returns:
            Dictionary of parameter values, or None if not found
        """
        template = self.get_template(template_id)
        if not template:
            return None

        preset_values = template.get_preset(preset_name)
        if preset_values is None:
            return None

        # Merge with defaults
        defaults = template.get_default_values()
        return {**defaults, **preset_values}

    def get_parameter_schema(self, template_id: str) -> Optional[dict]:
        """
        Get JSON Schema for a template's parameters.

        Useful for frontend form generation.
        """
        template = self.get_template(template_id)
        if not template:
            return None

        return self._validation_service.get_parameter_schema(template._raw)

    def to_engine_config(
        self, template_id: str, parameters: dict[str, Any], symbol: str
    ) -> Optional[dict]:
        """
        Convert template configuration to engine-compatible format.

        Args:
            template_id: Template ID
            parameters: Validated parameter values
            symbol: Trading symbol

        Returns:
            Configuration dict for C++ engine
        """
        template = self.get_template(template_id)
        if not template:
            return None

        # Map strategy_type to C++ strategy name
        strategy_type_map = {
            "grid": "Grid",
            "dca": "DCA",
            "mean_reversion": "MeanReversion",
            "momentum": "Momentum",
            "trend_following": "TrendFollowing",
            "market_making": "MarketMaking",
            "rsi": "RSI",
            "macd": "MACD",
            "bollinger_bands": "BollingerBands",
            "stochastic": "StochasticOscillator",
            "arbitrage": "CrossExchangeArbitrage",
            "scalping": "MarketMakingHFT",
        }

        cpp_strategy_type = strategy_type_map.get(
            template.strategy_type, template.strategy_type
        )

        # Build engine config
        config = {
            "strategy_name": cpp_strategy_type,
            "symbol": symbol,
            "parameters": {},
        }

        # Map parameters to C++ parameter names
        for param in template.parameters:
            name = param["name"]
            if name in parameters:
                # Convert parameter name to C++ convention if needed
                cpp_name = self._to_cpp_param_name(name)
                config["parameters"][cpp_name] = parameters[name]

        return config

    def _to_cpp_param_name(self, name: str) -> str:
        """Convert parameter name to C++ convention."""
        # Most parameters use the same name
        # Add specific mappings if needed
        mappings = {
            "grid_count": "grid_count",
            "grid_levels": "grid_count",
            "grid_spacing_pct": "grid_spacing",
            "total_investment": "total_investment",
            "upper_price": "upper_price",
            "lower_price": "lower_price",
            "investment_amount": "investment_amount",
            "frequency": "frequency",
            "lookback_period": "lookback_period",
            "entry_threshold": "entry_threshold",
            "exit_threshold": "exit_threshold",
            "position_size": "position_size",
            "fast_period": "fast_period",
            "slow_period": "slow_period",
            "signal_period": "signal_period",
            "rsi_period": "rsi_period",
            "overbought_level": "overbought_level",
            "oversold_level": "oversold_level",
            "period": "period",
            "std_dev": "std_dev",
        }
        return mappings.get(name, name)


# Singleton instance
_template_service: Optional[TemplateService] = None


def get_template_service() -> TemplateService:
    """Get the singleton TemplateService instance."""
    global _template_service
    if _template_service is None:
        _template_service = TemplateService()
    return _template_service
