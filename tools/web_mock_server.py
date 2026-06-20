#!/usr/bin/env python3
import os
import json
import random
from http.server import HTTPServer, BaseHTTPRequestHandler

PORT = 8000
HTML_FILE = "web/index.html"

# Default files to save mock database state
SETTINGS_FILE = "tools/mock_settings.json"
OUTPUTS_FILE = "tools/mock_outputs.json"
PEERS_FILE = "tools/mock_peers.json"

DEFAULT_SETTINGS = {
    "device_mode": 0,
    "artnet_port": 6454,
    "dhcp": True,
    "ip": [192, 168, 1, 93],
    "subnet": [255, 255, 255, 0],
    "gateway": [192, 168, 1, 1],
    "dns": [8, 8, 8, 8],
    "wifi_ssid": "My-Stage-WiFi",
    "wifi_pass": "securepassword",
    "ap_ssid": "ArtNet-Node-AP",
    "ap_pass": "12345678",
    "output_fps": 40,
    "dmx_start_universe": 0
}

DEFAULT_OUTPUTS = {
    "channels": [
        {
            "type": 0,
            "source": 0,
            "pin": 12,
            "length": 150,
            "color_order": 0,
            "universe": 0,
            "address": 1
        },
        {
            "type": 1,
            "source": 0,
            "pin": 17,
            "dmx_port": 1,
            "universe": 1,
            "address": 1
        }
    ]
}

DEFAULT_PEERS = {
    "peers": [
        {
            "mac": "24:6F:28:B1:A2:C4",
            "start_universe": 0,
            "start_address": 1,
            "end_universe": 0,
            "end_address": 512
        }
    ]
}

def load_json(filepath, default_data):
    if os.path.exists(filepath):
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return default_data

def save_json(filepath, data):
    try:
        with open(filepath, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=4)
    except Exception as e:
        print(f"Error saving to {filepath}: {e}")

class MockRequestHandler(BaseHTTPRequestHandler):
    def send_cors_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_cors_headers()
        self.end_headers()

    def do_GET(self):
        # Serve the HTML page
        if self.path in ("/", "/index.html"):
            if not os.path.exists(HTML_FILE):
                self.send_response(404)
                self.end_headers()
                self.wfile.write(b"Error: web/index.html not found. Run extract_web.py first!")
                return
            
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_cors_headers()
            self.end_headers()
            with open(HTML_FILE, "rb") as f:
                self.wfile.write(f.read())
            return

        # Serve mockup API endpoints
        elif self.path == "/api/settings":
            data = load_json(SETTINGS_FILE, DEFAULT_SETTINGS)
            self.send_json_response(200, data)
            
        elif self.path == "/api/outputs":
            data = load_json(OUTPUTS_FILE, DEFAULT_OUTPUTS)
            self.send_json_response(200, data)
            
        elif self.path == "/api/espnow-peers":
            data = load_json(PEERS_FILE, DEFAULT_PEERS)
            self.send_json_response(200, data)

        elif self.path == "/api/wifi-scan":
            mock_ssids = [
                {"ssid": "Stage-Light-5G", "rssi": -45, "secure": True},
                {"ssid": "Backstage-Staff", "rssi": -68, "secure": True},
                {"ssid": "Public_WiFi", "rssi": -85, "secure": False},
                {"ssid": "ArtNet-Hotspot", "rssi": -30, "secure": True}
            ]
            self.send_json_response(200, {"networks": mock_ssids})

        elif self.path == "/api/status":
            settings = load_json(SETTINGS_FILE, DEFAULT_SETTINGS)
            mock_telemetry = {
                "device_mode": settings.get("device_mode", 0),
                "eth_up": True,
                "wifi_up": False,
                "ap_up": True,
                "wifi_ssid_active": "",
                "ip": "192.168.1.93",
                "packets_received": random.randint(1000, 5000),
                "sacn_packets": random.randint(50, 200),
                "output_fps": settings.get("output_fps", 40),
                "time": ["Art-Net", "ESP-NOW Master", "ESP-NOW Slave"][settings.get("device_mode", 0)],
                "active": True,
                "heap_free": random.randint(180000, 210000),
                "min_heap": 175000,
                "cpu_freq": 240,
                "board_mac": "30:AE:A4:07:0C:48"
            }
            self.send_json_response(200, mock_telemetry)

        elif self.path == "/api/scoring":
            outputs = load_json(OUTPUTS_FILE, DEFAULT_OUTPUTS)
            # Calculate mock scores
            channels = []
            total_score = 0.0
            
            # Simple score rule simulation: NeoPixel=10, DMX=15, DFPlayer=5, etc.
            score_map = {0: 10, 1: 15, 2: 1.0, 3: 5, 5: 3, 6: 3, 7: 15, 8: 2, 9: 1, 10: 3, 11: 2, 12: 2, 13: 4, 15: 8}
            type_names = {0: "NeoPixel", 1: "DMX Output", 2: "Relay", 3: "AC Dimmer", 5: "LEDC PWM", 6: "DC Motor", 7: "Stepper Motor", 8: "RC Servo", 9: "Solenoid", 10: "RGB/RGBW Analog", 11: "Buzzer", 12: "Smoke Shooter", 13: "7-Segment", 15: "DFPlayer MP3"}
            
            for idx, ch in enumerate(outputs.get("channels", [])):
                ch_type = ch.get("type", 0)
                score = score_map.get(ch_type, 2.0)
                total_score += score
                channels.append({
                    "index": idx,
                    "type": ch_type,
                    "name": type_names.get(ch_type, "Unknown"),
                    "score": score
                })
                
            self.send_json_response(200, {
                "total": total_score,
                "limit": 100.0,
                "remaining": 100.0 - total_score,
                "channels": channels
            })
            
        elif self.path == "/api/i2c-scan":
            # Simulate scanning I2C addresses
            self.send_json_response(200, {"devices": [0x20, 0x40, 0x70]})

        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"404 Not Found")

    def do_POST(self):
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length)
        
        try:
            json_data = json.loads(post_data.decode('utf-8'))
        except Exception:
            json_data = None

        print(f"\n>>> POST Request on {self.path}")
        if json_data:
            print(json.dumps(json_data, indent=2))
        else:
            print(post_data)

        if self.path == "/api/settings":
            if json_data:
                save_json(SETTINGS_FILE, json_data)
            self.send_json_response(200, {"status": "ok"})
            
        elif self.path == "/api/outputs":
            if json_data:
                save_json(OUTPUTS_FILE, json_data)
            self.send_json_response(200, {"status": "ok"})
            
        elif self.path == "/api/espnow-peers":
            if json_data:
                save_json(PEERS_FILE, json_data)
            self.send_json_response(200, {"status": "ok"})
            
        elif self.path in ("/api/outputs/clear", "/api/config/factory-reset", "/api/config/import"):
            if self.path == "/api/outputs/clear":
                save_json(OUTPUTS_FILE, {"channels": []})
            elif self.path == "/api/config/factory-reset":
                save_json(SETTINGS_FILE, DEFAULT_SETTINGS)
                save_json(OUTPUTS_FILE, DEFAULT_OUTPUTS)
                save_json(PEERS_FILE, DEFAULT_PEERS)
            self.send_json_response(200, {"status": "ok"})
            
        elif self.path == "/api/output-test":
            self.send_json_response(200, {"status": "test_triggered"})
            
        elif self.path == "/update":
            # OTA upload mock
            self.send_response(200)
            self.send_cors_headers()
            self.end_headers()
            self.wfile.write(b"OTA Mock successful! Board is restarting...")
            
        else:
            self.send_response(404)
            self.end_headers()

    def send_json_response(self, status_code, data):
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json")
        self.send_cors_headers()
        self.end_headers()
        self.wfile.write(json.dumps(data).encode('utf-8'))

def run():
    print(f"Starting Mock Server on http://localhost:{PORT} ...")
    if not os.path.exists(HTML_FILE):
        print(f"WARNING: '{HTML_FILE}' was not found. Running extractor first...")
        from extract_web import main as extract_main
        extract_main()
        
    server_address = ('', PORT)
    httpd = HTTPServer(server_address, MockRequestHandler)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server.")
        httpd.server_close()

if __name__ == "__main__":
    run()
