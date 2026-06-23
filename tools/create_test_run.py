#!/usr/bin/env python3
import argparse
import datetime as dt
import os
import subprocess
import sys


def repo_root() -> str:
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def git_text(root: str, *args: str) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        return "unknown"
    return result.stdout.strip()


def checklist_body(source: str) -> str:
    text = source.replace("\r\n", "\n")
    marker = "\n## 1. Build And Static Checks\n"
    idx = text.find(marker)
    if idx < 0:
        raise ValueError("Could not find checklist section marker")
    return text[idx + 1:].rstrip() + "\n"


def local_timestamp(now: dt.datetime) -> str:
    text = now.strftime("%Y-%m-%d %H:%M:%S %z")
    if len(text) >= 5:
        return f"{text[:-2]}:{text[-2:]}"
    return text


def main() -> int:
    parser = argparse.ArgumentParser(description="Create a timestamped test run checklist record.")
    parser.add_argument("--tester", default="", help="Tester name")
    parser.add_argument("--device-ip", default="", help="Device IP or connection note")
    parser.add_argument("--hardware-setup", default="", help="Hardware setup description")
    parser.add_argument("--scope", default="", help="Test scope description")
    parser.add_argument("--result", default="Pending", help="Initial result summary")
    parser.add_argument("--timestamp", default="", help="Override timestamp as YYYY-MM-DD HH:MM:SS +07:00")
    args = parser.parse_args()

    root = repo_root()
    source_path = os.path.join(root, "docs", "test_checklist.md")
    runs_dir = os.path.join(root, "docs", "test_runs")

    if not os.path.exists(source_path):
        print(f"[-] Missing checklist source: {source_path}")
        return 1
    os.makedirs(runs_dir, exist_ok=True)

    now = dt.datetime.now().astimezone()
    timestamp = args.timestamp or local_timestamp(now)
    file_stamp = now.strftime("%Y-%m-%d_%H%M")
    output_path = os.path.join(runs_dir, f"{file_stamp}_test_checklist.md")
    if os.path.exists(output_path):
        print(f"[-] Test run file already exists: {output_path}")
        return 1

    short_commit = git_text(root, "rev-parse", "--short", "HEAD")
    commit_subject = git_text(root, "log", "-1", "--pretty=%s")

    with open(source_path, "r", encoding="utf-8") as f:
        body = checklist_body(f.read())

    header = f"""# Test Run Record

- Date/time: {timestamp}
- Firmware commit: `{short_commit}` (`{commit_subject}`)
- Firmware version/build:
- Tester: {args.tester}
- Device/IP: {args.device_ip}
- Hardware setup: {args.hardware_setup}
- Test scope: {args.scope}
- Result summary: {args.result}

## Incomplete Or Failed Items

- [ ]

## Additional Test Items Found

- [ ]

## Checklist

"""

    with open(output_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(header)
        f.write(body)

    rel_path = os.path.relpath(output_path, root)
    print(f"[+] Created {rel_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
