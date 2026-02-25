"""
Tests for the Template Service
"""

import pytest
import tempfile
from pathlib import Path

import yaml

from ..services.template_service import TemplateService, StrategyTemplate
from ..services.validation_service import ValidationService


class TestTemplateService:
    """Tests for TemplateService."""

    @pytest.fixture
    def temp_templates_dir(self):
        """Create a temporary directory with test templates."""
        with tempfile.TemporaryDirectory() as tmpdir:
            templates_path = Path(tmpdir)

            # Create a test template
            test_template = {
                "id": "test-strategy",
                "name": "Test Strategy",
                "version": "1.0.0",
                "category": "trend_following",
                "strategy_type": "trend_following",
                "difficulty": "beginner",
                "risk_level": "low",
                "description": {
                    "short": "A test strategy",
                    "long": "A longer description of the test strategy",
                },
                "parameters": [
                    {
                        "name": "period",
                        "type": "integer",
                        "description": "Lookback period",
                        "default": 14,
                        "min": 5,
                        "max": 100,
                    },
                    {
                        "name": "threshold",
                        "type": "float",
                        "description": "Entry threshold",
                        "default": 0.5,
                        "min": 0.1,
                        "max": 1.0,
                    },
                ],
                "presets": {
                    "conservative": {
                        "name": "Conservative",
                        "description": "Safe settings",
                        "values": {"period": 20, "threshold": 0.3},
                    },
                    "aggressive": {
                        "name": "Aggressive",
                        "description": "Risky settings",
                        "values": {"period": 10, "threshold": 0.7},
                    },
                },
                "risks": ["Risk 1", "Risk 2"],
                "tags": ["test", "example"],
            }

            with open(templates_path / "test_strategy.yaml", "w") as f:
                yaml.dump(test_template, f)

            yield templates_path

    @pytest.fixture
    def service(self, temp_templates_dir):
        """Create a TemplateService with test templates."""
        return TemplateService(templates_dir=temp_templates_dir)

    def test_load_templates(self, service):
        """Test loading templates from directory."""
        count = service.load_templates()
        assert count == 1

    def test_get_template(self, service):
        """Test getting a template by ID."""
        service.load_templates()
        template = service.get_template("test-strategy")

        assert template is not None
        assert template.id == "test-strategy"
        assert template.name == "Test Strategy"
        assert template.category == "trend_following"
        assert len(template.parameters) == 2

    def test_get_nonexistent_template(self, service):
        """Test getting a non-existent template."""
        service.load_templates()
        template = service.get_template("nonexistent")
        assert template is None

    def test_list_templates(self, service):
        """Test listing templates."""
        service.load_templates()
        templates = service.list_templates()

        assert len(templates) == 1
        assert templates[0].id == "test-strategy"

    def test_list_templates_with_filter(self, service):
        """Test filtering templates."""
        service.load_templates()

        # Filter by category
        templates = service.list_templates(category="trend_following")
        assert len(templates) == 1

        templates = service.list_templates(category="mean_reversion")
        assert len(templates) == 0

        # Filter by difficulty
        templates = service.list_templates(difficulty="beginner")
        assert len(templates) == 1

        templates = service.list_templates(difficulty="advanced")
        assert len(templates) == 0

    def test_list_templates_with_search(self, service):
        """Test searching templates."""
        service.load_templates()

        templates = service.list_templates(search="test")
        assert len(templates) == 1

        templates = service.list_templates(search="nonexistent")
        assert len(templates) == 0

    def test_list_templates_with_tags(self, service):
        """Test filtering by tags."""
        service.load_templates()

        templates = service.list_templates(tags=["test"])
        assert len(templates) == 1

        templates = service.list_templates(tags=["nonexistent"])
        assert len(templates) == 0

    def test_get_categories(self, service):
        """Test getting categories."""
        service.load_templates()
        categories = service.get_categories()

        assert len(categories) == 1
        assert categories[0]["id"] == "trend_following"
        assert categories[0]["count"] == 1

    def test_validate_configuration_valid(self, service):
        """Test validating valid configuration."""
        service.load_templates()

        result = service.validate_configuration(
            "test-strategy", {"period": 20, "threshold": 0.5}
        )

        assert result.valid
        assert len(result.errors) == 0

    def test_validate_configuration_invalid_type(self, service):
        """Test validating configuration with invalid type."""
        service.load_templates()

        result = service.validate_configuration(
            "test-strategy", {"period": "not a number", "threshold": 0.5}
        )

        assert not result.valid
        assert len(result.errors) > 0

    def test_validate_configuration_out_of_range(self, service):
        """Test validating configuration with out-of-range value."""
        service.load_templates()

        result = service.validate_configuration(
            "test-strategy", {"period": 200, "threshold": 0.5}  # max is 100
        )

        assert not result.valid
        assert any(e.code == "max_value" for e in result.errors)

    def test_apply_preset(self, service):
        """Test applying a preset."""
        service.load_templates()

        values = service.apply_preset("test-strategy", "conservative")

        assert values is not None
        assert values["period"] == 20
        assert values["threshold"] == 0.3

    def test_apply_nonexistent_preset(self, service):
        """Test applying a non-existent preset."""
        service.load_templates()

        values = service.apply_preset("test-strategy", "nonexistent")
        assert values is None

    def test_get_parameter_schema(self, service):
        """Test getting parameter schema."""
        service.load_templates()

        schema = service.get_parameter_schema("test-strategy")

        assert schema is not None
        assert "properties" in schema
        assert "period" in schema["properties"]
        assert "threshold" in schema["properties"]

    def test_to_engine_config(self, service):
        """Test converting to engine config."""
        service.load_templates()

        config = service.to_engine_config(
            "test-strategy", {"period": 20, "threshold": 0.5}, "BTCUSDT"
        )

        assert config is not None
        assert config["strategy_name"] == "TrendFollowing"
        assert config["symbol"] == "BTCUSDT"
        assert "parameters" in config


class TestValidationService:
    """Tests for ValidationService."""

    @pytest.fixture
    def service(self):
        """Create a ValidationService."""
        return ValidationService()

    @pytest.fixture
    def template(self):
        """Create a test template."""
        return {
            "parameters": [
                {
                    "name": "upper_price",
                    "type": "price",
                    "description": "Upper price",
                    "required": True,
                },
                {
                    "name": "lower_price",
                    "type": "price",
                    "description": "Lower price",
                    "required": True,
                    "validation": {
                        "type": "less_than",
                        "reference": "upper_price",
                        "message": "Lower price must be less than upper price",
                    },
                },
                {
                    "name": "grid_count",
                    "type": "integer",
                    "description": "Number of grids",
                    "default": 10,
                    "min": 3,
                    "max": 100,
                },
                {
                    "name": "optional_param",
                    "type": "string",
                    "description": "Optional parameter",
                    "required": False,
                },
            ]
        }

    def test_validate_valid_parameters(self, service, template):
        """Test validating valid parameters."""
        result = service.validate_parameters(
            template, {"upper_price": 50000, "lower_price": 40000, "grid_count": 10}
        )

        assert result.valid
        assert len(result.errors) == 0

    def test_validate_missing_required(self, service, template):
        """Test validating with missing required parameter."""
        result = service.validate_parameters(
            template, {"upper_price": 50000}  # missing lower_price
        )

        assert not result.valid
        assert any(e.code == "required" for e in result.errors)

    def test_validate_cross_parameter(self, service, template):
        """Test cross-parameter validation."""
        result = service.validate_parameters(
            template,
            {
                "upper_price": 40000,  # lower than lower_price
                "lower_price": 50000,
            },
        )

        assert not result.valid
        assert any(e.code == "less_than" for e in result.errors)

    def test_validate_range(self, service, template):
        """Test range validation."""
        result = service.validate_parameters(
            template,
            {
                "upper_price": 50000,
                "lower_price": 40000,
                "grid_count": 200,  # exceeds max of 100
            },
        )

        assert not result.valid
        assert any(e.code == "max_value" for e in result.errors)

    def test_validate_type_coercion(self, service, template):
        """Test type coercion."""
        result = service.validate_parameters(
            template,
            {
                "upper_price": "50000",  # string that should be coerced to float
                "lower_price": "40000",
                "grid_count": "10",  # string that should be coerced to int
            },
        )

        assert result.valid
        assert result.normalized_values["upper_price"] == 50000.0
        assert result.normalized_values["grid_count"] == 10

    def test_validate_uses_defaults(self, service, template):
        """Test that defaults are used for missing optional parameters."""
        result = service.validate_parameters(
            template, {"upper_price": 50000, "lower_price": 40000}
        )

        assert result.valid
        assert result.normalized_values["grid_count"] == 10


class TestStrategyTemplate:
    """Tests for StrategyTemplate dataclass."""

    def test_from_dict(self):
        """Test creating template from dict."""
        data = {
            "id": "test",
            "name": "Test",
            "version": "1.0.0",
            "category": "trend_following",
            "strategy_type": "trend_following",
            "parameters": [],
        }

        template = StrategyTemplate.from_dict(data)

        assert template.id == "test"
        assert template.name == "Test"
        assert template.difficulty == "intermediate"  # default

    def test_to_dict(self):
        """Test converting template to dict."""
        template = StrategyTemplate(
            id="test",
            name="Test",
            version="1.0.0",
            category="trend_following",
            strategy_type="trend_following",
        )

        data = template.to_dict()

        assert data["id"] == "test"
        assert data["name"] == "Test"

    def test_to_summary(self):
        """Test getting template summary."""
        template = StrategyTemplate(
            id="test",
            name="Test",
            version="1.0.0",
            category="trend_following",
            strategy_type="trend_following",
            description={"short": "Short description"},
        )

        summary = template.to_summary()

        assert summary["id"] == "test"
        assert summary["description_short"] == "Short description"
        assert "parameters" not in summary  # not in summary

    def test_get_preset(self):
        """Test getting preset values."""
        template = StrategyTemplate(
            id="test",
            name="Test",
            version="1.0.0",
            category="trend_following",
            strategy_type="trend_following",
            presets={
                "conservative": {
                    "name": "Conservative",
                    "values": {"period": 20},
                }
            },
        )

        values = template.get_preset("conservative")
        assert values == {"period": 20}

        values = template.get_preset("nonexistent")
        assert values is None

    def test_get_default_values(self):
        """Test getting default values."""
        template = StrategyTemplate(
            id="test",
            name="Test",
            version="1.0.0",
            category="trend_following",
            strategy_type="trend_following",
            parameters=[
                {"name": "period", "default": 14},
                {"name": "threshold", "default": 0.5},
                {"name": "no_default"},
            ],
        )

        defaults = template.get_default_values()

        assert defaults["period"] == 14
        assert defaults["threshold"] == 0.5
        assert "no_default" not in defaults
