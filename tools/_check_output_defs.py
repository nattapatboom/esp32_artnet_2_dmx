"""Quick script to check generated OUTPUT_DEFS type-12 modes"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))
os.chdir(os.path.join(os.path.dirname(__file__), '..'))
from build_web import generate_output_defs_js
j = generate_output_defs_js()
# Find type 12
idx = j.find('"12"')
if idx < 0: idx = j.find("'12'")
if idx < 0:
    print("type 12 not found")
    sys.exit(1)
# Get the modes block
modes_start = j.find('modes', idx, idx+500)
if modes_start < 0:
    print("modes key not found near type 12")
    # Print context
    print(j[max(0,idx-50):idx+500])
    sys.exit(1)
# Find the opening brace of modes value
brace = j.find('{', modes_start+5)
depth = 1
i = brace + 1
while depth > 0 and i < len(j):
    if j[i] == '{': depth += 1
    elif j[i] == '}': depth -= 1
    i += 1
modes_text = j[brace:i]
print("Type 12 modes keys:", [k.strip().strip('"') for k in modes_text.split(',') if ':' in k])
