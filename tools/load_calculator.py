#!/usr/bin/env python3
import os
import sys
import json
import math

# Force UTF-8 encoding on stdout/stderr to support emojis/Unicode on Windows terminals
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')
if hasattr(sys.stderr, 'reconfigure'):
    sys.stderr.reconfigure(encoding='utf-8')

# Hardware limits matching include/scoring.h
MAX_LEDC = 16
MAX_RMT = 8
MAX_UART = 2
MAX_DAC = 2
MAX_TIMER = 4
RAM_LIMIT = 65535 # 64 KB cap

BASE_CHANNEL_RAM = 224
DMX_BUFFER_RAM = 512
PIXEL_STRIP_OBJECT_RAM = 256
DFPLAYER_OBJECT_RAM = 160
STEPPER_RUNTIME_RAM = 512
FUNCGEN_OBJECT_RAM = 1120
RMT_DMX_DRIVER_RAM = 5150 * 4 + 32
I2C_ROUTE_RAM = 32

BASE_OVERHEAD_US = 500
I2C_WRITE_US = 180

def print_banner():
    print("=====================================================================")
    print("        WT32-ETH01 / ESP32 Hardware Resource Calculator (v3)         ")
    print("=====================================================================")

def check_pin_safety(pin, status_led=5, zc_pin=255, role="Output", sda_pin=14, scl_pin=15):
    """
    Checks if a GPIO pin is safe to use on WT32-ETH01.
    """
    if pin == 255:
        return True, "Unused"
    
    # Internal Ethernet pins for LAN8720 on WT32-ETH01
    eth_pins = {0, 16, 18, 19, 21, 22, 23, 25, 26, 27}
    # Debug serial pins (UART0)
    serial_pins = {1, 3}
    # Standard exposed output pins on WT32-ETH01
    safe_output_pins = {2, 4, 12, 14, 15, 17, 32, 33}
    # Standard exposed input-only pins on WT32-ETH01
    input_only_pins = {34, 35, 36, 39}
    
    if pin in eth_pins:
        return False, f"❌ CRITICAL ERROR: GPIO {pin} is reserved for Ethernet LAN8720. Using it will crash Ethernet!"
    if pin in serial_pins:
        return False, f"⚠️ WARNING: GPIO {pin} is UART0 Serial TX/RX (USB debug). Using it may interfere with flashing/logs."
    if pin == status_led:
        return False, f"❌ CONFLICT: GPIO {pin} is configured as the Status LED pin."
    if pin == zc_pin and zc_pin != 255:
        return False, f"❌ CONFLICT: GPIO {pin} is configured as the Zero-Crossing interrupt pin."
    if pin == sda_pin or pin == scl_pin:
        return False, f"❌ CONFLICT: GPIO {pin} is configured as a shared I2C bus pin (SDA/SCL)."
    
    if role == "Input":
        if pin in input_only_pins or pin in safe_output_pins:
            return True, "Safe Input"
        else:
            return False, f"⚠️ WARNING: GPIO {pin} is not standard exposed pin on WT32-ETH01. Verify hardware pinout."
    else:
        if pin in input_only_pins:
            return False, f"❌ CRITICAL: GPIO {pin} is an INPUT-ONLY pin (GPI). It cannot be used as an output role: '{role}'!"
        if pin in safe_output_pins:
            return True, "Safe Output"
        return False, f"⚠️ WARNING: GPIO {pin} is not in the standard exposed output pin list for WT32-ETH01. Verify hardware."

def parse_json_outputs(filepath):
    """
    Parses outputs.json and returns channels list.
    """
    if not os.path.exists(filepath):
        return None
    try:
        with open(filepath, 'r') as f:
            data = json.load(f)
            return data.get("outputs", [])
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return None

def estimate_hardware(ch):
    """
    Estimates hardware peripheral counts matching scoring.h::estimateHardware()
    """
    ch_type = ch.get("type", 0)
    source = ch.get("source", 0)
    
    ledc = 0
    rmt = 0
    uart = 0
    dac = 0
    timer = 0
    
    if ch_type == 0:
        pass
    elif ch_type == 1:
        uart = 1
    elif ch_type == 2:
        pass
    elif ch_type == 3:
        rmt = 1
    elif ch_type == 4:
        if source == 0: ledc = 1
    elif ch_type == 5:
        if source == 0: ledc += 1
        if ch.get("pin2_source", source) == 0: ledc += 1
        if ch.get("pin3_source", source) == 0: ledc += 1
        if ch.get("color_order", 0) >= 4 and ch.get("pin4_source", source) == 0: ledc += 1
    elif ch_type == 6:
        mc_mode = ch.get("mc_mode", 0)
        if source == 0: ledc += 1
        if ch.get("pin2_source", source) == 0 and mc_mode == 0: ledc += 1
        if mc_mode == 2 and ch.get("pin3_source", source) == 0: ledc += 1
    elif ch_type == 7:
        rmt = 2
    elif ch_type == 8:
        if source == 0: ledc = 1
    elif ch_type == 9:
        if source == 0: ledc = 1
    elif ch_type == 10:
        uart = 1
    elif ch_type == 11:
        pass
    elif ch_type in (12, 13):
        seg_n = 8 if ch_type == 13 else 7
        seg_sources = ch.get("seg_sources", [])
        for i in range(seg_n):
            s_src = seg_sources[i] if i < len(seg_sources) else source
            if s_src == 0: ledc += 1
    elif ch_type == 14:
        if source != 5: dac = 1
    elif ch_type == 15:
        if source == 0: ledc = 1
    elif ch_type == 16:
        ledc = 1
        timer = 1
        
    return {"ledc": ledc, "rmt": rmt, "uart": uart, "dac": dac, "timer": timer}

def src_on_i2c(src):
    return src >= 1 and src <= 5

def i2c_writes_for_channel(ch):
    ch_type = ch.get("type", 0)
    source = ch.get("source", 0)
    mode = ch.get("mc_mode", 0)
    writes = 0
    
    if ch_type in (2, 17):
        if src_on_i2c(source): writes += 1
    elif ch_type in (4, 8, 15):
        if src_on_i2c(source): writes += 1
    elif ch_type == 5:
        if src_on_i2c(source): writes += 1
        if src_on_i2c(ch.get("pin2_source", source)): writes += 1
        if src_on_i2c(ch.get("pin3_source", source)): writes += 1
        if ch.get("color_order", 0) >= 4 and src_on_i2c(ch.get("pin4_source", source)): writes += 1
    elif ch_type == 6:
        if src_on_i2c(source):
            writes += 3 if mode == 2 else 2
        else:
            if src_on_i2c(ch.get("pin2_source", source)): writes += 1
            if mode == 2 and src_on_i2c(ch.get("pin3_source", source)): writes += 1
    elif ch_type == 7:
        if src_on_i2c(ch.get("pin2_source", source)): writes += 1
        if src_on_i2c(ch.get("pin3_source", source)): writes += 1
    elif ch_type in (12, 13):
        n = 8 if ch_type == 13 else 7
        pin2_source = ch.get("pin2_source", source)
        if mode >= 2 and src_on_i2c(pin2_source):
            writes += n
        else:
            seg_sources = ch.get("seg_sources", [])
            for i in range(n):
                s_src = seg_sources[i] if i < len(seg_sources) else source
                if src_on_i2c(s_src): writes += 1
        if mode >= 6 and mode <= 9 and src_on_i2c(source): writes += 1
    elif ch_type == 14:
        if src_on_i2c(source): writes += 1
    elif ch_type == 18:
        if src_on_i2c(source): writes += 1
        if src_on_i2c(ch.get("pin2_source", source)): writes += 1
    return writes

def value_byte_count(res):
    if res == 16: return 2
    if res == 12 or res == 10: return 2
    return 1

def dmx_buffer_ram_for_channel(ch_type, led_count, color_order, resolution, mode):
    if ch_type == 1:
        return DMX_BUFFER_RAM
    elif ch_type == 3:
        bytes_per_pixel = 4 if color_order >= 4 else 3
        pixels_per_universe = 512 // bytes_per_pixel
        universes = (led_count + pixels_per_universe - 1) // pixels_per_universe
        if universes < 1: universes = 1
        return universes * 512
    elif ch_type == 5:
        return 4 if color_order >= 4 else 3
    elif ch_type == 7:
        return value_byte_count(resolution) + 2
    elif ch_type in (9, 11):
        return 4 if mode == 1 else 2
    elif ch_type in (12, 13):
        return 2 if (mode == 4 or mode == 5 or mode >= 6) else 1
    elif ch_type == 10:
        return 3
    elif ch_type == 16:
        return 5
    elif ch_type in (4, 6, 8, 15):
        return value_byte_count(resolution)
    return 1

def pixel_buffer_ram(led_count, color_order):
    return led_count * (4 if color_order >= 4 else 3)

def led_strip_service_us(led_count, color_order):
    bytes_per_pixel = 4 if color_order >= 4 else 3
    return 80 + led_count * bytes_per_pixel

def estimate_channel_cost(ch):
    ch_type = ch.get("type", 0)
    led_count = ch.get("led_count", 0)
    color_order = ch.get("color_order", 0)
    resolution = ch.get("mc_resolution", 8)
    mode = ch.get("mc_mode", 0)
    
    ram = BASE_CHANNEL_RAM + dmx_buffer_ram_for_channel(ch_type, led_count, color_order, resolution, mode)
    cpu = 0
    
    if ch_type == 0: cpu = 5
    elif ch_type == 1: cpu = 22600
    elif ch_type == 2: cpu = 5
    elif ch_type == 3:
        cpu = led_strip_service_us(led_count, color_order)
        ram += pixel_buffer_ram(led_count, color_order) + PIXEL_STRIP_OBJECT_RAM
    elif ch_type == 4: cpu = 6
    elif ch_type == 5: cpu = 18
    elif ch_type == 6: cpu = 35
    elif ch_type == 7:
        cpu = 80
        ram += STEPPER_RUNTIME_RAM
    elif ch_type == 8: cpu = 12
    elif ch_type == 9: cpu = 35
    elif ch_type == 10:
        cpu = 30
        ram += DFPLAYER_OBJECT_RAM + 100
    elif ch_type == 11: cpu = 900
    elif ch_type == 12: cpu = 30
    elif ch_type == 13: cpu = 35
    elif ch_type == 14: cpu = 10
    elif ch_type == 15: cpu = 6
    elif ch_type == 16:
        cpu = 120
        ram += FUNCGEN_OBJECT_RAM
    elif ch_type == 17: cpu = 10
    elif ch_type == 18: cpu = 25
    
    writes = i2c_writes_for_channel(ch)
    cpu += writes * I2C_WRITE_US
    ram += writes * I2C_ROUTE_RAM
    return {"cpu": cpu, "ram": ram}

def frame_time_us(fps):
    return 1000000 // fps

def ac_dimmer_background_us(dimmer_count, fps):
    if dimmer_count == 0: return 0
    ticks = frame_time_us(fps) // 39
    return ticks * (1 + dimmer_count)

def func_gen_background_us(func_gen_count, fps):
    if func_gen_count == 0: return 0
    samples = frame_time_us(fps) // 50
    return samples * 4 * func_gen_count

def espnow_master_cost(peer_count, universe_count, chunk_size=200):
    chunks_per_uni = (511 + chunk_size) // chunk_size
    cpu = 500 + peer_count * chunks_per_uni * 170 + universe_count * 100
    ram = 512 + peer_count * (chunk_size + 44)
    return {"cpu": cpu, "ram": ram}

def get_espnow_peer_count():
    locations = ["espnow_peers.json", "data/espnow_peers.json", "../data/espnow_peers.json", "c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/data/espnow_peers.json"]
    for loc in locations:
        if os.path.exists(loc):
            try:
                with open(loc, 'r') as f:
                    d = json.load(f)
                    peers = d.get("peers", [])
                    return len(peers) if len(peers) > 0 else 1
            except:
                pass
    return 0

def run_calculation_on_channels(outputs, status_led=5, zc_pin=255, sda_pin=14, scl_pin=15, output_fps=40, is_master=False):
    """
    Performs resource analysis on a list of channel configurations using the 3 budgets model.
    """
    print(f"\nAnalyzing {len(outputs)} configured channels under 3 budgets model...")
    
    # Initialize accumulators
    total_ledc = 0
    total_rmt = 0
    total_uart = 0
    total_dac = 0
    total_timer = 0
    
    total_cpu_us = BASE_OVERHEAD_US
    total_ram_bytes = 0
    
    dimmer_count = 0
    funcgen_count = 0
    dmx_gpio_count = 0
    dfplayer_count = 0
    
    gpio_usage = {}
    pca9685_usage = {}
    other_expanders = {}
    
    src_names = {
        0: "ESP32 GPIO", 1: "PCA9685", 2: "MCP23017", 3: "TCA9555", 4: "PCF857x", 5: "MCP4725"
    }
    
    type_names = {
        0: "AC Dimmer", 1: "DMX Output", 2: "Relay", 3: "RGB LED", 4: "Single LED",
        5: "Analog RGB", 6: "Motor", 7: "Stepper", 8: "Servo", 9: "Buzzer",
        10: "DFPlayer", 11: "7-Seg TM1637", 12: "7-Seg DD 7-Pin PWM", 13: "7-Seg DD 8-Pin PWM",
        14: "DAC", 15: "PWM DAC", 16: "Func Gen", 17: "Solenoid", 18: "Smoke Shooter"
    }
    
    warnings = []
    errors = []
    
    has_dimmer = False
    unique_universes = set()
    
    # Process each channel
    for idx, ch in enumerate(outputs):
        ch_type = ch.get("type", 0)
        ch_name = type_names.get(ch_type, f"Unknown ({ch_type})")
        source = ch.get("source", 0)
        pin = ch.get("pin", 255)
        pin2 = ch.get("pin2", 255)
        pin3 = ch.get("pin3", 255)
        pin4 = ch.get("pin4", 255)
        
        pin2_source = ch.get("pin2_source", source)
        pin3_source = ch.get("pin3_source", source)
        pin4_source = ch.get("pin4_source", source)
        
        unique_universes.add(ch.get("start_universe", 0))
        
        # 1. Accumulate hardware peripherals
        hw = estimate_hardware(ch)
        total_ledc += hw["ledc"]
        total_rmt += hw["rmt"]
        total_uart += hw["uart"]
        total_dac += hw["dac"]
        total_timer += hw["timer"]
        
        if ch_type == 0:
            has_dimmer = True
            dimmer_count += 1
        elif ch_type == 16:
            funcgen_count += 1
            
        if ch_type == 1 and source == 0:
            dmx_gpio_count += 1
        elif ch_type == 10:
            dfplayer_count += 1
            
        # 2. Accumulate active costs (CPU & RAM)
        cost = estimate_channel_cost(ch)
        total_cpu_us += cost["cpu"]
        total_ram_bytes += cost["ram"]
        
        # 3. Track physical pins/channels for overlap check
        pins_to_check = [
            ("pin", "Primary Pin", pin, source, ch.get("pca_addr", 0x40), ch.get("pca_channel", 0)),
            ("pin2", "Secondary Pin", pin2, pin2_source, ch.get("pin2_addr", 0x20), ch.get("pca_channel2", 255)),
            ("pin3", "Tertiary Pin", pin3, pin3_source, ch.get("pin3_addr", 0x20), ch.get("pca_channel3", 255)),
            ("pin4", "Quaternary Pin", pin4, pin4_source, ch.get("pin4_addr", 0x20), ch.get("pca_channel4", 255)),
        ]
        
        if ch_type in (12, 13):
            num_seg = 8 if ch_type == 13 else 7
            seg_pins = ch.get("seg_pins", [])
            seg_sources = ch.get("seg_sources", [])
            seg_addrs = ch.get("seg_addrs", [])
            for s in range(num_seg):
                s_pin = seg_pins[s] if s < len(seg_pins) else 255
                s_src = seg_sources[s] if s < len(seg_sources) else source
                s_addr = seg_addrs[s] if s < len(seg_addrs) else 0x20
                pins_to_check.append((f"seg_pins[{s}]", f"Segment {s} Pin", s_pin, s_src, s_addr, s))
                
        i2c_bus_used = False
        for field, role_name, pin_val, pin_src, pin_addr, ch_idx in pins_to_check:
            if pin_val == 255:
                continue
            role_desc = f"{ch_name} (Ch{idx+1} {role_name})"
            
            if pin_src == 0:
                if pin_val not in gpio_usage:
                    gpio_usage[pin_val] = []
                gpio_usage[pin_val].append((idx, role_desc))
            else:
                i2c_bus_used = True
                if pin_src == 1:
                    if pin_addr not in pca9685_usage:
                        pca9685_usage[pin_addr] = {"channels": {}, "freqs": set()}
                    target_freq = ch.get("mc_freq", 50 if ch_type == 8 else 1000)
                    pca9685_usage[pin_addr]["freqs"].add(target_freq)
                    if ch_idx != 255:
                        if ch_idx not in pca9685_usage[pin_addr]["channels"]:
                            pca9685_usage[pin_addr]["channels"][ch_idx] = []
                        pca9685_usage[pin_addr]["channels"][ch_idx].append((idx, role_desc))
                else:
                    if pin_src not in other_expanders:
                        other_expanders[pin_src] = {}
                    if pin_addr not in other_expanders[pin_src]:
                        other_expanders[pin_src][pin_addr] = {}
                    if ch_idx == 255:
                        ch_idx = pin_val
                    if ch_idx not in other_expanders[pin_src][pin_addr]:
                        other_expanders[pin_src][pin_addr][ch_idx] = []
                    other_expanders[pin_src][pin_addr][ch_idx].append((idx, role_desc))

        if i2c_bus_used:
            for pin_val, pin_role in [(sda_pin, "I2C SDA"), (scl_pin, "I2C SCL")]:
                if pin_val not in gpio_usage:
                    gpio_usage[pin_val] = []
                gpio_usage[pin_val].append((idx, f"Shared {pin_role}"))

    # Extra shared dimmer timer
    if has_dimmer:
        total_timer += 1
        
    # Check background timer execution costs
    total_cpu_us += ac_dimmer_background_us(dimmer_count, output_fps)
    total_cpu_us += func_gen_background_us(funcgen_count, output_fps)
    
    # Calculate RMT DMX fallbacks
    free_uarts = max(0, 2 - dfplayer_count)
    dmx_rmt_use = max(0, dmx_gpio_count - free_uarts)
    total_ram_bytes += dmx_rmt_use * RMT_DMX_DRIVER_RAM
    
    # ESP-NOW Master check
    peer_count = get_espnow_peer_count()
    if is_master or peer_count > 0:
        actual_peers = peer_count if peer_count > 0 else 1
        now_cost = espnow_master_cost(actual_peers, len(unique_universes))
        total_cpu_us += now_cost["cpu"]
        total_ram_bytes += now_cost["ram"]
        print(f" 📡 ESP-NOW Master mode active. Added overhead for {actual_peers} peers.")

    # 4. Check budgets and write results
    cpu_limit_us = (1000000 // output_fps) - 1500
    
    print("\n--- BUDGET STATUS REPORT ---")
    
    print(f"1. HardwareResource Peripherals:")
    print(f" - LEDC PWM : {total_ledc:2d} / {MAX_LEDC} {'❌ OVERLIMIT' if total_ledc > MAX_LEDC else '✅ OK'}")
    print(f" - RMT      : {total_rmt:2d} / {MAX_RMT} {'❌ OVERLIMIT' if total_rmt > MAX_RMT else '✅ OK'}")
    print(f" - UART     : {total_uart:2d} / {MAX_UART} {'❌ OVERLIMIT' if total_uart > MAX_UART else '✅ OK'}")
    print(f" - DAC (Int): {total_dac:2d} / {MAX_DAC} {'❌ OVERLIMIT' if total_dac > MAX_DAC else '✅ OK'}")
    print(f" - Timers   : {total_timer:2d} / {MAX_TIMER} {'❌ OVERLIMIT' if total_timer > MAX_TIMER else '✅ OK'}")
    
    print(f"2. CpuBudget (Output Service Time):")
    print(f" - Estimate : {total_cpu_us} µs/frame")
    print(f" - Limit    : {cpu_limit_us} µs/frame (at {output_fps} FPS)")
    print(f" - Status   : {'❌ OVERLIMIT' if total_cpu_us > cpu_limit_us else '✅ OK'}")
    
    print(f"3. RamBudget (Static Allocation):")
    print(f" - Estimate : {total_ram_bytes} bytes ({(total_ram_bytes/1024):.2f} KB)")
    print(f" - Limit    : {RAM_LIMIT} bytes ({RAM_LIMIT/1024:.0f} KB)")
    print(f" - Status   : {'❌ OVERLIMIT' if total_ram_bytes > RAM_LIMIT else '✅ OK'}")

    # Safety checks
    if total_ledc > MAX_LEDC: errors.append("❌ LEDC limit exceeded")
    if total_rmt > MAX_RMT: errors.append("❌ RMT limit exceeded")
    if total_uart > MAX_UART: errors.append("❌ UART limit exceeded")
    if total_dac > MAX_DAC: errors.append("❌ DAC limit exceeded")
    if total_timer > MAX_TIMER: errors.append("❌ Timer limit exceeded")
    if total_cpu_us > cpu_limit_us: errors.append("❌ CPU compute budget exceeded")
    if total_ram_bytes > RAM_LIMIT: errors.append("❌ RAM budget exceeded")

    # Verify physical GPIOs
    print("\nChecking ESP32 physical GPIO allocations...")
    for pin, usages in sorted(gpio_usage.items()):
        role_desc = ", ".join([u[1] for u in usages])
        print(f" - GPIO {pin:2d}: Used by {role_desc}")
        
        is_input = any("LIMIT" in u[1] or "Zero-Crossing" in u[1] or "Sensor" in u[1] for u in usages)
        role_label = "Input" if is_input else "Output"
        safe, msg = check_pin_safety(pin, status_led, zc_pin, role_label, sda_pin, scl_pin)
        if not safe:
            if "CRITICAL" in msg or "CONFLICT" in msg:
                errors.append(f"❌ GPIO Pin Conflict: Pin {pin} used as {role_label} for {role_desc}. Details: {msg}")
            else:
                warnings.append(f"⚠️ GPIO Pin Warning: Pin {pin} used as {role_label} for {role_desc}. Details: {msg}")
                
        unique_channels = {u[0] for u in usages}
        is_shared_allowable = any("I2C" in u[1] or "Zero-Crossing" in u[1] for u in usages)
        if len(unique_channels) > 1 and not is_shared_allowable:
            errors.append(f"❌ GPIO Pin Collision: GPIO {pin} is shared across multiple outputs: {role_desc}")

    # Check PCA9685 address and frequency overlaps
    if pca9685_usage:
        print("\nChecking PCA9685 expander channels...")
        for addr, usage in pca9685_usage.items():
            freqs = list(usage["freqs"])
            print(f" - PCA9685 @ 0x{addr:02X}: frequencies {freqs} Hz")
            if len(freqs) > 1:
                warnings.append(f"⚠️ PCA9685 Frequency Mismatch: PCA9685 at 0x{addr:02X} has channels sharing conflicting frequencies: {freqs} Hz.")
            for ch_idx, usages in usage["channels"].items():
                if len(usages) > 1:
                    role_desc = ", ".join([u[1] for u in usages])
                    errors.append(f"❌ PCA9685 Channel Collision: PCA9685 @ 0x{addr:02X} channel {ch_idx} is shared multiple times: {role_desc}")

    if other_expanders:
        print("\nChecking Digital Expander / DAC address channels...")
        for src_id, addrs in other_expanders.items():
            src_name = src_names.get(src_id, f"Expander {src_id}")
            for addr, channels in addrs.items():
                print(f" - {src_name} @ 0x{addr:02X}: channels {list(channels.keys())}")
                for ch_idx, usages in channels.items():
                    if len(usages) > 1:
                        role_desc = ", ".join([u[1] for u in usages])
                        errors.append(f"❌ Expander Channel Collision: {src_name} @ 0x{addr:02X} channel {ch_idx} is shared multiple times: {role_desc}")

    print("\n================== RESOURCE REPORT SUMMARY ==================")
    if errors:
        print(f"🔴 CONFIGURATION FAILED ({len(errors)} Errors, {len(warnings)} Warnings)")
        for err in errors:
            print(f"  {err}")
    elif warnings:
        print(f"🟡 CONFIGURATION SAFE WITH WARNINGS ({len(warnings)} Warnings)")
    else:
        print("🟢 CONFIGURATION SAFE! (0 Errors, 0 Warnings)")
        print("   All budget checks passed successfully.")
    for warn in warnings:
        print(f"  {warn}")
    print("=============================================================")

def run_interactive():
    print("\n--- Running in Interactive Mode (3 Budgets) ---")
    total_leds = int(input("1. How many total LEDs (NeoPixels) do you plan to use? [0] ") or 0)
    led_strips = int(input("2. How many separate physical LED strips (Output Pins) will you use? [0] ") or 0)
    dmx_channels = int(input("3. How many DMX Serial outputs will you use? [0] ") or 0)
    
    print("\nFor Motion Control / Analog GPIO devices:")
    dimmers = int(input(" - How many DC PWM Dimmers? [0] ") or 0)
    analog_rgb = int(input(" - How many Analog RGB modules? [0] ") or 0)
    analog_rgbw = int(input(" - How many Analog RGBW modules? [0] ") or 0)
    servos = int(input(" - How many RC Servos? [0] ") or 0)
    motors = int(input(" - How many DC Motors? [0] ") or 0)
    motor_mode = 0
    if motors > 0:
        motor_mode = int(input("   - Enter average motor mode (0=PWM+PWM [2 channels], 1=PWM+DIR [1 channel], 2=IN1+IN2+EN [1 channel]): [0] ") or 0)
    steppers = int(input(" - How many Steppers? [0] ") or 0)
    mixed_stepper = "n"
    if steppers > 0:
        mixed_stepper = input("   - Will they use mixed pins (STEP on ESP32, DIR/ENABLE on expander)? (y/n) [n] ") or "n"
    relays = int(input(" - How many Relays/Solenoids? [0] ") or 0)
    smoke_machines = int(input(" - How many Smoke Sequences? [0] ") or 0)
    segment_displays_2pin = int(input(" - How many 7-Segment displays (TM1637)? [0] ") or 0)
    segment_displays_pwm7 = int(input(" - How many 7-Segment displays (7-Pin Direct Drive)? [0] ") or 0)
    segment_displays_pwm8 = int(input(" - How many 7-Segment displays (8-Pin Direct Drive)? [0] ") or 0)
    dfplayers = int(input(" - How many DFPlayer MP3 modules? [0] ") or 0)
    func_gens = int(input(" - How many Function Generators? [0] ") or 0)
    buzzers = int(input(" - How many Passive Buzzers? [0] ") or 0)
    
    dummy_outputs = []
    
    # LED Strips
    for _ in range(led_strips):
        dummy_outputs.append({"type": 3, "led_count": total_leds // led_strips if led_strips > 0 else 0, "pin": 4, "source": 0})
    # DMX Serials
    for i in range(dmx_channels):
        dummy_outputs.append({"type": 1, "pin": 17 if i == 0 else (2 if i == 1 else 15), "source": 0})
    # Relays
    for _ in range(relays):
        dummy_outputs.append({"type": 2, "pin": 2, "source": 0})
    # Single LED / Dimmers
    for _ in range(dimmers):
        dummy_outputs.append({"type": 4, "pin": 12, "source": 0})
    # Analog RGB
    for _ in range(analog_rgb):
        dummy_outputs.append({"type": 5, "pin": 12, "pin2": 14, "pin3": 15, "source": 0, "color_order": 0})
    # Analog RGBW
    for _ in range(analog_rgbw):
        dummy_outputs.append({"type": 5, "pin": 12, "pin2": 14, "pin3": 15, "pin4": 13, "source": 0, "color_order": 4})
    # DC Motors
    for _ in range(motors):
        dummy_outputs.append({"type": 6, "pin": 12, "pin2": 14, "pin3": 15, "source": 0, "mc_mode": motor_mode})
    # Steppers
    for _ in range(steppers):
        if mixed_stepper.lower() == 'y':
            dummy_outputs.append({
                "type": 7, "pin": 12, "source": 0, "pin2": 0, "pin2_source": 2, "pin2_addr": 32, "pin3": 1, "pin3_source": 2, "pin3_addr": 32
            })
        else:
            dummy_outputs.append({"type": 7, "pin": 12, "pin2": 14, "source": 0})
    # Servos
    for _ in range(servos):
        dummy_outputs.append({"type": 8, "pin": 12, "source": 0})
    # Passive Buzzers
    for _ in range(buzzers):
        dummy_outputs.append({"type": 9, "pin": 12, "source": 0})
    # DFPlayer
    for _ in range(dfplayers):
        dummy_outputs.append({"type": 10, "pin": 17, "pin2": 16, "source": 0})
    # TM1637
    for _ in range(segment_displays_2pin):
        dummy_outputs.append({"type": 11, "pin": 12, "pin2": 14, "source": 0})
    # 7-Seg 7-Pin
    for _ in range(segment_displays_pwm7):
        dummy_outputs.append({"type": 12, "seg_pins": [12, 14, 15, 13, 2, 4, 17], "source": 0})
    # 7-Seg 8-Pin
    for _ in range(segment_displays_pwm8):
        dummy_outputs.append({"type": 13, "seg_pins": [12, 14, 15, 13, 2, 4, 17, 32], "source": 0})
    # Function Generator
    for _ in range(func_gens):
        dummy_outputs.append({"type": 16, "pin": 12, "source": 0})
    # Solenoids
    for _ in range(relays):
        dummy_outputs.append({"type": 17, "pin": 2, "source": 0})
    # Smoke machines
    for _ in range(smoke_machines):
        dummy_outputs.append({"type": 18, "pin": 12, "pin2": 14, "source": 0})
        
    status_pin = int(input("\nEnter Status LED GPIO pin: [5] ") or 5)
    zc_pin = int(input("Enter Zero-Crossing GPIO pin (255 for none): [255] ") or 255)
    output_fps = int(input("Enter output FPS: [40] ") or 40)
    is_master_input = input("Is ESP-NOW Master mode active? (y/n) [n] ") or "n"
    is_master = is_master_input.lower() == 'y'
    
    run_calculation_on_channels(dummy_outputs, status_led=status_pin, zc_pin=zc_pin, output_fps=output_fps, is_master=is_master)

def main():
    print_banner()
    
    if len(sys.argv) > 1:
        path = sys.argv[1]
        print(f"Loading configuration from command line path: {path}")
        outputs = parse_json_outputs(path)
        if outputs is not None:
            run_calculation_on_channels(outputs)
            return
        else:
            print("Failed to load specified file. Falling back to search.")
            
    default_locations = [
        "outputs.json",
        "data/outputs.json",
        "../data/outputs.json",
        "c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/data/outputs.json",
        "c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/outputs.json"
    ]
    
    found_path = None
    for loc in default_locations:
        if os.path.exists(loc):
            found_path = loc
            break
            
    if found_path:
        print(f"Found outputs configuration file at: {found_path}")
        outputs = parse_json_outputs(found_path)
        if outputs is not None:
            run_calculation_on_channels(outputs)
            return
 
    print("Could not find any 'outputs.json' configuration file automatically.")
    choice = input("Would you like to run in interactive input mode? (y/n): [y] ") or "y"
    if choice.lower() == 'y':
        run_interactive()
    else:
        print("Calculation cancelled. Place 'outputs.json' in this folder and rerun.")

if __name__ == "__main__":
    main()
