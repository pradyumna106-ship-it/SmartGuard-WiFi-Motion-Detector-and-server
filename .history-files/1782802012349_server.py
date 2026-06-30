from datetime import datetime
from flask import Flask, jsonify, request, Response
import socket
import os
from flask_cors import CORS
import cv2
app = Flask(__name__)

# In-memory store: deviceId -> latest device state
devices = {}



def now_str():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


@app.route("/wifi/rssi", methods=["POST"])
def receive_device_data():
    data = request.get_json(silent=True)

    if not data:
        return jsonify({"error": "JSON body required"}), 400

    device_id = data.get("deviceId")
    if not device_id:
        return jsonify({"error": "deviceId is required"}), 400

    existing = devices.get(device_id, {})

    device = {
        "id": device_id,
        "name": device_id,
        "rssi": data.get("rssi", existing.get("rssi")),
        "motionDetected": data.get("motionDetected", existing.get("motionDetected")),
        "strongMotionSense": data.get("strongMotionSense", existing.get("strongMotionSense")),
        "ip": data.get("ip", existing.get("ip")),
        "lastActive": now_str()
    }

    devices[device_id] = device

    print(f"[{device['lastActive']}] Received from {device_id}: {device}")
    print("RAW REQUEST JSON:", data)

    return jsonify({
        "status": "success",
        "message": "Device data received",
        "data": device
    }), 200


@app.route("/wifi/rssi/<device_id>", methods=["GET"])
def get_device(device_id):
    device = devices.get(device_id)

    if not device:
        return jsonify({"error": "Device not found"}), 404

    return jsonify(device), 200


@app.route("/devices", methods=["GET"])
def list_devices():
    return jsonify(list(devices.values())), 200


@app.route("/health", methods=["GET"])
def health():
    return jsonify({"status": "ok", "devices": len(devices)}), 200

def get_local_ip():
    try:
        # Create a UDP socket
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # Connect to an external address (doesn't have to be reachable)
        s.connect(('8.8.8.8', 80))
        ip_address = s.getsockname()[0]
        s.close()
        return ip_address
    except Exception:
        return "Unable to determine IP. Ensure you are connected to a network."

SERVER_PORT = 3000
SERVER_IP = get_local_ip()


    
@app.route("/config", methods=["POST"])
def get_device_config():
    data = request.get_json(silent=True) or {}
    device_id = data.get("deviceId", "")
    ssid = data.get("wifiSsid","")
    password = data.get("wifiPassword","")
    print("ipv4:",SERVER_IP)
    lines = [
             '#ifndef CONFIG_H\n',
             '#define CONFIG_H\n',
             f'#define WIFI_SSID  "{ssid}"\n',
             f'#define WIFI_PASSWORD  "{password}"\n',
             f'#define SERVER_IP "{SERVER_IP}"\n',
             f'#define SERVER_PORT    "{SERVER_PORT}"\n',
             f'#define DEVICE_ID      "{device_id}"\n',
             f'#define API_URL "http://{SERVER_IP}:{SERVER_PORT}/wifi/rssi"\n',
             '#define RSSI_THRESHOLD 5\n',
             '#define CONFIG_HEARTBEAT_MS 10000\n',
             '#define CONFIG_SCAN_INTERVAL_MS 100\n',
             '#endif'
             ]
    with open("config.h","w") as file:
        file.writelines(lines)
    return jsonify({
        "status": "success",
        "deviceId": device_id,
        "wifiSsid": ssid,        # ← was hardcoded
        "wifiPassword": password, # ← was hardcoded
        "serverIp": SERVER_IP,
        "serverPort": SERVER_PORT,
        "apiUrl": f"http://{SERVER_IP}:{SERVER_PORT}/wifi/rssi",
        "rssiThreshold": 5,
        "heartbeatMs": 10000,
        "scanIntervalMs": 100
    }), 200
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=SERVER_PORT, debug=True)