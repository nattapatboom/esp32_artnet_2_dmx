#!/usr/bin/env python3
import os

def main():
    print("=== Compiling web/index.html to web_pages.h ===")
    
    html_path = "web/index.html"
    header_path = "include/web_pages.h"
    
    if not os.path.exists(html_path):
        print(f"Error: {html_path} does not exist. Please run extract_web.py first.")
        return
        
    with open(html_path, "r", encoding="utf-8") as f:
        html_content = f.read()
        
    cpp_header = f"""#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <Arduino.h>

const char CONFIG_HTML[] PROGMEM = R"rawliteral({html_content})rawliteral";

#endif // WEB_PAGES_H
"""
    
    with open(header_path, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(cpp_header)
        
    print(f"Success: Compiled {html_path} into {header_path}!")

if __name__ == "__main__":
    main()
