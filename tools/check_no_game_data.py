#!/usr/bin/env python3
"""Fail if any tracked file looks like Resident Evil 4 game data.

The repository holds source, tools and docs only. Everyone building it supplies their own
disc images, which stay outside the repository (see docs/pc-port.md). This check runs in CI
on every push and pull request and catches files force-added past .gitignore.

    python3 tools/check_no_game_data.py            # check tracked files (git ls-files)
    python3 tools/check_no_game_data.py --staged   # check what is staged for commit
"""
import subprocess
import sys
from pathlib import Path

# Disc images, archives and the game's own data formats.
BANNED_EXT = {
    ".iso", ".gcm", ".ciso", ".cso", ".rvz", ".wbfs", ".gcz", ".wia", ".tgc", ".nkit",
    ".dol", ".rel", ".rar", ".7z", ".zip",
    ".das", ".drs", ".dat", ".tpl", ".bin", ".esl", ".evd", ".sfd", ".adx", ".udas",
    ".eff", ".uwf", ".bnr", ".snd", ".sym",
}

# (offset, bytes) signatures of disc images, archives and GameCube data formats.
SIGNATURES = [
    (0x1C, bytes.fromhex("C2339F3D"), "GameCube disc image"),
    (0x18, bytes.fromhex("5D1C9EA3"), "Wii disc image"),
    (0, b"CISO", "CISO disc image"),
    (0, b"RVZ\x01", "RVZ disc image"),
    (0, b"WIA\x01", "WIA disc image"),
    (0, b"WBFS", "WBFS disc image"),
    (0, b"Rar!", "RAR archive"),
    (0, b"7z\xbc\xaf\x27\x1c", "7z archive"),
    (0, b"PK\x03\x04", "zip archive"),
    (0, b"Yaz0", "Yaz0-compressed game data"),
    (0, bytes.fromhex("0020AF30"), "TPL texture"),
    (0, b"RIFF", "RIFF audio/video"),
]

# Text this large is fine (docs/research/*.md); binaries this large are not.
MAX_BINARY = 512 * 1024

# Paths that are allowed to be binary (add with care, and never game data).
ALLOW = set()


def tracked(staged: bool) -> list[str]:
    cmd = ["git", "diff", "--cached", "--name-only", "--diff-filter=AM", "-z"] if staged else ["git", "ls-files", "-z"]
    out = subprocess.run(cmd, check=True, capture_output=True).stdout
    return [p for p in out.decode("utf-8", "surrogateescape").split("\0") if p]


def is_binary(head: bytes) -> bool:
    return b"\0" in head


def check(path: str) -> str | None:
    p = Path(path)
    if path in ALLOW or not p.is_file():
        return None
    if p.suffix.lower() in BANNED_EXT or p.name.lower().endswith((".nkit.iso", ".nkit.gcz")):
        return f"banned file type {p.suffix}"
    with p.open("rb") as f:
        head = f.read(0x40)
    for off, sig, what in SIGNATURES:
        if head[off:off + len(sig)] == sig:
            return what
    if is_binary(head) and p.stat().st_size > MAX_BINARY:
        return f"binary file over {MAX_BINARY // 1024} KB"
    return None


def main() -> int:
    bad = [(p, why) for p in tracked("--staged" in sys.argv) if (why := check(p))]
    for p, why in bad:
        print(f"game data not allowed: {p} ({why})")
    if bad:
        print("\nThis repository must not contain game files. Keep your discs and extracted data")
        print("outside the repository and point the build at them (docs/pc-port.md).")
        return 1
    print("ok: no game data in the repository")
    return 0


if __name__ == "__main__":
    sys.exit(main())
