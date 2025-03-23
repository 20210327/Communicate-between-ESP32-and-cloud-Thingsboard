from flask import Flask, request, jsonify
import requests
import json

THINGSBOARD_HOST = "https://c7.hust-2slab.org"  # Thay bằng địa chỉ ThingsBoard của bạn
ACCESS_TOKEN = "lkUXFNXWIQnLpkQ2jfI2"  # Thay bằng Access Token của thiết bị

app = Flask(__name__)
esp32_data = {}
count = 1  # Biến đếm, bắt đầu từ 1
# Xử lý dữ liệu từ ESP32
@app.route('/data', methods=['POST'])
def handle_data():
    data = request.json
    print("Dữ liệu nhận:", data)
    global esp32_data
    global count;
    # Thêm dữ liệu với chỉ số
    esp32_data[f"temperature_{count}"] = data.get("temperature")
    esp32_data[f"humidity_{count}"] = data.get("humidity")
    esp32_data[f"light_{count}"] = data.get("light")
    count += 1  # Tăng chỉ số sau mỗi lần thêm

    # Append dữ liệu mới với timestamp vào danh sách
    temperature = data.get("temperature")
    humidity = data.get("humidity")
    light = data.get("light")
    if light >= 25 and humidity >= 50:
        return jsonify({"turn": "off","action": "off"})
    elif light <25 and humidity >= 50:
        return jsonify({"turn": "off","action":"on"})
    elif light <25 and humidity < 50:
        return jsonify({"turn":"on","action":"on"})
    else:
        return jsonify({"turn":"on","action":"off"})

# Xử lý RPC từ ESP32
@app.route('/rpc', methods=['POST'])
def handle_rpc():
    command = request.json.get('command')
    print("Yêu cầu RPC:", command)

    # Phản hồi tùy theo yêu cầu
    if command == "get_status":
        return jsonify({"status": "running", "uptime": "120s"})
    else:
        return jsonify({"error": "unknown command"}), 400

# Kiểm tra kết nối với Google
@app.route('/check_connection', methods=['GET'])
def check_connection():
    try:
        # Gửi yêu cầu HTTP GET lên Google
        response = requests.get("https://www.google.com", timeout=5)

        # Phản hồi dựa trên trạng thái kết nối
        if response.status_code == 200:
            print("Kết nối mạng ổn định!")
            return jsonify({"connection": "ok"})
        else:
            print(f"Kết nối mạng không thành công, mã lỗi: {response.status_code}")
            return jsonify({"connection": "failed"}), 500
    except requests.exceptions.RequestException as e:
        print(f"Lỗi khi kiểm tra kết nối mạng: {e}")
        return jsonify({"connection": "failed"}), 500
@app.route('/resend', methods=['POST'])
def resend_data():
    global esp32_data
    data = request.json
    # URL API để gửi dữ liệu
    url = f"{THINGSBOARD_HOST}/api/v1/{ACCESS_TOKEN}/telemetry"
    # Gửi dữ liệu bằng POST request
    headers = {"Content-Type": "application/json"}
    esp32_data_json = json.dumps(esp32_data, ensure_ascii=False)
    response = requests.post(url, data=esp32_data_json, headers=headers)
    # Kiểm tra phản hồi
    if response.status_code == 200:
        print("Dữ liệu đã được gửi thành công!")
    else:
        print(f"Lỗi: {response.status_code}, Nội dung: {response.text}")
    del esp32_data
    return jsonify({"call resend": 1})
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False)
