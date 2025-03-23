#include <PubSubClient.h>
#include <WiFi.h>
#include <coap-simple.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
// Thông tin server và devices
IPAddress server(103, 116, 39, 179);  // Địa chỉ IP ThingsBoard server, có thể thay bằng địa chỉ cục bộ nếu cần
const int mqttPort = 1883;  // Port mặc định của MQTT
const char* accessToken = "r8kl21QibgyEQGNkCK25";  // Access Token của thiết bị
const char* controlToken = "F5QSSUFXA6ta7vIYdZCw"; // Access Token của lệnh điều khiển
String httpUrl = String("http://c7.hust-2slab.org/api/v1/") + accessToken + "/telemetry";
String http_rpcUrl = String("http://c7.hust-2slab.org/api/v1/") + controlToken + "/rpc";
const char* localUrl = "http://192.168.32.103:5000/data";
const char* rpcUrl = "http://192.168.32.103:5000/rpc";
const char* connectionUrl = "http://192.168.32.103:5000/check_connection";
const char* resendUrl = "http://192.168.32.103:5000/resend";
#define humidity_pin 26
#define light_pin 27
WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP udp;
Coap coap(udp);
HTTPClient http;
HTTPClient http_rpc;
void callback_response(CoapPacket &packet, IPAddress ip, int port);
void setup() {
  Serial.begin(115200);

  // Kết nối Wi-Fi
  delay(1000);

  // Bước 1: Quét và hiển thị các mạng Wi-Fi
  Serial.println("Bắt đầu quét Wi-Fi...");
  int networkCount = WiFi.scanNetworks();  // Quét mạng Wi-Fi xung quanh
  Serial.println("Quét hoàn tất.");

  if (networkCount == 0) {
    Serial.println("Không tìm thấy mạng Wi-Fi nào.");
  } else {
    Serial.println("Các mạng Wi-Fi khả dụng:");
    for (int i = 0; i < networkCount; ++i) {
      // Hiển thị danh sách các mạng Wi-Fi
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));       // Tên Wi-Fi
      Serial.print(" (Tín hiệu: ");
      Serial.print(WiFi.RSSI(i));       // Độ mạnh tín hiệu
      Serial.println(" dBm)");
      delay(10);
    }
  }

  // Bước 2: Chọn Wi-Fi
  int choice = 0;
  Serial.println("Nhập số thứ tự của Wi-Fi muốn kết nối:");
  while (Serial.available() == 0);  // Chờ nhập số thứ tự
  choice = Serial.parseInt();  // Đọc số thứ tự Wi-Fi muốn chọn
  choice -= 1;  // Trừ 1 để dùng làm chỉ số mảng

  if (choice < 0 || choice >= networkCount) {
    Serial.println("Số thứ tự không hợp lệ!");
    return;
  }

  String ssid = WiFi.SSID(choice);  // Lấy tên Wi-Fi đã chọn
  String password;

  // Bước 3: Nhập mật khẩu
  Serial.print("Nhập mật khẩu cho Wi-Fi ");
  Serial.print(ssid);
  Serial.println(": ");
  
  // Chờ cho đến khi người dùng nhập mật khẩu và nhấn Enter
  while (password.length() == 0) {
    if (Serial.available() > 0) {
      password = Serial.readStringUntil('\n');  // Đọc đến khi nhấn Enter
      password.trim();  // Xóa khoảng trắng
    }
  }

  // Bước 4: Kết nối Wi-Fi
  Serial.print("Đang kết nối với Wi-Fi...");
  WiFi.begin(ssid.c_str(), password.c_str());  // Bắt đầu kết nối Wi-Fi
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nKết nối thành công!");
  Serial.print("Địa chỉ IP: ");
  Serial.println(WiFi.localIP());  // Hiển thị địa chỉ IP khi kết nối thành công
  pinMode(humidity_pin, OUTPUT);
  pinMode(light_pin, OUTPUT);

  // Thiết lập MQTT server và cổng
  client.setServer(server, mqttPort);
  
  // Kết nối MQTT
  mqttConnect();
  coap.response(callback_response);
}

void mqttConnect() {
  Serial.print("Đang kết nối MQTT...");
  while (!client.connected()) {
    if (client.connect("ESP32_Device", accessToken, "")) {  // Kết nối với access token làm username
      Serial.println("Đã kết nối MQTT!");
    } else {
      Serial.print("\nLỗi kết nối MQTT. Mã lỗi: ");
      Serial.print(client.state());
      Serial.print("\n");
      break;
      delay(2000);  // Thử lại sau 2 giây
    }
  }
}

void sendTemperature(float temperature) { // giao thức MQTT
  if (!client.connected()) {
    mqttConnect();  // Kết nối lại nếu mất kết nối
  }

  // JSON data để gửi lên ThingsBoard
  String payload = "{";
  payload += "\"temperature\":" + String(temperature);
  payload += "}";
  // Gửi dữ liệu đến topic "v1/devices/me/telemetry" của ThingsBoard
  if (client.publish("v1/devices/me/telemetry", payload.c_str())) {
    //Serial.println("Dữ liệu đã được gửi!");
    Serial.print("temperature: ");
    Serial.print(temperature);
    Serial.print(" Celsius; ");
  } else {
    Serial.println("Gửi dữ liệu thất bại.");
  }
}

void sendHumidity(float humidity) { // giao thức CoAP
    String url = String("api/v1/") + accessToken + String("/telemetry");
    String coap_rpc_url = String("api/v1/") + controlToken + String("/rpc");
    String payload = String("{\"humidity\":") + String(humidity) + String("}");
    String coap_rpc = R"({"method": "getCurrentHumidity","params":{}})";

    Serial.print("humidity: ");
    Serial.print(humidity);
    Serial.print(" %; ");
    
    coap.post(server, 5683, url.c_str(), payload.c_str());
    int responseCode = coap.post(server, 5683, coap_rpc_url.c_str(), coap_rpc.c_str());
    //Serial.print("Response Code: ");
    //Serial.println(responseCode);
}

void sendLight(float light){ // giao thức HTTP
    if (WiFi.status() == WL_CONNECTED) {

    //String url = String("https://demo.thingsboard.io/api/v1/") + accessToken + "/telemetry";
    String jsonData =  "{";
    jsonData += "\"light\":" + String(light);
    jsonData += "}";

    http.begin(httpUrl);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(jsonData);

    //Serial.print("HTTP Response Code: ");
    //Serial.println(httpResponseCode);

    if (httpResponseCode > 0) {
      //String response = http.getString();
      //Serial.println("Response:");
      //Serial.println(response);
      Serial.print("light: ");
      Serial.print(light);
      Serial.println(" lux");
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }
    http_rpc.begin(http_rpcUrl);
    http_rpc.addHeader("Content-Type", "application/json");
    String light_rpc =  R"({"method": "getCurrentLight", "params": {}})";
    http_rpc.POST(light_rpc);
    String light_signal = http_rpc.getString();
    Serial.print(light_signal);
    if (light_signal == R"({"action":"on"})"){
      digitalWrite(light_pin, HIGH);
    }
    else{
      digitalWrite(light_pin, LOW);
    }
    http_rpc.end();  // Đóng kết nối
    http.end();
  } else {
    Serial.println("WiFi không kết nối!");
  }
}
int pre_check = 1;
int after_check = 1;
float temperature_last;
float humidity_last;
float light_last;
void loop() {
  // Giả sử dữ liệu là nhiệt độ 
  float temperature = random(20, 30);  // Giá trị ngẫu nhiên để test
  float humidity = random(0, 101); // Random value between 0 and 100
  float light = random(10, 50);
  mqttConnect();
  if (!client.connected()){
    pre_check = 1;
    after_check = 0;
    Serial.print("temperature: ");
    Serial.print(temperature);
    Serial.print(" Celsius; ");
    Serial.print("humidity: ");
    Serial.print(humidity);
    Serial.print(" %; ");
    Serial.print("light: ");
    Serial.print(light);
    Serial.println(" lux");
    temperature_last = temperature;
    humidity_last = humidity;
    light_last = light;
    controller(temperature, humidity, light);
    //resend(temperature, humidity, light);
  }
  else{
    if (pre_check - after_check == 1){
      resend(temperature_last, humidity_last, light_last);
      int exchange;
      exchange = pre_check;
      pre_check = after_check;
      after_check = exchange;
    }
    client.loop();  // Duy trì kết nối MQTT
  // Gửi dữ liệu
    sendTemperature(temperature);
    sendHumidity(humidity);
    sendLight(light);
    coap.loop();
  }
  coap.loop();
  delay(5000);  // Gửi mỗi 5 giây
  client.loop();  // Duy trì kết nối MQTT
}
void callback_response(CoapPacket &packet, IPAddress ip, int port)
{
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = '\0';

  //pumpControlSignal = p[10];
  Serial.println(p);
  StaticJsonDocument<20> doc;
  DeserializationError error = deserializeJson(doc, p);
  String signal = doc["turn"].as<String>();
  if (signal == "on"){
    digitalWrite(humidity_pin, HIGH);
  }
  else{
    digitalWrite(humidity_pin, LOW);
  }
}

void check_connection(String connection_state){
  HTTPClient http;
  http.begin(connectionUrl);
  //http.addHeader("Content-Type", "application/json");
  //String payload = "{\"command\":\"get_status\"}";
  int httpResponseCode = http.GET();
  String response = http.getString();
  StaticJsonDocument<50> doc;
  DeserializationError error = deserializeJson(doc, response);
  connection_state = doc["connection"].as<String>();
  http.end();
}

void controller(int temperature, int humidity, int light){
  HTTPClient http;
  http.begin(localUrl);
  http.addHeader("Content-Type", "application/json");
  String payload =  "{";
  payload += "\"temperature\":" + String(temperature);
  payload += ",";
  payload += "\"humidity\":" + String(humidity);
  payload += ",";
  payload += "\"light\":" + String(light);
  payload += "}";
  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Phản hồi từ server: " + response);
    StaticJsonDocument<100> doc;
    DeserializationError error = deserializeJson(doc, response);
    String humidity_signal = doc["turn"].as<String>();
    String light_signal = doc["action"].as<String>();
    if (humidity_signal == "on"){
      digitalWrite(humidity_pin, HIGH);
      //Serial.println("Bật");
    }
    else{
      digitalWrite(humidity_pin, LOW);
      //Serial.println("Tắt");
    }
    if (light_signal == "on"){
      digitalWrite(light_pin, HIGH);
      //Serial.println("Bật");
    }
    else{
      digitalWrite(light_pin, LOW);
      //Serial.println("Tắt");
    }
  } else {
    Serial.println("Không gửi được dữ liệu.");
  }
  http.end();

  //delay(5000);
}

void resend(int temperature, int humidity, int light){
  HTTPClient http;
  http.begin(resendUrl);
  http.addHeader("Content-Type", "application/json");
  String payload =  "{";
  payload += "\"temperature\":" + String(temperature);
  payload += ",";
  payload += "\"humidity\":" + String(humidity);
  payload += ",";
  payload += "\"light\":" + String(light);
  payload += "}";
  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println(response);
  } else {
    Serial.println("Không gửi được dữ liệu.");
  }
  http.end();

  //delay(5000);
}
