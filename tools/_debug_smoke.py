"""Debug: print the HTML that renderPinRows produces for type 12 mode 2"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))
os.chdir(os.path.join(os.path.dirname(__file__), '..'))

from build_web import generate_gpio_js, generate_source_rules_js, generate_output_defs_js

# Collect all JS
js_parts = [generate_gpio_js(), generate_source_rules_js(), generate_output_defs_js()]
JS_ORDER = ["_gpio.js", "network_protocol.js", "scoring.js", "espnow.js", "app.js", "outputs.js"]
for js_file in JS_ORDER:
    with open(os.path.join("web/js", js_file)) as f:
        js_parts.append(f.read())

# per-type JS files removed in v2 of test tool migration

combined = "\n".join(js_parts)

# Find renderPinRows function
idx = combined.find("function renderPinRows()")
if idx >= 0:
    print("renderPinRows found at index", idx)
    # Check segmentPinLayout reference  
    seg_check = combined.count("segmentPinLayout")
    print(f"segmentPinLayout references: {seg_check}")
else:
    print("renderPinRows NOT FOUND!")
