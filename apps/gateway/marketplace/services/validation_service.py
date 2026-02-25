"""
Parameter Validation Service

Provides validation for strategy parameters including type checking,
range validation, and cross-parameter validation.
"""

import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional


@dataclass
class ValidationError:
    """Represents a validation error."""

    parameter: str
    message: str
    code: str
    value: Any = None


@dataclass
class ValidationResult:
    """Result of parameter validation."""

    valid: bool
    errors: list[ValidationError] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    normalized_values: dict[str, Any] = field(default_factory=dict)


class ValidationService:
    """
    Service for validating strategy parameters.

    Supports:
    - Type validation (integer, float, percentage, currency, price, boolean, string, enum)
    - Range validation (min, max)
    - Cross-parameter validation (less_than, greater_than)
    - Regex pattern validation
    - Required field validation
    - Conditional validation (depends_on)
    """

    # Type coercion functions
    TYPE_COERCERS = {
        "integer": lambda v: int(v) if v is not None else None,
        "float": lambda v: float(v) if v is not None else None,
        "percentage": lambda v: float(v) if v is not None else None,
        "currency": lambda v: float(v) if v is not None else None,
        "price": lambda v: float(v) if v is not None else None,
        "boolean": lambda v: bool(v) if v is not None else None,
        "string": lambda v: str(v) if v is not None else None,
        "enum": lambda v: str(v) if v is not None else None,
    }

    def __init__(self):
        self._schema_path = (
            Path(__file__).parent.parent / "schemas" / "strategy_template.json"
        )
        self._schema = None

    def load_schema(self) -> dict:
        """Load the JSON schema for strategy templates."""
        if self._schema is None:
            with open(self._schema_path, "r") as f:
                self._schema = json.load(f)
        return self._schema

    def validate_parameters(
        self,
        template: dict,
        user_values: dict[str, Any],
        strict: bool = True,
    ) -> ValidationResult:
        """
        Validate user-provided parameter values against a strategy template.

        Args:
            template: Strategy template definition
            user_values: User-provided parameter values
            strict: If True, fail on unknown parameters

        Returns:
            ValidationResult with validation status and any errors
        """
        errors: list[ValidationError] = []
        warnings: list[str] = []
        normalized: dict[str, Any] = {}

        # Build parameter lookup
        param_defs = {p["name"]: p for p in template.get("parameters", [])}

        # Check for unknown parameters
        if strict:
            for key in user_values:
                if key not in param_defs:
                    errors.append(
                        ValidationError(
                            parameter=key,
                            message=f"Unknown parameter: {key}",
                            code="unknown_parameter",
                            value=user_values[key],
                        )
                    )

        # Validate each defined parameter
        for param_name, param_def in param_defs.items():
            value = user_values.get(param_name)

            # Check conditional visibility
            if not self._check_depends_on(param_def, user_values):
                # Parameter is hidden, skip validation
                continue

            # Check required
            is_required = param_def.get("required", True)
            if value is None:
                if is_required and param_def.get("default") is None:
                    errors.append(
                        ValidationError(
                            parameter=param_name,
                            message=f"Required parameter missing: {param_name}",
                            code="required",
                        )
                    )
                else:
                    # Use default value
                    normalized[param_name] = param_def.get("default")
                continue

            # Type coercion and validation
            param_type = param_def.get("type", "string")
            try:
                coerced = self._coerce_value(value, param_type)
                normalized[param_name] = coerced
            except (ValueError, TypeError) as e:
                errors.append(
                    ValidationError(
                        parameter=param_name,
                        message=f"Invalid type for {param_name}: expected {param_type}",
                        code="type_error",
                        value=value,
                    )
                )
                continue

            # Range validation
            range_error = self._validate_range(param_name, coerced, param_def)
            if range_error:
                errors.append(range_error)

            # Enum validation
            if param_type == "enum":
                enum_error = self._validate_enum(param_name, coerced, param_def)
                if enum_error:
                    errors.append(enum_error)

            # Custom validation rules
            validation_rule = param_def.get("validation")
            if validation_rule:
                rule_error = self._validate_rule(
                    param_name, coerced, validation_rule, user_values, param_defs
                )
                if rule_error:
                    errors.append(rule_error)

        # Cross-parameter validation
        cross_errors = self._validate_cross_parameters(normalized, param_defs)
        errors.extend(cross_errors)

        return ValidationResult(
            valid=len(errors) == 0,
            errors=errors,
            warnings=warnings,
            normalized_values=normalized,
        )

    def _coerce_value(self, value: Any, param_type: str) -> Any:
        """Coerce a value to the specified type."""
        coercer = self.TYPE_COERCERS.get(param_type)
        if coercer:
            return coercer(value)
        return value

    def _validate_range(
        self, param_name: str, value: Any, param_def: dict
    ) -> Optional[ValidationError]:
        """Validate numeric range constraints."""
        if not isinstance(value, (int, float)):
            return None

        min_val = param_def.get("min")
        max_val = param_def.get("max")

        if min_val is not None and value < min_val:
            return ValidationError(
                parameter=param_name,
                message=f"{param_name} must be at least {min_val}",
                code="min_value",
                value=value,
            )

        if max_val is not None and value > max_val:
            return ValidationError(
                parameter=param_name,
                message=f"{param_name} must be at most {max_val}",
                code="max_value",
                value=value,
            )

        return None

    def _validate_enum(
        self, param_name: str, value: Any, param_def: dict
    ) -> Optional[ValidationError]:
        """Validate enum value against allowed options."""
        enum_values = param_def.get("enum_values", [])
        allowed = [e["value"] for e in enum_values]

        if value not in allowed:
            return ValidationError(
                parameter=param_name,
                message=f"{param_name} must be one of: {', '.join(allowed)}",
                code="invalid_enum",
                value=value,
            )

        return None

    def _validate_rule(
        self,
        param_name: str,
        value: Any,
        rule: dict,
        all_values: dict,
        param_defs: dict,
    ) -> Optional[ValidationError]:
        """Validate against a custom validation rule."""
        rule_type = rule.get("type")
        message = rule.get("message", f"Validation failed for {param_name}")

        if rule_type == "less_than":
            ref_param = rule.get("reference")
            ref_value = all_values.get(ref_param)
            # Coerce reference value if needed
            if ref_param in param_defs:
                ref_type = param_defs[ref_param].get("type", "string")
                try:
                    ref_value = self._coerce_value(ref_value, ref_type)
                except (ValueError, TypeError):
                    pass
            if ref_value is not None and value >= ref_value:
                return ValidationError(
                    parameter=param_name,
                    message=message or f"{param_name} must be less than {ref_param}",
                    code="less_than",
                    value=value,
                )

        elif rule_type == "greater_than":
            ref_param = rule.get("reference")
            ref_value = all_values.get(ref_param)
            # Coerce reference value if needed
            if ref_param in param_defs:
                ref_type = param_defs[ref_param].get("type", "string")
                try:
                    ref_value = self._coerce_value(ref_value, ref_type)
                except (ValueError, TypeError):
                    pass
            if ref_value is not None and value <= ref_value:
                return ValidationError(
                    parameter=param_name,
                    message=message
                    or f"{param_name} must be greater than {ref_param}",
                    code="greater_than",
                    value=value,
                )

        elif rule_type == "regex":
            pattern = rule.get("pattern")
            if pattern and not re.match(pattern, str(value)):
                return ValidationError(
                    parameter=param_name,
                    message=message or f"{param_name} does not match required pattern",
                    code="regex",
                    value=value,
                )

        return None

    def _check_depends_on(self, param_def: dict, all_values: dict) -> bool:
        """Check if a parameter should be visible based on depends_on condition."""
        depends = param_def.get("depends_on")
        if not depends:
            return True

        ref_param = depends.get("parameter")
        condition = depends.get("condition", "equals")
        expected = depends.get("value")
        actual = all_values.get(ref_param)

        if condition == "equals":
            return actual == expected
        elif condition == "not_equals":
            return actual != expected
        elif condition == "exists":
            return actual is not None
        elif condition == "not_exists":
            return actual is None

        return True

    def _validate_cross_parameters(
        self, values: dict, param_defs: dict
    ) -> list[ValidationError]:
        """Perform cross-parameter validation."""
        errors = []

        # Common cross-parameter validations
        # upper_price > lower_price
        if "upper_price" in values and "lower_price" in values:
            if values["upper_price"] <= values["lower_price"]:
                errors.append(
                    ValidationError(
                        parameter="upper_price",
                        message="Upper price must be greater than lower price",
                        code="cross_validation",
                    )
                )

        # fast_period < slow_period (for MACD-like strategies)
        if "fast_period" in values and "slow_period" in values:
            if values["fast_period"] >= values["slow_period"]:
                errors.append(
                    ValidationError(
                        parameter="fast_period",
                        message="Fast period must be less than slow period",
                        code="cross_validation",
                    )
                )

        # entry_threshold > exit_threshold (for mean reversion)
        if "entry_threshold" in values and "exit_threshold" in values:
            if values["entry_threshold"] <= values["exit_threshold"]:
                errors.append(
                    ValidationError(
                        parameter="entry_threshold",
                        message="Entry threshold must be greater than exit threshold",
                        code="cross_validation",
                    )
                )

        return errors

    def get_parameter_schema(self, template: dict) -> dict:
        """
        Generate a JSON Schema for the template's parameters.

        Useful for frontend form generation.
        """
        properties = {}
        required = []

        for param in template.get("parameters", []):
            name = param["name"]
            param_type = param.get("type", "string")

            prop = {
                "title": param.get("display_name", name),
                "description": param.get("description", ""),
            }

            # Map types to JSON Schema types
            if param_type in ("integer",):
                prop["type"] = "integer"
            elif param_type in ("float", "percentage", "currency", "price"):
                prop["type"] = "number"
            elif param_type == "boolean":
                prop["type"] = "boolean"
            elif param_type == "enum":
                prop["type"] = "string"
                prop["enum"] = [e["value"] for e in param.get("enum_values", [])]
            else:
                prop["type"] = "string"

            # Add constraints
            if "min" in param:
                prop["minimum"] = param["min"]
            if "max" in param:
                prop["maximum"] = param["max"]
            if "default" in param:
                prop["default"] = param["default"]

            properties[name] = prop

            if param.get("required", True):
                required.append(name)

        return {
            "$schema": "https://json-schema.org/draft/2020-12/schema",
            "type": "object",
            "properties": properties,
            "required": required,
        }
