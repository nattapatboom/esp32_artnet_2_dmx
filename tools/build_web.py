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
HEADER_PATH = "include/web_pages.h"
SHELL_PATH = os.path.join(WEB_DIR, "index.html")
OUTPUT_DEFS_PATH = os.path.join("include", "output_defs.h")
SOURCE_RULES_PATH = os.path.join("include", "source_rules.h")
GPIO_CONTROL_PATH = os.path.join("include", "gpio_control.h")
DISPLAY_PROTO_PATH = os.path.join("include", "display_protocol.h")
SCORE_LIMITS_PATH = os.path.join("include", "scoring_limits.h")
DIMMER_PATH = os.path.join("include", "output_devices", "dimmer.h")
SCORING_PATH = os.path.join("include", "scoring.h")

PANE_MARKERS = {
    "<!-- PANE_NET -->":    "pane-network.html",
    "<!-- PANE_OUT -->":    "pane-outputs.html",
    "<!-- PANE_ESPNOW -->": "pane-espnow.html",
    "<!-- PANE_OTA -->":    "pane-ota.html",
    "<!-- PANE_DIAG -->":   "pane-diag.html",
}

JS_ORDER = ["_gpio.js", "network_protocol.js", "scoring.js", "espnow.js", "outputs.js", "app.js"]


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


# Resolution bit value lookup
RES_BIT_MAP = {
    "RES_BIT_8": 1, "RES_BIT_10": 2, "RES_BIT_12": 4, "RES_BIT_16": 8,
    "RES_BIT_24": 16, "RES_BIT_32": 32, "RES_BITS_8_10_12_16": 15,
}

# Test UI ID mapping
TEST_UI_MAP = {
    "TEST_UI_NONE": 0, "TEST_UI_SLIDER": 1, "TEST_UI_DMX": 2,
    "TEST_UI_LED": 3, "TEST_UI_MOTOR": 4, "TEST_UI_STEPPER": 5,
    "TEST_UI_DFPLAYER": 6, "TEST_UI_7SEG": 7,
}

FIELD_TYPE_MAP = {
    "FT_NUMBER": "number",
    "FT_FLOAT": "float",
    "FT_BOOL": "bool",
    "FT_SELECT": "select",
    "FT_COLOR": "color",
    "FT_TEXT": "text",
}

RES_LABELS_DMX = ["8-bit (1 DMX Ch)", "10-bit (2 DMX Ch)", "12-bit (2 DMX Ch)", "16-bit (2 DMX Ch)", "24-bit (3 DMX Ch)", "32-bit (4 DMX Ch)"]
RES_LABELS_POS = ["8-bit (1 Pos Ch)", "10-bit (2 Pos Ch)", "12-bit (2 Pos Ch)", "16-bit (2 Pos Ch)", "24-bit (3 Pos Ch)", "32-bit (4 Pos Ch)"]
RES_LABELS_7SEG = ["ASCII / Character", "Numeric (0-9, 10-19 for DP)", None, None, None, None]


def eval_res_bits(expr):
    """Evaluate resolution bitmask expression like 'RES_BIT_8|RES_BIT_10'."""
    val = 0
    for part in expr.split("|"):
        val |= RES_BIT_MAP.get(part.strip(), 0)
    return val


def parse_cpp_string(s):
    """Strip surrounding quotes from a C++ string literal."""
    return s.strip().strip('"')


def match_braces(text, start):
    """Find matching closing brace from start. Returns index of '}' or -1."""
    if text[start] != '{':
        return -1
    depth = 0
    for i in range(start, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return i
    return -1


def parse_test_commands_for_type(type_num):
    """Parse TEST_COMMANDS[] array from include/type_interfaces/type_{num}.h."""
    path = os.path.join("include", "type_interfaces", f"type_{type_num}.h")
    if not os.path.exists(path):
        return []
    src = read_file(path)
    # Find the TEST_COMMANDS array block
    m = re.search(r"constexpr\s+TestCmdDef\s+TEST_COMMANDS\[\]\s*=\s*\{(.*?)\}\s*;", src, re.DOTALL)
    if not m:
        return []
    body = m.group(1)
    cmds = []
    entry_re = re.compile(r'\{\s*"([^"]+)"\s*,\s*(\d+)\s*,\s*"([^"]*)"\s*\}')
    for label, cmd, desc in entry_re.findall(body):
        cmds.append([label, int(cmd), desc])
    return cmds


def parse_test_fields_for_type(type_num):
    """Parse TEST_FIELDS[] from include/type_interfaces/type_{num}.h."""
    path = os.path.join("include", "type_interfaces", f"type_{type_num}.h")
    if not os.path.exists(path):
        return []
    src = read_file(path)
    constants = parse_type_string_constants(src)
    m = re.search(r"constexpr\s+FieldDef\s+TEST_FIELDS\[\]\s*=\s*\{(.*?)\}\s*;", src, re.DOTALL)
    if not m:
        return []
    body = m.group(1)
    fields = []
    entry_re = re.compile(
        r'\{\s*"([^"]+)"\s*,\s*(FT_\w+)\s*,\s*"([^"]*)"\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*([^}]+?)\s*\}'
    )
    for key, field_type, label, min_val, max_val, default, opts_expr in entry_re.findall(body):
        fields.append({
            "key": key,
            "type": FIELD_TYPE_MAP.get(field_type, "number"),
            "label": label,
            "min": int(min_val),
            "max": int(max_val),
            "def": int(default),
            "opts": parse_field_options(opts_expr.rstrip(","), constants),
        })
    return fields


def parse_type_string_constants(src):
    constants = {}
    for name, value in re.findall(r'constexpr\s+const\s+char\s*\*\s*(\w+)\s*=\s*"([^"]*)"\s*;', src):
        constants[name] = value
    return constants


def parse_field_options(expr, constants):
    expr = expr.strip()
    if expr == "nullptr":
        return []
    if expr.startswith('"'):
        raw = parse_cpp_string(expr)
    else:
        raw = constants.get(expr, "")
    opts = []
    for part in raw.split(","):
        if not part:
            continue
        key, sep, label = part.partition(":")
        if not sep:
            continue
        try:
            value = int(key.strip())
        except ValueError:
            continue
        opts.append([value, label.strip()])
    return opts


def parse_extra_fields_for_type(type_num):
    """Parse EXTRA_FIELDS[] from include/type_interfaces/type_{num}.h."""
    path = os.path.join("include", "type_interfaces", f"type_{type_num}.h")
    if not os.path.exists(path):
        return []
    src = read_file(path)
    constants = parse_type_string_constants(src)
    m = re.search(r"constexpr\s+FieldDef\s+EXTRA_FIELDS\[\]\s*=\s*\{(.*?)\}\s*;", src, re.DOTALL)
    if not m:
        return []
    body = m.group(1)
    fields = []
    entry_re = re.compile(
        r'\{\s*"([^"]+)"\s*,\s*(FT_\w+)\s*,\s*"([^"]*)"\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*([^}]+?)\s*\}'
    )
    for key, field_type, label, min_val, max_val, default, opts_expr in entry_re.findall(body):
        fields.append({
            "key": key,
            "type": FIELD_TYPE_MAP.get(field_type, "number"),
            "label": label,
            "min": int(min_val),
            "max": int(max_val),
            "def": int(default),
            "opts": parse_field_options(opts_expr.rstrip(","), constants),
        })
    return fields


def generate_score_limits_js():
    """Parse scoring_limits.h, dimmer.h, scoring.h → const SCORE_LIMITS."""
    out = {}
    # scoring_limits.h: constexpr values
    src_limits = read_file(SCORE_LIMITS_PATH)
    for name, val in re.findall(r"constexpr\s+\w+\s+(\w+)\s*=\s*([^;]+)\s*;", src_limits):
        try:
            val = eval(val.replace("UL", "").replace("u", "").replace("U", ""))
            if isinstance(val, (int, float)):
                out[name] = int(val)
        except Exception:
            pass

    # dimmer.h: DIMMER_TICK_US #define
    src_dimmer = read_file(DIMMER_PATH)
    m = re.search(r"#define\s+DIMMER_TICK_US\s+(\d+)", src_dimmer)
    if m:
        out["DIMMER_TICK_US"] = int(m.group(1))

    # scoring.h: RMT_DMX_DRIVER_RAM (computed)
    src_scoring = read_file(SCORING_PATH)
    m = re.search(
        r"constexpr\s+uint32_t\s+RMT_DMX_DRIVER_RAM\s*=\s*(\d+)UL\s*\*\s*sizeof\s*\(\s*rmt_item32_t\s*\)\s*\+\s*(\d+)",
        src_scoring,
    )
    if m:
        out["RMT_DMX_DRIVER_RAM"] = int(m.group(1)) * 4 + int(m.group(2))

    # Domain constants not yet in headers
    out["FUNCGEN_MIN_PERIOD_US"] = 50
    out["FUNCGEN_ISR_US"] = 4
    out["ESPNOW_BASE_US"] = 500
    out["ESPNOW_PER_CHUNK_US"] = 170
    out["ESPNOW_PER_UNIVERSE_US"] = 100
    out["ESPNOW_OVERHEAD_BYTES"] = 44
    out["STEPPER_POSITION_BYTES"] = 2
    out["AC_DIMMER_TIMER_COUNT"] = 1
    out["DEFAULT_OUTPUT_FPS"] = 40

    return "const SCORE_LIMITS=" + json.dumps(out, separators=(",", ":")) + ";\n"


def generate_output_defs_js():
    src = read_file(OUTPUT_DEFS_PATH)

    # Type ID constants
    type_ids = {
        name: int(value)
        for name, value in re.findall(r"constexpr\s+uint8_t\s+(TYPE_\w+)\s*=\s*(\d+)\s*;", src)
    }

    # HardwareCost constants
    hardware = {}
    for name, ledc, rmt, uart, dac, timer in re.findall(
        r"constexpr\s+HardwareCost\s+(HW_\w+)\s*=\s*\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}\s*;",
        src,
    ):
        hardware[name] = {"ledc": int(ledc), "rmt": int(rmt), "uart": int(uart), "dac": int(dac), "timer": int(timer)}

    cost_flags = {"CF_NONE": 0}
    for name, value in re.findall(r"(CF_\w+)\s*=\s*1\s*<<\s*(\d+)", src):
        cost_flags[name] = 1 << int(value)

    def eval_cost_flags(expr):
        if not expr:
            return 0
        total = 0
        for part in expr.split("|"):
            token = part.strip()
            if not token:
                continue
            if token in cost_flags:
                total |= cost_flags[token]
            else:
                try:
                    total |= int(token, 0)
                except ValueError:
                    pass
        return total

    # ModeCost constants
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
            "flags": eval_cost_flags(args[6]) if len(args) > 6 else 0,
        }

    # PinRule arrays
    pin_sets = {}
    for name, body in re.findall(r"constexpr\s+PinRule\s+(PINS_\w+)\[\]\s*=\s*\{(.*?)\}\s*;", src, re.DOTALL):
        pin_sets[name] = []
        for slot, label, sources, direction, invert, hw in re.findall(
            r'\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*(.*?)\s*,\s*(PIN_OUTPUT|PIN_INPUT)\s*,\s*(true|false)\s*,\s*(\d+)\s*\}',
            body,
        ):
            pin_sets[name].append({
                "slot": slot,
                "label": label,
                "sources": parse_source_mask(sources),
                "dir": "out" if direction == "PIN_OUTPUT" else "in",
                "invert": invert == "true",
                "hwIfGpio": int(hw),
            })

    # Pre-parse type interface metadata for all types 0-18
    test_cmds_by_type = {}
    fields_by_type = {}
    for tn in range(19):
        cmd_list = parse_test_commands_for_type(tn)
        if cmd_list:
            test_cmds_by_type[tn] = cmd_list
        fields_by_type[tn] = parse_extra_fields_for_type(tn)

    # Parse OUTPUT_MODES array using brace matching
    defs = {}
    mode_key_map = {}
    test_ui_per_type = [0] * 19

    modes_body_m = re.search(r"constexpr\s+OutputModeDef\s+OUTPUT_MODES\[\]\s*=\s*\{(.*?)\n\};", src, re.DOTALL)
    if not modes_body_m:
        raise RuntimeError("Cannot find OUTPUT_MODES[] array")
    modes_body = modes_body_m.group(1)

    i = 0
    while i < len(modes_body):
        brace_start = modes_body.find('{', i)
        if brace_start == -1:
            break
        brace_end = match_braces(modes_body, brace_start)
        if brace_end == -1:
            break
        inner = modes_body[brace_start + 1:brace_end]
        args = split_cpp_args(inner)
        if len(args) < 13:
            i = brace_end + 1
            continue

        type_ident = args[0].strip()
        mode_val = int(args[1].strip())
        name = parse_cpp_string(args[2])
        mode_key_str = parse_cpp_string(args[3])
        res_bits = eval_res_bits(args[4].strip())
        slot_mask = int(args[5].strip())
        seg_layout = args[6].strip() == 'true'
        test_ui = TEST_UI_MAP.get(args[7].strip(), 0)
        test_cmds_ref = args[8].strip()
        test_cmd_count_str = args[9].strip()
        cost_ident = args[10].strip()
        pins_ident = args[11].strip()
        pin_count = int(args[12].strip())
        start_at_first_universe = len(args) > 13 and args[13].strip() == 'true'
        segment_count = int(args[14].strip()) if len(args) > 14 else 0
        primary_route_is_segment = len(args) > 15 and args[15].strip() == 'true'

        type_id = type_ids.get(type_ident, -1)
        if type_id < 0:
            i = brace_end + 1
            continue

        # Build OUTPUT_DEFS
        entry = defs.setdefault(str(type_id), {"name": name, "modes": {}})
        pins = {}
        for pin in pin_sets.get(pins_ident, [])[:pin_count]:
            pins[pin["slot"]] = {k: v for k, v in pin.items() if k != "slot"}

        effective_mode_key = mode_key_str if mode_key_str else str(mode_val)
        entry["modes"][effective_mode_key] = {
            "label": name,
            "cost": costs.get(cost_ident, {}),
            "pins": pins,
            "resolutionBits": res_bits,
            "slotActiveMask": slot_mask,
            "segmentLayout": seg_layout,
            "startAtFirstUniverse": start_at_first_universe,
            "segmentCount": segment_count,
            "primaryRouteIsSegment": primary_route_is_segment,
        }

        # Build modeKeyMap
        type_str = str(type_id)
        if type_str not in mode_key_map:
            mode_key_map[type_str] = {}
        mode_key_map[type_str][str(mode_val)] = effective_mode_key

        # Store test UI per type (use highest number for types with multiple modes)
        if test_ui > test_ui_per_type[type_id]:
            test_ui_per_type[type_id] = test_ui

        i = brace_end + 1

    # Parse TYPE_DISPLAY_NAMES
    display_names = []
    m = re.search(r"constexpr\s+const\s+char\s*\*\s*TYPE_DISPLAY_NAMES\[\]\s*=\s*\{(.*?)\};", src, re.DOTALL)
    if m:
        for match in re.finditer(r'"([^"]+)"', m.group(1)):
            display_names.append(match.group(1))
    types_js = {str(i): name for i, name in enumerate(display_names)}

    # Parse per-type metadata arrays
    def parse_uint16_array(name):
        m = re.search(rf"constexpr\s+uint16_t\s+{re.escape(name)}\[\]\s*=\s*\{{(.*?)}}\s*;", src, re.DOTALL)
        if not m:
            return []
        body = re.sub(r"//.*", "", m.group(1))
        return [int(v.strip()) for v in body.split(",") if v.strip()]

    config_labels = []
    m = re.search(r"constexpr\s+const\s+char\s*\*\s*TYPE_CONFIG_LABELS\[\]\s*=\s*\{(.*?)\};", src, re.DOTALL)
    if m:
        for match in re.finditer(r'"([^"]*)"', m.group(1)):
            config_labels.append(match.group(1))

    channel_counts = parse_uint16_array("TYPE_CHANNEL_COUNTS")
    byte_counts = parse_uint16_array("TYPE_BYTE_COUNTS")

    # Build resolution options per type from resolution bits
    res_opts = []
    for tn in range(19):
        bits = 0
        type_str = str(tn)
        if type_str in defs:
            for mk, mode_entry in defs[type_str]["modes"].items():
                bits |= mode_entry.get("resolutionBits", 0)
        label_set = RES_LABELS_7SEG if tn in (11, 12, 13) else (RES_LABELS_POS if tn == 7 else RES_LABELS_DMX)
        opts = []
        for bit_idx in range(6):
            if bits & (1 << bit_idx):
                label = label_set[bit_idx]
                if label:
                    res_val = [8, 10, 12, 16, 24, 32][bit_idx]
                    opts.append([res_val, label])
        res_opts.append(opts if opts else None)

    # Build test commands data per type
    test_cmds_js = {}
    for tn in range(19):
        if tn in test_cmds_by_type:
            test_cmds_js[str(tn)] = test_cmds_by_type[tn]

    fields_js = {}
    for tn in range(19):
        fields_js[str(tn)] = fields_by_type.get(tn, [])

    test_fields_js = {}
    for tn in range(19):
        test_fields_js[str(tn)] = parse_test_fields_for_type(tn)

    meta = {
        "configLabels": config_labels if len(config_labels) == 19 else [],
        "channelCounts": channel_counts if len(channel_counts) == 19 else [],
        "byteCounts": byte_counts if len(byte_counts) == 19 else [],
        "modeKeyMap": mode_key_map,
        "testUi": test_ui_per_type,
        "testFields": test_fields_js,
        "testCmds": test_cmds_js,
        "fields": fields_js,
        "resOpts": res_opts,
        "typeIds": {name.replace("TYPE_", ""): val for name, val in type_ids.items()},
        "costFlags": cost_flags,
    }

    return (
        "const OUTPUT_DEFS=" + json.dumps(defs, separators=(",", ":")) + ";\n"
        "const TYPES=" + json.dumps(types_js, separators=(",", ":")) + ";\n"
        "const TYPE_META=" + json.dumps(meta, separators=(",", ":")) + ";\n"
    )


def generate_gpio_js():
    """Parse gpio_control.h → GPIO_PINS const for JS."""
    src = read_file(GPIO_CONTROL_PATH)

    def extract_array(name):
        m = re.search(rf"constexpr\s+uint8_t\s+{re.escape(name)}\[\]\s*=\s*\{{([^}}]+)}}\s*;", src)
        if not m:
            raise RuntimeError(f"Cannot find {name}[] in {GPIO_CONTROL_PATH}")
        return [int(v.strip()) for v in m.group(1).split(",") if v.strip()]

    output_pins = extract_array("OUTPUT_GPIO_PINS")
    input_pins = extract_array("INPUT_GPIO_PINS")
    input_only_pins = extract_array("INPUT_ONLY_PINS")

    reserved = []
    for m in re.finditer(r'\{\s*(\d+)\s*,\s*"([^"]+)"\s*\}', src):
        # Only match inside RESERVED_ETHERNET_PINS block
        pin = int(m.group(1))
        reason = m.group(2)
        reserved.append({"pin": pin, "reason": reason})

    data = {
        "output": output_pins,
        "input": input_pins,
        "inputOnly": input_only_pins,
        "reserved": reserved,
    }
    return "const GPIO_PINS=" + json.dumps(data, separators=(",", ":")) + ";\n"


def generate_source_rules_js():
    """Parse source_rules.h → SOURCE_DATA const for JS."""
    src = read_file(SOURCE_RULES_PATH)

    # SOURCE_NAMES array
    names_match = re.search(r'constexpr\s+const\s+char\s*\*\s*SOURCE_NAMES\[\]\s*=\s*\{(.*?)\}\s*;', src, re.DOTALL)
    names = []
    if names_match:
        for m in re.finditer(r'"([^"]+)"', names_match.group(1)):
            names.append(m.group(1))

    # Source mask constants (SRC_GPIO=1, SRC_PCA=2, ...)
    masks = {}
    for m in re.finditer(r'(SRC_\w+)\s*=\s*1\s*<<\s*(\d+)', src):
        name = m.group(1).replace("SRC_", "")
        masks[name] = 1 << int(m.group(2))

    # ADDRESS_RULES array
    rules = []
    for m in re.finditer(
        r'\{\s*(\d+)\s*,\s*(SRC_\w+)\s*,\s*"([^"]+)"\s*,\s*\{\s*\{\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\}\s*,\s*\{\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\}\s*\}\s*\}',
        src,
    ):
        source = int(m.group(1))
        mask_name = m.group(2).replace("SRC_", "")
        mask_val = masks.get(mask_name, 0)
        label = m.group(3)
        r1_min = int(m.group(4), 16 if m.group(4).startswith("0x") else 10)
        r1_max = int(m.group(5), 16 if m.group(5).startswith("0x") else 10)
        r2_min = int(m.group(6), 16 if m.group(6).startswith("0x") else 10)
        r2_max = int(m.group(7), 16 if m.group(7).startswith("0x") else 10)
        ranges = [[r1_min, r1_max]]
        if r2_min != 0 or r2_max != 0:
            ranges.append([r2_min, r2_max])
        rules.append({"source": source, "mask": mask_val, "label": label, "ranges": ranges})

    data = {
        "names": names,
        "addressRules": rules,
        "masks": masks,
    }
    return "const SOURCE_DATA=" + json.dumps(data, separators=(",", ":")) + ";\n"


def generate_display_js():
    src = read_file(DISPLAY_PROTO_PATH)

    def parse_addr_array(name):
        m = re.search(rf"constexpr\s+uint8_t\s+{re.escape(name)}\[\]\s*=\s*\{{(.*?)\}}\s*;", src, re.DOTALL)
        if not m:
            return []
        return [int(v.strip(), 0) for v in m.group(1).split(",") if v.strip()]

    type_ids = {
        name.replace("DTYPE_", ""): int(value)
        for name, value in re.findall(r"constexpr\s+uint8_t\s+(DTYPE_\w+)\s*=\s*(\d+)\s*;", src)
    }
    type_names = []
    m = re.search(r"constexpr\s+const\s+char\s*\*\s*TYPE_NAMES\[\]\s*=\s*\{(.*?)\};", src, re.DOTALL)
    if m:
        type_names = re.findall(r'"([^"]+)"', m.group(1))

    data = {
        "typeIds": type_ids,
        "typeNames": type_names,
        "addresses": {
            str(type_ids.get("SSD1306", 1)): parse_addr_array("SSD1306_ADDRS"),
            str(type_ids.get("SH1106", 2)): parse_addr_array("SH1106_ADDRS"),
            str(type_ids.get("LCD", 3)): parse_addr_array("LCD_ADDRS"),
        },
    }
    return "const DISPLAY_DATA=" + json.dumps(data, separators=(",", ":")) + ";\n"


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

    # Replace per-type config marker with a single metadata-rendered container.
    marker = "<!-- CONFIG_TYPE_FIELDS -->"
    if marker not in html:
        print(f"Warning: marker {marker} not found in shell, skipping")
    else:
        html = html.replace(marker, '<div id="type-config-generated" class="type-config" style="display:none"></div>')

    # Replace generic test panel marker
    test_panel_path = os.path.join(PARTS_DIR, "pane-test.html")
    if os.path.exists(test_panel_path):
        html = html.replace("<!-- PANE_TEST -->", read_file(test_panel_path))
    else:
        print(f"Warning: {test_panel_path} not found, leaving PANE_TEST marker")

    # Embed JS files into <script> tag (generated metadata, main JS, then per-type config/test)
    js_parts = [generate_score_limits_js(), generate_gpio_js(), generate_source_rules_js(), generate_display_js(), generate_output_defs_js()]
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
