#!/usr/bin/env python3
"""Validate OTF-1 packets against schema/otf-1.schema.json.

Usage:
    python tools/validate.py [--expect-invalid] [file_or_dir ...]

With no arguments, validates every *.json file under examples/.

A *.json file holds one packet. A *.jsonl file holds one packet per line,
which is how a fleet log is written -- a coordinator's wire log can be
checked directly:

    python tools/validate.py fleet-log.jsonl

--expect-invalid flips the check: every packet given must FAIL validation.
That is how tests/invalid/ is run, so a schema change that quietly starts
accepting a malformed packet is caught the same way one that breaks a good
example is.
"""
import json
import sys
from pathlib import Path

try:
    import jsonschema
except ImportError:
    print("Missing dependency: pip install jsonschema", file=sys.stderr)
    sys.exit(2)

REPO_ROOT = Path(__file__).resolve().parent.parent
SCHEMA_PATH = REPO_ROOT / "schema" / "otf-1.schema.json"


def load_schema():
    with open(SCHEMA_PATH, encoding="utf-8") as f:
        return json.load(f)


def collect_targets(args):
    if not args:
        return sorted((REPO_ROOT / "examples").glob("*.json"))
    targets = []
    for arg in args:
        p = Path(arg)
        if p.is_dir():
            targets.extend(sorted(p.glob("*.json")))
            targets.extend(sorted(p.glob("*.jsonl")))
        else:
            targets.append(p)
    return targets


def load_packets(path):
    """Yield (label, packet, parse_error) for every packet in a file."""
    with open(path, encoding="utf-8") as f:
        if path.suffix == ".jsonl":
            for number, line in enumerate(f, start=1):
                if not line.strip():
                    continue
                label = f"{path}:{number}"
                try:
                    yield label, json.loads(line), None
                except json.JSONDecodeError as exc:
                    yield label, None, f"not valid JSON: {exc}"
        else:
            try:
                yield str(path), json.load(f), None
            except json.JSONDecodeError as exc:
                yield str(path), None, f"not valid JSON: {exc}"


def describe(packet):
    if isinstance(packet, dict):
        return packet.get("kind", "event")
    return "?"


def main():
    args = sys.argv[1:]
    expect_invalid = "--expect-invalid" in args
    args = [a for a in args if a != "--expect-invalid"]

    schema = load_schema()
    jsonschema.Draft202012Validator.check_schema(schema)
    validator = jsonschema.Draft202012Validator(schema)

    targets = collect_targets(args)
    if not targets:
        print("No files to validate.")
        return 0

    checked = 0
    failures = 0
    for path in targets:
        # A log can hold thousands of packets; print one line per file for
        # those and one line per packet only when something is wrong.
        quiet = path.suffix == ".jsonl"
        file_checked = 0
        file_failures = 0
        for label, packet, parse_error in load_packets(path):
            checked += 1
            file_checked += 1
            if parse_error:
                messages = [f"  <root>: {parse_error}"]
            else:
                errors = sorted(validator.iter_errors(packet), key=lambda e: list(map(str, e.path)))
                # When a packet breaks a rule of its own kind, the schema also
                # reports every field of that kind as "unevaluated". That is a
                # side effect of the real error, not a second one, so show it
                # only when it is the whole story (a genuinely stray field).
                specific = [e for e in errors if e.validator != "unevaluatedProperties"]
                errors = specific or errors
                messages = [
                    f"  {'/'.join(str(p) for p in err.path) or '<root>'}: {err.message}"
                    for err in errors
                ]
            is_invalid = bool(messages)

            if expect_invalid:
                if is_invalid:
                    if not quiet:
                        print(f"OK   {label}  (rejected, as it should be)")
                else:
                    failures += 1
                    file_failures += 1
                    print(f"FAIL {label}  (accepted, but must be rejected)")
            elif is_invalid:
                failures += 1
                file_failures += 1
                print(f"FAIL {label}  [{describe(packet)}]")
                for message in messages:
                    print(message)
            elif not quiet:
                print(f"OK   {label}")

        if quiet and not file_failures:
            print(f"OK   {path}  ({file_checked} packets)")

    if failures:
        print(f"\n{failures} of {checked} packet(s) failed the check.")
        return 1
    if expect_invalid:
        print(f"\nAll {checked} packet(s) rejected by OTF-1, as expected.")
    else:
        print(f"\nAll {checked} packet(s) valid against OTF-1.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
