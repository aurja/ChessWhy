#!/usr/bin/env python3
"""Validate ChessWhy report JSON files against a Draft 2020-12 schema."""

from __future__ import annotations

import argparse
import json
import sys
from collections.abc import Iterable, Sequence
from pathlib import Path
from typing import Protocol, TypeAlias, cast

from jsonschema import Draft202012Validator
from jsonschema.exceptions import SchemaError, ValidationError

JsonValue: TypeAlias = (
    None | bool | int | float | str | list["JsonValue"] | dict[str, "JsonValue"]
)
JsonSchema: TypeAlias = bool | dict[str, JsonValue]


class Arguments(argparse.Namespace):
    def __init__(self) -> None:
        super().__init__()
        self.schema: Path = Path()
        self.reports: list[Path] = []


class ErrorValidator(Protocol):
    def iter_errors(self, instance: JsonValue) -> Iterable[ValidationError]: ...


def read_json(filename: Path) -> JsonValue:
    contents = filename.read_text(encoding="utf-8")
    return cast(JsonValue, json.loads(contents))


def json_path(parts: Iterable[object]) -> str:
    """Format a validation location as JSONPath, using `$` for the root."""
    location = "$"
    for part in parts:
        if isinstance(part, int):
            location += f"[{part}]"
        else:
            location += f"[{json.dumps(str(part))}]"
    return location


def report_failure(filename: Path, location: str, message: str) -> None:
    single_line = " ".join(message.splitlines())
    print(f"{filename}: {location}: {single_line}", file=sys.stderr)


def error_key(error: ValidationError) -> tuple[tuple[str, ...], str]:
    path = cast(Iterable[object], error.absolute_path)
    return tuple(str(part) for part in path), error.message


def validation_errors(
    validator: Draft202012Validator, instance: JsonValue
) -> list[ValidationError]:
    typed_validator = cast(ErrorValidator, validator)
    errors = list(typed_validator.iter_errors(instance))
    errors.sort(key=error_key)
    return errors


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="validate report JSON files against a Draft 2020-12 schema"
    )
    _ = parser.add_argument("schema", type=Path)
    _ = parser.add_argument("reports", type=Path, nargs="+")
    arguments = parser.parse_args(argv, namespace=Arguments())

    try:
        schema_value = read_json(arguments.schema)
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        report_failure(arguments.schema, "$", f"cannot read schema: {error}")
        return 1

    schema = cast(JsonSchema, schema_value)
    try:
        Draft202012Validator.check_schema(schema)
    except SchemaError as error:
        report_failure(
            arguments.schema,
            json_path(error.absolute_path),
            error.message,
        )
        return 1

    validator = Draft202012Validator(schema)
    failures = False
    for filename in arguments.reports:
        try:
            instance = read_json(filename)
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            report_failure(filename, "$", f"cannot read report: {error}")
            failures = True
            continue

        for error in validation_errors(validator, instance):
            report_failure(
                filename,
                json_path(error.absolute_path),
                error.message,
            )
            failures = True
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
