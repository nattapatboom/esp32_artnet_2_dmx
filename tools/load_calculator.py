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

# Resource Weights matching include/scoring.h
W_GPIO = 0.5
W_LEDC = 2.5
W_RMT = 3.0
W_UART = 8.0
W_DAC = 2.0
W_PCA = 0.25
W_EXP = 0.125

SCORE_LIMIT = 109.0

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
    Parses outputs.json and returns channels list and configuration details.
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

def estimate_resources(ch):
    """
    Estimates resource footprint for a single channel matching scoring.h::estimateResources()
    """
    gpio = 0
    ledc = 0
    rmt = 0
    uart = 0
    dac = 0
    pca = 0
    expander = 0

    ch_type = ch.get("type", 0)
    source = ch.get("source", 0)
    pin = ch.get("pin", 255)
    pin2 = ch.get("pin2", 255)
    pin3 = ch.get("pin3", 255)
    pin4 = ch.get("pin4", 255)

    pin2_source = ch.get("pin2_source", source)
    pin3_source = ch.get("pin3_source", source)
    pin4_source = ch.get("pin4_source", source)

    def route_pin(pin_val, pin_src):
        nonlocal gpio, pca, expander
        if pin_val == 255:
            return
        if pin_src == 0:  # GPIO
            gpio += 1
        elif pin_src == 1:  # PCA9685
            pca += 1
        else:  # Expanders (2=MCP23017, 3=TCA9555, 4=PCF857x)
            expander += 1

    if ch_type == 0:  # AC Dimmer
        if source == 0: gpio = 1
    elif ch_type == 1:  # DMX
        if source == 0: gpio = 1
        uart = 1
    elif ch_type == 2:  # Relay
        route_pin(pin, source)
    elif ch_type == 3:  # RGB LED (NeoPixel)
        if source == 0: gpio = 1
        rmt = 1
    elif ch_type == 4:  # Single LED
        route_pin(pin, source)
        if source == 0 and pin != 255: ledc = 1
    elif ch_type == 5:  # Analog RGB
        # Red
        route_pin(pin, source)
        if source == 0 and pin != 255: ledc += 1
        # Green
        route_pin(pin2, pin2_source)
        if pin2_source == 0 and pin2 != 255: ledc += 1
        # Blue
        route_pin(pin3, pin3_source)
        if pin3_source == 0 and pin3 != 255: ledc += 1
        # White
        if ch.get("color_order", 0) >= 4:
            route_pin(pin4, pin4_source)
            if pin4_source == 0 and pin4 != 255: ledc += 1
    elif ch_type == 6:  # Motor
        mc_mode = ch.get("mc_mode", 0)
        route_pin(pin, source)
        if source == 0 and pin != 255: ledc += 1
        
        route_pin(pin2, pin2_source)
        if pin2_source == 0 and pin2 != 255:
            if mc_mode == 0: ledc += 1
        
        if mc_mode == 2:
            route_pin(pin3, pin3_source)
            if pin3_source == 0 and pin3 != 255: ledc += 1
    elif ch_type == 7:  # Stepper
        gpio += 1  # STEP pin is always direct ESP32 GPIO
        rmt += 2
        route_pin(pin2, pin2_source)
        route_pin(pin3, pin3_source)
        if pin4 != 255 and ch.get("mc_homing_mode", 0) == 0:
            route_pin(pin4, pin4_source)
    elif ch_type == 8:  # Servo
        route_pin(pin, source)
        if source == 0 and pin != 255: ledc = 1
    elif ch_type == 9:  # Buzzer
        if source == 0 and pin != 255:
            gpio = 1
            ledc = 1
    elif ch_type == 10:  # DFPlayer
        if source == 0: gpio = 2
        uart = 1
    elif ch_type == 11:  # 7-Seg TM1637
        if source == 0: gpio = 2
        else: expander = 2
    elif ch_type in (12, 13):  # 7-Seg DD 7-Pin / 8-Pin PWM
        num_seg = 8 if ch_type == 13 else 7
        seg_sources = ch.get("seg_sources", [])
        for i in range(num_seg):
            seg_src = seg_sources[i] if i < len(seg_sources) else source
            if seg_src == 0:
                gpio += 1
                ledc += 1
            elif seg_src == 1:
                pca += 1
            else:
                expander += 1
    elif ch_type == 14:  # DAC
        if source == 5:  # MCP4725
            expander = 1
        else:
            gpio = 1
            dac = 1
    elif ch_type == 15:  # PWM DAC
        route_pin(pin, source)
        if source == 0 and pin != 255: ledc = 1
    elif ch_type == 16:  # Func Gen
        gpio = 1
        ledc = 1
    elif ch_type == 17:  # Solenoid
        route_pin(pin, source)
    elif ch_type == 18:  # Smoke Shooter
        route_pin(pin, source)
        route_pin(pin2, pin2_source)

    return {
        "gpio": gpio,
        "ledc": ledc,
        "rmt": rmt,
        "uart": uart,
        "dac": dac,
        "pca": pca,
        "expander": expander
    }

def channel_compute_score(ch):
    """
    Computes compute score for a single channel matching scoring.h::channelComputeScore()
    """
    ch_type = ch.get("type", 0)
    if ch_type == 3:  # RGB LED
        return ch.get("led_count", 0) * 0.005
    elif ch_type == 6:  # Motor
        return 0.5
    elif ch_type == 7:  # Stepper
        return 2.0
    elif ch_type == 10:  # DFPlayer
        return 0.5
    elif ch_type == 11:  # 7-Seg TM1637
        return 0.5
    elif ch_type == 12:  # 7-Seg DD 7-Pin PWM
        return 1.0
    elif ch_type == 13:  # 7-Seg DD 8-Pin PWM
        return 1.2
    elif ch_type == 16:  # Func Gen
        return 2.0
    elif ch_type == 18:  # Smoke Shooter
        return 0.3
    return 0.0

def run_calculation_on_channels(outputs, status_led=5, zc_pin=255, sda_pin=14, scl_pin=15, output_fps=40):
    """
    Performs resource analysis on a list of channel configurations.
    """
    print(f"\nAnalyzing {len(outputs)} configured channels (v3)...")
    
    # Standard ESP32 hardware capacity limits
    max_rmt = 8
    max_uart_dmx = 2
    max_ledc = 16
    max_channels_slots = 16
    
    # Per-peripheral accumulators for resource limits check
    used_rmt = 0
    used_uart_dmx = 0
    used_ledc = 0
    total_leds = 0
    dfplayer_count = 0
    stepper_count = 0
    
    # Total scores
    total_resource_score = 0.0
    total_compute_score = (output_fps / 60.0) * 5.0  # Base FPS factor
    
    # Hardware pin/channel trackers
    gpio_usage = {}        # pin -> list of (channel_index, role)
    pca9685_usage = {}     # addr -> {"channels": {ch_index: list of (ch_idx, role)}, "freqs": set()}
    other_expanders = {}   # source_id -> {addr: {ch_index: list of (ch_idx, role)}}

    # Source Name Mapping
    src_names = {
        0: "ESP32 GPIO",
        1: "PCA9685",
        2: "MCP23017",
        3: "TCA9555",
        4: "PCF857x",
        5: "MCP4725"
    }
    
    type_names = {
        0: "AC Dimmer",
        1: "DMX Output",
        2: "Relay",
        3: "RGB LED",
        4: "Single LED",
        5: "Analog RGB",
        6: "Motor",
        7: "Stepper",
        8: "Servo",
        9: "Buzzer",
        10: "DFPlayer",
        11: "7-Seg TM1637",
        12: "7-Seg DD 7-Pin PWM",
        13: "7-Seg DD 8-Pin PWM",
        14: "DAC",
        15: "PWM DAC",
        16: "Func Gen",
        17: "Solenoid",
        18: "Smoke Shooter"
    }
    
    warnings = []
    errors = []
    
    # Step 1: Analyze each channel's footprint
    for idx, ch in enumerate(outputs):
        ch_type = ch.get("type", 0)
        ch_name = type_names.get(ch_type, f"Unknown ({ch_type})")
        
        # Estimate channel hardware resources
        u = estimate_resources(ch)
        
        # Accumulate peripheral limits
        used_rmt += u["rmt"]
        used_ledc += u["ledc"]
        used_uart_dmx += u["uart"]
        
        # Accumulate score
        ch_resource_score = (
            u["gpio"] * W_GPIO +
            u["ledc"] * W_LEDC +
            u["rmt"] * W_RMT +
            u["uart"] * W_UART +
            u["dac"] * W_DAC +
            u["pca"] * W_PCA +
            u["expander"] * W_EXP
        )
        ch_compute_score = channel_compute_score(ch)
        total_resource_score += ch_resource_score
        total_compute_score += ch_compute_score
        
        if ch_type == 3:  # RGB LED count
            total_leds += ch.get("led_count", 0)
        elif ch_type == 7:  # Stepper engines
            stepper_count += 1
        elif ch_type == 10:  # DFPlayer count
            dfplayer_count += 1

        # Track detailed pin/channel routing
        pins_to_check = [
            ("pin", "Primary Pin", ch.get("pin", 255), ch.get("source", 0), ch.get("pca_addr", 0x40), ch.get("pca_channel", 0)),
            ("pin2", "Secondary Pin", ch.get("pin2", 255), ch.get("pin2_source", ch.get("source", 0)), ch.get("pin2_addr", 0x20), ch.get("pca_channel2", 255)),
            ("pin3", "Tertiary Pin", ch.get("pin3", 255), ch.get("pin3_source", ch.get("source", 0)), ch.get("pin3_addr", 0x20), ch.get("pca_channel3", 255)),
            ("pin4", "Quaternary Pin", ch.get("pin4", 255), ch.get("pin4_source", ch.get("source", 0)), ch.get("pin4_addr", 0x20), ch.get("pca_channel4", 255)),
        ]
        
        # Add 7-segment segment pins if direct drive
        if ch_type in (12, 13):
            num_seg = 8 if ch_type == 13 else 7
            seg_pins = ch.get("seg_pins", [])
            seg_sources = ch.get("seg_sources", [])
            seg_addrs = ch.get("seg_addrs", [])
            for s in range(num_seg):
                s_pin = seg_pins[s] if s < len(seg_pins) else 255
                s_src = seg_sources[s] if s < len(seg_sources) else ch.get("source", 0)
                s_addr = seg_addrs[s] if s < len(seg_addrs) else 0x20
                pins_to_check.append((f"seg_pins[{s}]", f"Segment {s} Pin", s_pin, s_src, s_addr, s))

        i2c_bus_used = False
        for field, role_name, pin_val, pin_src, pin_addr, ch_idx in pins_to_check:
            if pin_val == 255:
                continue
                
            role_desc = f"{ch_name} (Ch{idx+1} {role_name})"
            
            if pin_src == 0:  # Direct ESP32 GPIO
                if pin_val not in gpio_usage:
                    gpio_usage[pin_val] = []
                gpio_usage[pin_val].append((idx, role_desc))
            else:  # I2C-based resource
                i2c_bus_used = True
                if pin_src == 1:  # PCA9685
                    if pin_addr not in pca9685_usage:
                        pca9685_usage[pin_addr] = {"channels": {}, "freqs": set()}
                    
                    # Track PCA9685 frequency requirements
                    # Servos force 50 Hz, Motors/Dimmers might require higher frequencies
                    target_freq = ch.get("mc_freq", ch.get("pca_frequency", 50 if ch_type == 8 else 1000))
                    pca9685_usage[pin_addr]["freqs"].add(target_freq)
                    
                    if ch_idx == 255:
                        continue
                    if ch_idx not in pca9685_usage[pin_addr]["channels"]:
                        pca9685_usage[pin_addr]["channels"][ch_idx] = []
                    pca9685_usage[pin_addr]["channels"][ch_idx].append((idx, role_desc))
                    
                else:  # Digital Expanders (2..4) or DAC (5)
                    if pin_src not in other_expanders:
                        other_expanders[pin_src] = {}
                    if pin_addr not in other_expanders[pin_src]:
                        other_expanders[pin_src][pin_addr] = {}
                    
                    if ch_idx == 255:
                        ch_idx = pin_val
                    if ch_idx not in other_expanders[pin_src][pin_addr]:
                        other_expanders[pin_src][pin_addr][ch_idx] = []
                    other_expanders[pin_src][pin_addr][ch_idx].append((idx, role_desc))

        # Register shared Zero-Crossing pin if direct AC Dimmer is used
        if ch_type == 0:
            if zc_pin != 255:
                if zc_pin not in gpio_usage:
                    gpio_usage[zc_pin] = []
                gpio_usage[zc_pin].append((idx, f"AC Dimmer Zero-Crossing Interrupt"))
                
        # Register shared I2C pins if I2C expanders are used
        if i2c_bus_used:
            for pin_val, pin_role in [(sda_pin, "I2C SDA"), (scl_pin, "I2C SCL")]:
                if pin_val not in gpio_usage:
                    gpio_usage[pin_val] = []
                gpio_usage[pin_val].append((idx, f"Shared {pin_role}"))

    # Step 2: Validate GPIO Pin Overlaps & safety
    print("\nChecking ESP32 physical GPIO allocations...")
    for pin, usages in sorted(gpio_usage.items()):
        role_desc = ", ".join([u[1] for u in usages])
        print(f" - GPIO {pin:2d}: Used by {role_desc}")
        
        # Check safety
        is_input = any("LIMIT" in u[1] or "Zero-Crossing" in u[1] or "Sensor" in u[1] for u in usages)
        role_label = "Input" if is_input else "Output"
        safe, msg = check_pin_safety(pin, status_led, zc_pin, role_label, sda_pin, scl_pin)
        if not safe:
            if "CRITICAL" in msg or "CONFLICT" in msg:
                errors.append(f"❌ GPIO Pin Conflict: Pin {pin} used as {role_label} for {role_desc}. Details: {msg}")
            else:
                warnings.append(f"⚠️ GPIO Pin Warning: Pin {pin} used as {role_label} for {role_desc}. Details: {msg}")
                
        # Duplicate direct GPIO check (except allowed shared pins: I2C or ZC)
        unique_channels = {u[0] for u in usages}
        is_shared_allowable = any("I2C" in u[1] or "Zero-Crossing" in u[1] for u in usages)
        if len(unique_channels) > 1 and not is_shared_allowable:
            errors.append(f"❌ GPIO Pin Collision: GPIO {pin} is shared across multiple outputs: {role_desc}")

    # Check status LED, zero-crossing, and I2C safety specifically
    if status_led != 255:
        safe_led, led_msg = check_pin_safety(status_led, status_led, zc_pin, "Output", sda_pin, scl_pin)
        if not safe_led and "CRITICAL" in led_msg:
            errors.append(f"❌ Status LED Pin Error: GPIO {status_led} configured as Status LED. Details: {led_msg}")
            
    if zc_pin != 255:
        safe_zc, zc_msg = check_pin_safety(zc_pin, status_led, zc_pin, "Input", sda_pin, scl_pin)
        if not safe_zc and "CRITICAL" in zc_msg:
            errors.append(f"❌ Zero-Crossing Pin Error: GPIO {zc_pin} configured as Zero-Crossing. Details: {zc_msg}")

    # Step 3: Validate I2C Address Collisions and Expander Channel Overlaps
    if pca9685_usage:
        print("\nChecking PCA9685 expander channels...")
        for addr, usage in pca9685_usage.items():
            freqs = list(usage["freqs"])
            print(f" - PCA9685 @ 0x{addr:02X}: frequency requirements {freqs} Hz")
            
            # Frequency conflict check
            if len(freqs) > 1:
                warnings.append(
                    f"⚠️ PCA9685 Frequency Mismatch: PCA9685 at 0x{addr:02X} has channels sharing conflicting frequencies: {freqs} Hz. "
                    f"All 16 channels share ONE physical frequency register. The hardware will run at the last configured frequency."
                )
            
            # Channel overlap check
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

    # Step 4: Validate global hardware capacity constraints & score limits
    print("\nChecking hardware peripheral constraints...")
    
    # RMT limit
    if used_rmt > max_rmt:
        errors.append(f"❌ RMT Overload: Requested {used_rmt} RMT channels. ESP32 hardware limit is {max_rmt}.")
    else:
        print(f" ✅ RMT Channels: {used_rmt}/{max_rmt} used.")
        
    # LEDC limit
    if used_ledc > max_ledc:
        errors.append(f"❌ LEDC PWM Overload: Requested {used_ledc} LEDC channels. ESP32 hardware limit is {max_ledc}.")
    else:
        print(f" ✅ LEDC PWM Channels: {used_ledc}/{max_ledc} used.")
        
    # UART DMX/DFPlayer limit (shared)
    # dfplayer has priority, UART count is checked
    used_uart = dfplayer_count + used_uart_dmx
    if used_uart > max_uart_dmx:
        errors.append(f"❌ UART Overload: Requested {used_uart} UART channels (DFPlayers: {dfplayer_count}, DMXs: {used_uart_dmx}). ESP32 hardware limit is {max_uart_dmx}.")
    else:
        print(f" ✅ Hardware UARTs: {used_uart}/{max_uart_dmx} used.")
 
    # Stepper limit
    if stepper_count > 8:
        errors.append(f"❌ Stepper Limit Exceeded: Configured {stepper_count} steppers. Firmware restricts steppers to maximum 8.")
    elif stepper_count > 0:
        print(f" ✅ Stepper Engines: {stepper_count}/8 used.")

    # Max channels slots
    if len(outputs) > max_channels_slots:
        errors.append(f"❌ Slot Count Exceeded: Configured {len(outputs)} output slots. The firmware restricts outputs.json to a maximum of {max_channels_slots} entries.")
    else:
        print(f" ✅ Output Channel Slots: {len(outputs)}/{max_channels_slots} used.")

    # High load checks
    if total_leds > 4000:
        warnings.append(f"⚠️ High LED Count: {total_leds} total LEDs. Exceeding 4000 LEDs consumes critical RAM heap and can trigger brownouts or watchdog resets.")
    elif total_leds > 0:
        print(f" ✅ Total LED Strip Load: {total_leds} pixels.")

    # Score limit validation
    total_combined_score = total_resource_score + total_compute_score
    print(f"\nScoring Breakdown:")
    print(f" - Resource Score : {total_resource_score:.3f} / 84.0")
    print(f" - Compute Score  : {total_compute_score:.3f} / 25.0")
    print(f" - Total Score     : {total_combined_score:.3f} / {SCORE_LIMIT}")

    if total_combined_score > SCORE_LIMIT:
        errors.append(f"❌ SCORE LIMIT EXCEEDED: Configured score of {total_combined_score:.3f} exceeds the safe limit of {SCORE_LIMIT}!")
    else:
        print(f" ✅ Total Combined Score is safe.")

    # Final summary display
    print("\n================== RESOURCE REPORT SUMMARY ==================")
    if errors:
        print(f"🔴 CONFIGURATION FAILED ({len(errors)} Errors, {len(warnings)} Warnings)")
        for err in errors:
            print(f"  {err}")
    elif warnings:
        print(f"🟡 CONFIGURATION SAFE WITH WARNINGS ({len(warnings)} Warnings)")
    else:
        print("🟢 CONFIGURATION SAFE! (0 Errors, 0 Warnings)")
        print("   All resource checks passed. Suitable for deployment.")
        
    for warn in warnings:
        print(f"  {warn}")
    print("=============================================================")

def run_interactive():
    print("\n--- Running in Interactive Mode (v3) ---")
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
    
    # LED Strips (Type 3)
    for _ in range(led_strips):
        dummy_outputs.append({"type": 3, "led_count": total_leds // led_strips if led_strips > 0 else 0, "pin": 4, "source": 0})
    # DMX Serials (Type 1)
    for i in range(dmx_channels):
        dummy_outputs.append({"type": 1, "pin": 17 if i == 0 else (2 if i == 1 else 15), "source": 0})
    # Relays (Type 2)
    for _ in range(relays):
        dummy_outputs.append({"type": 2, "pin": 2, "source": 0})
    # Single LED / Dimmers (Type 4)
    for _ in range(dimmers):
        dummy_outputs.append({"type": 4, "pin": 12, "source": 0})
    # Analog RGB (Type 5)
    for _ in range(analog_rgb):
        dummy_outputs.append({"type": 5, "pin": 12, "pin2": 14, "pin3": 15, "source": 0, "color_order": 0})
    # Analog RGBW (Type 5 RGBW)
    for _ in range(analog_rgbw):
        dummy_outputs.append({"type": 5, "pin": 12, "pin2": 14, "pin3": 15, "pin4": 13, "source": 0, "color_order": 4})
    # DC Motors (Type 6)
    for _ in range(motors):
        dummy_outputs.append({"type": 6, "pin": 12, "pin2": 14, "pin3": 15, "source": 0, "mc_mode": motor_mode})
    # Steppers (Type 7)
    for _ in range(steppers):
        if mixed_stepper.lower() == 'y':
            dummy_outputs.append({
                "type": 7, 
                "pin": 12,                  # STEP on ESP32 GPIO
                "source": 0,
                "pin2": 0,                  # DIR on expander address 0x20
                "pin2_source": 2,
                "pin2_addr": 32,
                "pin3": 1,                  # EN on expander address 0x20
                "pin3_source": 2,
                "pin3_addr": 32
            })
        else:
            dummy_outputs.append({"type": 7, "pin": 12, "pin2": 14, "source": 0})
    # Servos (Type 8)
    for _ in range(servos):
        dummy_outputs.append({"type": 8, "pin": 12, "source": 0})
    # Passive Buzzers (Type 9)
    for _ in range(buzzers):
        dummy_outputs.append({"type": 9, "pin": 12, "source": 0})
    # DFPlayer (Type 10)
    for _ in range(dfplayers):
        dummy_outputs.append({"type": 10, "pin": 17, "pin2": 16, "source": 0})
    # TM1637 Display (Type 11)
    for _ in range(segment_displays_2pin):
        dummy_outputs.append({"type": 11, "pin": 12, "pin2": 14, "source": 0})
    # 7-Seg DD 7-Pin (Type 12)
    for _ in range(segment_displays_pwm7):
        dummy_outputs.append({"type": 12, "seg_pins": [12, 14, 15, 13, 2, 4, 17], "source": 0})
    # 7-Seg DD 8-Pin (Type 13)
    for _ in range(segment_displays_pwm8):
        dummy_outputs.append({"type": 13, "seg_pins": [12, 14, 15, 13, 2, 4, 17, 32], "source": 0})
    # Function Generator (Type 16)
    for _ in range(func_gens):
        dummy_outputs.append({"type": 16, "pin": 12, "source": 0})
    # Solenoids (Type 17)
    for _ in range(relays):
        dummy_outputs.append({"type": 17, "pin": 2, "source": 0})
    # Smoke machines (Type 18)
    for _ in range(smoke_machines):
        dummy_outputs.append({"type": 18, "pin": 12, "pin2": 14, "source": 0})

    status_pin = int(input("\nEnter Status LED GPIO pin number: [5] ") or 5)
    zc_pin = int(input("Enter Zero-Crossing GPIO pin number (or 255 for none): [255] ") or 255)
    
    run_calculation_on_channels(dummy_outputs, status_led=status_pin, zc_pin=zc_pin)

def main():
    print_banner()
    
    # 1. Check if config path is provided as argument
    if len(sys.argv) > 1:
        path = sys.argv[1]
        print(f"Loading configuration from command line path: {path}")
        outputs = parse_json_outputs(path)
        if outputs is not None:
            run_calculation_on_channels(outputs)
            return
        else:
            print("Failed to load specified file. Falling back to search.")
            
    # 2. Try default locations
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
 
    # 3. If no file found, prompt user for interactive run
    print("Could not find any 'outputs.json' configuration file automatically.")
    choice = input("Would you like to run in interactive input mode? (y/n): [y] ") or "y"
    if choice.lower() == 'y':
        run_interactive()
    else:
        print("Calculation cancelled. Place 'outputs.json' in this folder and rerun.")

if __name__ == "__main__":
    main()
