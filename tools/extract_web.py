#!/usr/bin/env python3
import os
import re

def main():
    print("=== Extracting Web UI from web_pages.h ===")
    
    header_path = "include/web_pages.h"
    html_out_path = "web/index.html"
    
    if not os.path.exists(header_path):
        print(f"Error: {header_path} does not exist.")
        return
        
    with open(header_path, "r", encoding="utf-8") as f:
        content = f.read()
        
    # Match everything between R"rawliteral( and )rawliteral";
    # We use a pattern that matches R"rawliteral( ... )rawliteral";
    pattern = r'R"rawliteral\((.*?)\)rawliteral";'
    match = re.search(pattern, content, re.DOTALL)
    
    if match:
        html_content = match.group(1).strip()
        
        # Ensure output directory exists
        os.makedirs("web", exist_ok=True)
        
        with open(html_out_path, "w", encoding="utf-8", newline="\n") as out:
            out.write(html_content)
            
        print(f"Success: Web UI successfully extracted to {html_out_path}!")
        print("You can now open 'web/index.html' in your browser or run the mock server.")
    else:
        print("Error: Could not find rawliteral block in web_pages.h")

if __name__ == "__main__":
    main()
