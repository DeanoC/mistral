#!/usr/bin/env python3
"""Verify bundled reference checksums and run the LAB/MLAB whole-RBF oracle."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("comparator", type=Path, help="compiled lab-clock-oracle executable")
args = parser.parse_args()
fixtures = Path(__file__).resolve().parent / "fixtures" / "lab-clock"
manifest = json.loads((fixtures / "manifest.json").read_text())
with tempfile.TemporaryDirectory(prefix="mistral-clock-oracle-") as scratch:
    out = Path(scratch)
    for name, expected in manifest.items():
        data = gzip.decompress((fixtures / (name + ".gz")).read_bytes())
        if len(data) != expected["bytes"] or hashlib.sha256(data).hexdigest() != expected["sha256"]:
            raise SystemExit("fixture checksum mismatch: " + name)
        (out / name).write_bytes(data)
    for block in ("LAB", "MLAB"):
        subprocess.run(
            [
                str(args.comparator.resolve()),
                str(out / (block.lower() + "-pos.rbf")),
                str(out / (block.lower() + "-neg.rbf")),
                block,
                "3",
            ],
            check=True,
        )
