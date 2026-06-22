#!/usr/bin/env python3
"""Build web_pages.h from multi-file Web UI source.

Reads web/index.html (shell with <!-- PANE_* --> markers),
replaces markers with pane HTML from web/parts/,
embeds JS files from web/js/ in order,
minifies, gzips, writes include/web_pages.h.
"""
import gzip
import os

WEB_DIR = "web"
PARTS_DIR = os.path.join(WEB_DIR, "parts")
JS_DIR = os.path.join(WEB_DIR, "js")
HEADER_PATH = "include/web_pages.h"
SHELL_PATH = os.path.join(WEB_DIR, "index.html")

PANE_MARKERS = {
    "<!-- PANE_NET -->":    "pane-network.html",
    "<!-- PANE_OUT -->":    "pane-outputs.html",
    "<!-- PANE_ESPNOW -->": "pane-espnow.html",
    "<!-- PANE_OTA -->":    "pane-ota.html",
    "<!-- PANE_DIAG -->":   "pane-diag.html",
}

JS_ORDER = ["_gpio.js", "scoring.js", "espnow.js", "app.js", "outputs.js"]


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

    # Embed JS files into <script> tag
    js_parts = []
    for js_file in JS_ORDER:
        js_path = os.path.join(JS_DIR, js_file)
        if not os.path.exists(js_path):
            print(f"Warning: {js_path} not found, skipping")
            continue
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
