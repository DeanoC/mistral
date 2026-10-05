#!/usr/bin/env python3
"""Verify the bundled slew-rate references and run the multi-bit boolean oracle."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("comparator", type=Path, help="compiled multibit-bool executable")
args = parser.parse_args()
fixtures = Path(__file__).resolve().parent / "fixtures" / "gpio-slew"
manifest = json.loads((fixtures / "manifest.json").read_text())
with tempfile.TemporaryDirectory(prefix="mistral-slew-oracle-") as scratch:
    out = Path(scratch)
    for name, expected in manifest.items():
        data = gzip.decompress((fixtures / (name + ".gz")).read_bytes())
        if len(data) != expected["bytes"] or hashlib.sha256(data).hexdigest() != expected["sha256"]:
            raise SystemExit("fixture checksum mismatch: " + name)
        (out / name).write_bytes(data)
    subprocess.run(
        [str(args.comparator.resolve()), str(out / "slew-fast.rbf"), str(out / "slew-slow.rbf")],
        check=True,
    )
