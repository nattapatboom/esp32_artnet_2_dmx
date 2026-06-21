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

def print_banner():
    print("=====================================================================")
    print("            ESP32 Art-Net/sACN Node Resource Calculator              ")
    print("=====================================================================")

def check_pin_safety(pin, status_led=5, zc_pin=255, role="Output"):
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

def run_calculation_on_channels(outputs, status_led=5, zc_pin=255):
    """
    Performs resource analysis on a list of channel configurations.
    """
    print(f"\nAnalyzing {len(outputs)} configured channels...")
    
    # Standard ESP32 resource limits
    max_rmt = 8
    max_uart_dmx = 2
    max_ledc = 16
    max_channels_slots = 16
    
    # Counter trackers
    used_rmt = 0
    used_uart_dmx = 0
    used_ledc = 0
    total_leds = 0
    wiz_bulbs = 0
    stepper_count = 0
    
    # Hardware/GPIO tracking
    gpio_usage = {}  # pin -> list of (channel_index, role)
    
    # Expander usage tracking
    # pca9685: {address: {channels_used_set, frequencies_set}}
    pca9685_usage = {}
    other_expanders = {}  # chip_type -> {address: [channels_used]}
    
    # Channel types
    type_names = {
        0: "LED Strip",
        1: "DMX Serial",
        2: "Relay",
        3: "AC Dimmer",
        4: "WiZ Smart Bulb (Deprecated)",
        5: "DC PWM Dimmer",
        6: "DC Motor",
        7: "Stepper Motor",
        8: "RC Servo",
        9: "Solenoid Trigger",
        10: "Analog RGB / RGBW Strip",
        11: "Passive Buzzer",
        12: "Sequential Smoke Shooter",
        13: "7-Segment Display"
    }
    
    warnings = []
    errors = []
    
    # Step 1: Analyze each channel's footprint
    for idx, ch in enumerate(outputs):
        ch_type = ch.get("type", 0)
        ch_name = type_names.get(ch_type, f"Unknown ({ch_type})")
        
        # WiZ bulbing doesn't use physical pins or ESP32 drivers
        if ch_type == 4:
            wiz_bulbs += 1
            warnings.append(f"⚠️ Deprecated Device: Channel {idx+1} is configured as a WiZ Smart Bulb. WiZ support is deprecated and will be removed.")
            continue
            
        # Determine the active pins and resource types for this channel
        active_pins = []  # list of (pin_key, default_val, role_name, resource_type)
        
        if ch_type == 0:  # LED Strip
            active_pins = [("pin", 4, "LED Data", "rmt")]
            total_leds += ch.get("led_count", 0)
            
        elif ch_type == 1:  # DMX Serial
            active_pins = [("pin", 17, "DMX TX", "dmx")]
            
        elif ch_type == 2:  # Relay
            active_pins = [("pin", 2, "Relay Pin", "digital")]
            
        elif ch_type == 3:  # AC Dimmer
            active_pins = [("pin", 12, "TRIAC Dimmer Pin", "digital")]
            
        elif ch_type == 5:  # DC PWM Dimmer
            active_pins = [("pin", 12, "PWM Dimmer Pin", "ledc")]
            
        elif ch_type == 6:  # DC Motor
            mc_mode = ch.get("mc_mode", 0)
            if mc_mode == 0:  # PWM+PWM
                active_pins = [("pin", 12, "Motor PWM1 (Fwd)", "ledc"), ("pin2", 14, "Motor PWM2 (Rev)", "ledc")]
            elif mc_mode == 1:  # PWM+DIR
                active_pins = [("pin", 12, "Motor PWM", "ledc"), ("pin2", 14, "Motor DIR", "digital")]
            elif mc_mode == 2:  # IN1+IN2+EN
                active_pins = [("pin", 12, "Motor IN1", "digital"), ("pin2", 14, "Motor IN2", "digital"), ("pin3", 15, "Motor EN (PWM)", "ledc")]
                
        elif ch_type == 7:  # Stepper Motor
            active_pins = [("pin", 12, "Stepper STEP", "stepper_step"), ("pin2", 14, "Stepper DIR", "digital")]
            if ch.get("pin3", 255) != 255:
                active_pins.append(("pin3", 255, "Stepper ENABLE", "digital"))
            if ch.get("pin4", 255) != 255:
                active_pins.append(("pin4", 255, "Stepper LIMIT/Sensor", "input"))
            stepper_count += 1
            
        elif ch_type == 8:  # RC Servo
            active_pins = [("pin", 12, "Servo PWM Pin", "ledc_servo")]
            
        elif ch_type == 9:  # Solenoid Trigger
            active_pins = [("pin", 2, "Solenoid Gate Pin", "digital")]
            
        elif ch_type == 10:  # Analog RGB / RGBW Strip
            active_pins = [("pin", 12, "RGB Red", "ledc"), ("pin2", 14, "RGB Green", "ledc"), ("pin3", 15, "RGB Blue", "ledc")]
            if ch.get("color_order", 0) >= 4:
                active_pins.append(("pin4", 13, "RGBW White", "ledc"))
            
        elif ch_type == 11:  # Passive Buzzer
            active_pins = [("pin", 12, "Buzzer PWM", "ledc")]
            
        elif ch_type == 12:  # Sequential Smoke Shooter
            active_pins = [("pin", 12, "Smoke Fogger Pin", "digital"), ("pin2", 14, "Smoke Shooter Pin", "digital")]
 
        elif ch_type == 13:  # 7-Segment Display
            active_pins = [("pin", 12, "7-Seg CLK", "digital"), ("pin2", 14, "7-Seg DIO", "digital")]

        # Process each pin individually (supports mixed-mode pins!)
        i2c_bus_used = False
        sda = ch.get("sda_pin", 32)
        scl = ch.get("scl_pin", 33)
        
        for pin_key, default_val, role_name, res_type in active_pins:
            pin_val = ch.get(pin_key, default_val)
            if pin_val == 255:
                continue
                
            # Check the source of this specific pin
            # Checks pin-specific source e.g. "pin2_source", falling back to "source", falling back to "esp32_gpio"
            p_source = ch.get(f"{pin_key}_source", ch.get("source", "esp32_gpio"))
            is_p_expander = p_source in ["pca9685", "mcp23017", "tca9555", "pcf8574", "tpic6b595", "tlc5947"]
            
            if is_p_expander:
                i2c_bus_used = True
                # Determine expander address
                p_addr = ch.get(f"{pin_key}_addr", ch.get("i2c_addr", ch.get("address", 0x40 if p_source == "pca9685" else 0x20)))
                if isinstance(p_addr, str):
                    p_addr = int(p_addr, 16) if p_addr.lower().startswith("0x") else int(p_addr)
                    
                if p_source == "pca9685":
                    if p_addr not in pca9685_usage:
                        pca9685_usage[p_addr] = {"channels": set(), "freqs": set()}
                    
                    # Track PCA9685 channels and frequency
                    freq = ch.get(f"{pin_key}_freq", ch.get("mc_freq", ch.get("pca_frequency", 50 if res_type == "ledc_servo" else 1000)))
                    pca9685_usage[p_addr]["freqs"].add(freq)
                    
                    if pin_val in pca9685_usage[p_addr]["channels"]:
                        errors.append(f"❌ Expander Conflict: PCA9685 at 0x{p_addr:02X} channel {pin_val} is used multiple times (Channel {idx+1}: {role_name}).")
                    pca9685_usage[p_addr]["channels"].add(pin_val)
                    
                else:  # Other GPIO/Sink expanders
                    if p_source not in other_expanders:
                        other_expanders[p_source] = {}
                    if p_addr not in other_expanders[p_source]:
                        other_expanders[p_source][p_addr] = set()
                        
                    if pin_val in other_expanders[p_source][p_addr]:
                        errors.append(f"❌ Expander Conflict: {p_source.upper()} at 0x{p_addr:02X} channel {pin_val} is used multiple times (Channel {idx+1}: {role_name}).")
                    other_expanders[p_source][p_addr].add(pin_val)
            else:
                # Direct ESP32 GPIO pin
                if pin_val not in gpio_usage:
                    gpio_usage[pin_val] = []
                gpio_usage[pin_val].append((idx, role_name))
                
                # Count ESP32 peripherals
                if res_type == "rmt":
                    used_rmt += 1
                elif res_type == "ledc" or res_type == "ledc_servo":
                    used_ledc += 1
                elif res_type == "dmx":
                    if used_uart_dmx < max_uart_dmx:
                        used_uart_dmx += 1
                    else:
                        used_rmt += 1
                        
        # Register shared Zero-Crossing pin if direct AC Dimmer is used
        if ch_type == 3:
            if zc_pin != 255:
                if zc_pin not in gpio_usage:
                    gpio_usage[zc_pin] = []
                gpio_usage[zc_pin].append((idx, "Zero-Crossing Interrupt"))
                
        # Register shared I2C pins if expander is used on this channel
        if i2c_bus_used:
            for pin, pin_role in [(sda, "I2C SDA"), (scl, "I2C SCL")]:
                if pin not in gpio_usage:
                    gpio_usage[pin] = []
                gpio_usage[pin].append((idx, pin_role))

    # Step 2: Validate GPIO Pin Overlaps and safety
    print("\nChecking ESP32 physical GPIO allocations...")
    for pin, usages in sorted(gpio_usage.items()):
        role_desc = ", ".join([f"Ch{u[0]+1}: {u[1]}" for u in usages])
        print(f" - GPIO {pin:2d}: Used by {role_desc}")
        
        # Check safety of the pin
        is_input = any("LIMIT" in u[1] or "Zero-Crossing" in u[1] for u in usages)
        role_label = "Input" if is_input else "Output"
        safe, msg = check_pin_safety(pin, status_led, zc_pin, role_label)
        if not safe:
            if "CRITICAL" in msg or "CONFLICT" in msg:
                errors.append(f"❌ GPIO Pin Conflict: Pin {pin} used as {role_label} for {role_desc}. Error: {msg}")
            else:
                warnings.append(f"⚠️ GPIO Pin Warning: Pin {pin} used as {role_label} for {role_desc}. Details: {msg}")
                
        # Check duplicate assignment across different channels (except shared I2C / ZC pins)
        unique_channels = {u[0] for u in usages}
        is_shared_allowable = any("I2C" in u[1] or "Zero-Crossing" in u[1] for u in usages)
        if len(unique_channels) > 1 and not is_shared_allowable:
            errors.append(f"❌ GPIO Pin Collision: GPIO {pin} is shared across multiple outputs: {role_desc}")

    # Check status LED and zero-crossing pins specifically
    safe_led, led_msg = check_pin_safety(status_led, status_led, zc_pin, "Output")
    if not safe_led and status_led != 255:
        if "CRITICAL" in led_msg:
            errors.append(f"❌ Status LED Pin Error: GPIO {status_led} configured as Status LED. Error: {led_msg}")
    
    if zc_pin != 255:
        safe_zc, zc_msg = check_pin_safety(zc_pin, status_led, zc_pin, "Input")
        if not safe_zc:
            if "CRITICAL" in zc_msg:
                errors.append(f"❌ Zero-Crossing Pin Error: GPIO {zc_pin} configured as Zero-Crossing. Error: {zc_msg}")

    # Step 3: Validate Expander Shared Frequency Conflicts
    if pca9685_usage:
        print("\nChecking PCA9685 expander frequencies...")
        for addr, usage in pca9685_usage.items():
            freqs = list(usage["freqs"])
            ch_list = sorted(list(usage["channels"]))
            print(f" - PCA9685 @ 0x{addr:02X}: Channels {ch_list} configured with frequencies {freqs} Hz")
            if len(freqs) > 1:
                warnings.append(
                    f"⚠️ PCA9685 Frequency Mismatch: PCA9685 at 0x{addr:02X} has channels sharing conflicting frequencies: {freqs} Hz. "
                    f"All 16 channels share ONE physical frequency register. The hardware will run at the last configured frequency."
                )

    # Step 4: Validate global firmware limits
    print("\nChecking hardware peripheral constraints...")
    
    # RMT limit
    if used_rmt > max_rmt:
        errors.append(f"❌ RMT Overload: Requested {used_rmt} RMT channels (LED strips + extra DMX serials). ESP32 hardware limit is {max_rmt}.")
    else:
        print(f" ✅ RMT Channels: {used_rmt}/{max_rmt} used.")
        
    # LEDC limit
    if used_ledc > max_ledc:
        errors.append(f"❌ LEDC PWM Overload: Requested {used_ledc} LEDC channels (dimmers + servos + motors). ESP32 hardware limit is {max_ledc}.")
    else:
        print(f" ✅ LEDC PWM Channels: {used_ledc}/{max_ledc} used.")
        
    # UART DMX limit
    if used_uart_dmx > max_uart_dmx:
        errors.append(f"❌ UART DMX Overload: Requested {used_uart_dmx} hardware UART DMX ports. ESP32 limit is {max_uart_dmx}.")
    else:
        print(f" ✅ DMX Hardware UARTs: {used_uart_dmx}/{max_uart_dmx} used.")

    # Stepper limit
    if stepper_count > 8:
        errors.append(f"❌ Stepper Limit Exceeded: Configured {stepper_count} steppers. Firmware only allocates memory for up to 8 steppers.")
    elif stepper_count > 0:
        print(f" ✅ Stepper Engines: {stepper_count}/8 used.")

    # Max channels slots
    if len(outputs) > max_channels_slots:
        errors.append(f"❌ Slot Count Exceeded: Configured {len(outputs)} output slots. The firmware code restricts outputs.json to a maximum of {max_channels_slots} entries.")
    else:
        print(f" ✅ Output Channel Slots: {len(outputs)}/{max_channels_slots} used.")

    # Network loads
    if wiz_bulbs > 20:
        warnings.append(f"⚠️ High WiZ Bulb Count: {wiz_bulbs} WiZ bulbs configured. Sending UDP packets to >20 bulbs over Wi-Fi can lead to packet losses and stuttering.")
    elif wiz_bulbs > 0:
        print(f" ✅ WiZ Bulbs Network Footprint: {wiz_bulbs} bulbs.")
        
    if total_leds > 4000:
        warnings.append(f"⚠️ High LED Count: {total_leds} total LEDs. Exceeding 4000 LEDs consumes critical RAM heap and can trigger brownouts or watchdog resets.")
    elif total_leds > 0:
        print(f" ✅ Total LED Strip Load: {total_leds} pixels.")

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
    print("\n--- Running in Interactive Mode ---")
    total_leds = int(input("1. How many total LEDs (NeoPixels) do you plan to use? [0] ") or 0)
    led_strips = int(input("2. How many separate physical LED strips (Output Pins) will you use? [0] ") or 0)
    dmx_channels = int(input("3. How many DMX Serial output pins? [0] ") or 0)
    wiz_bulbs = 0
    
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
    segment_displays = int(input(" - How many 7-Segment Displays? [0] ") or 0)
    buzzers = int(input(" - How many Passive Buzzers? [0] ") or 0)
    
    # Approximate outputs list for run_calculation_on_channels
    dummy_outputs = []
    
    # LED Strips
    for _ in range(led_strips):
        dummy_outputs.append({"type": 0, "led_count": total_leds // led_strips if led_strips > 0 else 0, "pin": 4})
    # DMX Serials
    for i in range(dmx_channels):
        dummy_outputs.append({"type": 1, "pin": 17 if i == 0 else (2 if i == 1 else 15)})
    # Relays/Solenoids
    for _ in range(relays):
        dummy_outputs.append({"type": 2, "pin": 2})
    # WiZ Bulbs
    for _ in range(wiz_bulbs):
        dummy_outputs.append({"type": 4})
    # DC Dimmers
    for _ in range(dimmers):
        dummy_outputs.append({"type": 5, "pin": 12})
    # DC Motors
    for _ in range(motors):
        dummy_outputs.append({"type": 6, "pin": 12, "pin2": 14, "pin3": 15, "mc_mode": motor_mode})
    # Steppers
    for _ in range(steppers):
        if mixed_stepper.lower() == 'y':
            dummy_outputs.append({
                "type": 7, 
                "pin": 12,                  # STEP on ESP32
                "pin_source": "esp32_gpio",
                "pin2": 0,                  # DIR on expander channel 0
                "pin2_source": "mcp23017",
                "pin3": 1,                  # EN on expander channel 1
                "pin3_source": "mcp23017"
            })
        else:
            dummy_outputs.append({"type": 7, "pin": 12, "pin2": 14})
    # Servos
    for _ in range(servos):
        dummy_outputs.append({"type": 8, "pin": 12})
    # Smoke machines
    for _ in range(smoke_machines):
        dummy_outputs.append({"type": 12, "pin": 12, "pin2": 14})
    # 7-Segment Displays
    for _ in range(segment_displays):
        dummy_outputs.append({"type": 13, "pin": 12, "pin2": 14})
    # Analog RGB
    for _ in range(analog_rgb):
        dummy_outputs.append({"type": 10, "pin": 12, "pin2": 14, "pin3": 15, "color_order": 0})
    # Analog RGBW
    for _ in range(analog_rgbw):
        dummy_outputs.append({"type": 10, "pin": 12, "pin2": 14, "pin3": 15, "pin4": 13, "color_order": 4})
    # Passive Buzzers
    for _ in range(buzzers):
        dummy_outputs.append({"type": 11, "pin": 12})

        
    status_pin = int(input("\nEnter Status LED GPIO pin number: [5] ") or 5)
    zc_pin = int(input("Enter Zero-Crossing GPIO pin number (or 255 for none): [255] ") or 255)
    
    run_calculation_on_channels(dummy_outputs, status_pin, zc_pin)

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
