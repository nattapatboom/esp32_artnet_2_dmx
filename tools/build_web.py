#!/usr/bin/env python3
"""Build web_pages.h from multi-file Web UI source.

Reads web/index.html (shell with <!-- PANE_* --> markers),
replaces markers with pane HTML from web/parts/,
embeds JS files from web/js/ in order,
minifies, gzips, writes include/web_pages.h.
"""
import gzip
import json
import os
import re

WEB_DIR = "web"
PARTS_DIR = os.path.join(WEB_DIR, "parts")
JS_DIR = os.path.join(WEB_DIR, "js")
OUTPUT_CONFIG_DIR = os.path.join(PARTS_DIR, "output-config")
OUTPUT_TEST_DIR = os.path.join(PARTS_DIR, "output-test")
JS_CONFIG_DIR = os.path.join(JS_DIR, "output-config")
JS_TEST_DIR = os.path.join(JS_DIR, "output-test")
HEADER_PATH = "include/web_pages.h"
SHELL_PATH = os.path.join(WEB_DIR, "index.html")
OUTPUT_DEFS_PATH = os.path.join("include", "output_defs.h")

PANE_MARKERS = {
    "<!-- PANE_NET -->":    "pane-network.html",
    "<!-- PANE_OUT -->":    "pane-outputs.html",
    "<!-- PANE_ESPNOW -->": "pane-espnow.html",
    "<!-- PANE_OTA -->":    "pane-ota.html",
    "<!-- PANE_DIAG -->":   "pane-diag.html",
}

JS_ORDER = ["_gpio.js", "network_protocol.js", "scoring.js", "espnow.js", "app.js", "outputs.js"]


def collect_type_files(directory, sort_key=lambda f: f):
    """Collect files from directory sorted by type number."""
    if not os.path.isdir(directory):
        return []
    files = [f for f in os.listdir(directory) if f.endswith(".html") or f.endswith(".js")]
    files.sort(key=sort_key)
    return files


def minify_html(html):
    return "\n".join(line.strip() for line in html.splitlines() if line.strip())


def c_byte_array(data, width=16):
    lines = []
    for i in range(0, len(data), width):
        chunk = data[i:i + width]
        lines.append("  " + ", ".join(f"0x{b:02x}" for b in chunk))
    return ",\n".join(lines)


def read_file(path):
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def split_cpp_args(text):
    args = []
    start = 0
    depth = 0
    for i, ch in enumerate(text):
        if ch in "({[":
            depth += 1
        elif ch in ")}]":
            depth -= 1
        elif ch == "," and depth == 0:
            args.append(text[start:i].strip())
            start = i + 1
    args.append(text[start:].strip())
    return args


def parse_source_mask(expr):
    values = {
        "SRC_GPIO": 1,
        "SRC_PCA": 2,
        "SRC_DIGITAL_EXPANDER": 4,
        "SRC_I2C_DAC": 8,
    }
    mask = 0
    for part in expr.split("|"):
        mask |= values[part.strip()]
    return mask


def mode_key(type_id, mode):
    if type_id in (12, 13) and mode in (4, 5):
        return "directDim"
    if type_id in (12, 13) and mode >= 6:
        return "commonDim"
    return "default" if mode == -1 else str(mode)


def generate_output_defs_js():
    src = read_file(OUTPUT_DEFS_PATH)

    type_ids = {
        name: int(value)
        for name, value in re.findall(r"constexpr\s+uint8_t\s+(TYPE_\w+)\s*=\s*(\d+)\s*;", src)
    }
    hardware = {
        name: {
            "ledc": int(ledc),
            "rmt": int(rmt),
            "uart": int(uart),
            "dac": int(dac),
            "timer": int(timer),
        }
        for name, ledc, rmt, uart, dac, timer in re.findall(
            r"constexpr\s+HardwareCost\s+(HW_\w+)\s*=\s*\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}\s*;",
            src,
        )
    }

    costs = {}
    for name, args_text in re.findall(r"constexpr\s+ModeCost\s+(COST_\w+)\s*=\s*modeCost\((.*?)\)\s*;", src):
        args = split_cpp_args(args_text)
        costs[name] = {
            "cpuUs": int(args[0]),
            "extraRam": int(args[1]) if len(args) > 1 else 0,
            "hardware": hardware[args[2]] if len(args) > 2 else hardware["HW_NONE"],
            "cpuPerUnit": int(args[3]) if len(args) > 3 else 0,
            "ramPerUnit": int(args[4]) if len(args) > 4 else 0,
            "dmxSlots": int(args[5]) if len(args) > 5 else 0,
        }

    pin_sets = {}
    pin_array_re = re.compile(r"constexpr\s+PinRule\s+(PINS_\w+)\[\]\s*=\s*\{(.*?)\}\s*;", re.DOTALL)
    pin_re = re.compile(
        r"\{\s*\"([^\"]+)\"\s*,\s*\"([^\"]+)\"\s*,\s*(.*?)\s*,\s*(PIN_OUTPUT|PIN_INPUT)\s*,\s*(true|false)\s*,\s*(\d+)\s*\}"
    )
    for name, body in pin_array_re.findall(src):
        pin_sets[name] = []
        for slot, label, sources, direction, invert, hw in pin_re.findall(body):
            pin_sets[name].append({
                "slot": slot,
                "label": label,
                "sources": parse_source_mask(sources),
                "dir": "out" if direction == "PIN_OUTPUT" else "in",
                "invert": invert == "true",
                "hwIfGpio": int(hw),
            })

    defs = {}
    modes_body = re.search(r"constexpr\s+OutputModeDef\s+OUTPUT_MODES\[\]\s*=\s*\{(.*?)\n\};", src, re.DOTALL).group(1)
    mode_re = re.compile(r"\{\s*(TYPE_\w+)\s*,\s*(-?\d+)\s*,\s*\"([^\"]+)\"\s*,\s*(COST_\w+)\s*,\s*(PINS_\w+)\s*,\s*(\d+)\s*\}")
    for type_name, mode_value, name, cost_name, pins_name, pin_count in mode_re.findall(modes_body):
        type_id = type_ids[type_name]
        mode = int(mode_value)
        entry = defs.setdefault(str(type_id), {"name": name, "modes": {}})
        pins = {}
        for pin in pin_sets[pins_name][:int(pin_count)]:
            pins[pin["slot"]] = {k: v for k, v in pin.items() if k != "slot"}
        entry["modes"][mode_key(type_id, mode)] = {
            "label": name,
            "cost": costs[cost_name],
            "pins": pins,
        }

    return "const OUTPUT_DEFS=" + json.dumps(defs, separators=(",", ":")) + ";\n"


def main():
    print(f"=== Compiling {SHELL_PATH} -> {HEADER_PATH} ===")

    if not os.path.exists(SHELL_PATH):
        print(f"Error: {SHELL_PATH} does not exist.")
        return

    # Read shell
    html = read_file(SHELL_PATH)

    # Replace pane markers
    for marker, filename in PANE_MARKERS.items():
        pane_path = os.path.join(PARTS_DIR, filename)
        if not os.path.exists(pane_path):
            print(f"Warning: {pane_path} not found, leaving marker {marker}")
            continue
        pane_html = read_file(pane_path)
        if marker not in html:
            print(f"Warning: marker {marker} not found in shell, skipping")
            continue
        html = html.replace(marker, pane_html)

    # Replace per-type config/test markers with all type-N.html files
    for marker_name, src_dir in [("CONFIG_TYPE_FIELDS", OUTPUT_CONFIG_DIR),
                                   ("TEST_TYPE_FIELDS", OUTPUT_TEST_DIR)]:
        marker = f"<!-- {marker_name} -->"
        if marker not in html:
            print(f"Warning: marker {marker} not found in shell, skipping")
            continue
        type_files = collect_type_files(src_dir)
        combined = "\n".join(read_file(os.path.join(src_dir, f)) for f in type_files
                             if os.path.isfile(os.path.join(src_dir, f)))
        html = html.replace(marker, combined)

    # Embed JS files into <script> tag (generated metadata, main JS, then per-type config/test)
    js_parts = [generate_output_defs_js()]
    for js_file in JS_ORDER:
        js_path = os.path.join(JS_DIR, js_file)
        if not os.path.exists(js_path):
            print(f"Warning: {js_path} not found, skipping")
            continue
        js_parts.append(read_file(js_path))

    for dir_path in [JS_CONFIG_DIR, JS_TEST_DIR]:
        for js_file in collect_type_files(dir_path):
            js_path = os.path.join(dir_path, js_file)
            js_parts.append(read_file(js_path))

    combined_js = "\n".join(js_parts)
    html = html.replace("<script>", f"<script>\n{combined_js}\n</script>")

    # Minify
    minified = minify_html(html)

    # Gzip
    compressed = gzip.compress(minified.encode("utf-8"), compresslevel=9)
    if gzip.decompress(compressed).decode("utf-8") != minified:
        raise RuntimeError("gzip verification failed")

    array_body = c_byte_array(compressed)

    cpp_header = f"""#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <Arduino.h>

const uint8_t CONFIG_HTML_GZ[] PROGMEM = {{
{array_body}
}};

const size_t CONFIG_HTML_GZ_LEN = sizeof(CONFIG_HTML_GZ);

#endif // WEB_PAGES_H
"""

    with open(HEADER_PATH, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(cpp_header)

    ratio = (len(compressed) / len(minified)) * 100 if minified else 0
    saved = len(minified) - len(compressed)
    print(f"Success: compiled {len(minified)} bytes to {len(compressed)} gzip bytes ({ratio:.1f}%, saved {saved} bytes) in {HEADER_PATH}!")


if __name__ == "__main__":
    main()
