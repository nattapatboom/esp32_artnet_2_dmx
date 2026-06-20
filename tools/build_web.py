#!/usr/bin/env python3
import gzip
import os


def minify_html(html):
    # Conservative minify: strip indentation/blank lines only. Gzip handles the rest
    # without risking JavaScript/template literal breakage.
    return "\n".join(line.strip() for line in html.splitlines() if line.strip())


def c_byte_array(data, width=16):
    lines = []
    for i in range(0, len(data), width):
        chunk = data[i:i + width]
        lines.append("  " + ", ".join(f"0x{b:02x}" for b in chunk))
    return ",\n".join(lines)

def main():
    print("=== Compiling web/index.html to web_pages.h ===")
    
    html_path = "web/index.html"
    header_path = "include/web_pages.h"
    
    if not os.path.exists(html_path):
        print(f"Error: {html_path} does not exist. Please run extract_web.py first.")
        return
        
    with open(html_path, "r", encoding="utf-8") as f:
        html_content = minify_html(f.read())

    compressed = gzip.compress(html_content.encode("utf-8"), compresslevel=9)
    if gzip.decompress(compressed).decode("utf-8") != html_content:
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
    
    with open(header_path, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(cpp_header)
        
    ratio = (len(compressed) / len(html_content)) * 100 if html_content else 0
    saved = len(html_content) - len(compressed)
    print(f"Success: compiled {len(html_content)} bytes to {len(compressed)} gzip bytes ({ratio:.1f}%, saved {saved} bytes) in {header_path}!")

if __name__ == "__main__":
    main()
