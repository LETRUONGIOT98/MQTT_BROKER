#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define DHTPIN 25
#define DHTTYPE DHT22 // DHT 22
DHT dht(DHTPIN, DHTTYPE);
String ttbom;
#define WIFI_SSID "MINHPHUONG"  //tên wifi
#define WIFI_PASSWORD "mp210999"  ///mật khẩu


WiFiClient espClient;
PubSubClient client(espClient);

// Khai báo địa chỉ I2C của mô-đun LCD và số cột và hàng của màn hình
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

// Khởi tạo đối tượng LiquidCrystal_I2C
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

#define BT 15
#define Mode 16
#define bom 26
String chedo = "Auto";
bool tt = true;
int set = 70; // Set độ ẩm bật tắt bơm tự động
float h, t;
#define MQTT_SERVER "broker.emqx.io"
const char *mqtt_server = MQTT_SERVER;
const int mqtt_port = 1883;
const char *mqtt_user = "admin";
const char *mqtt_password = "12345678";
const char *mqtt_clientId = "ESP32Client";
const char *topic_publish = "ESP32/giatri";
//const char *topic_subscribe = "ESP32/dieukhien";
long times,time1;
#define doamdat 33
String dat = " ";
void setup() {
  Serial.begin(115200);
  pinMode(BT, INPUT);
  pinMode(doamdat, INPUT);
  pinMode(Mode, INPUT);
  pinMode(bom, OUTPUT);
  digitalWrite(bom, LOW);
  setupWifi();
  client.setServer(mqtt_server, mqtt_port);
  //client.setCallback(callback);
  dht.begin();
  // Khởi tạo mô-đun LCD
  lcd.init();
  lcd.backlight();
}

void setupWifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    chay();
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_clientId, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(topic_subscribe);
    } else {
      if(millis() - time1 >= 3000){
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 3 seconds");
time1 = millis();
      }
      //delay(5000);
    }
  }
}
void chay() {
  h = dht.readHumidity();
  t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.print("NHIET DO: ");
  Serial.println(t);
  Serial.print("DO AM: ");
  Serial.println(h);
  lcd.clear(); // Xóa màn hình
  lcd.setCursor(0, 0); // Đặt con trỏ về góc trên bên trái
  lcd.print("t: "); // In nhãn nhiệt độ
  lcd.print(t); // In giá trị nhiệt độ
  lcd.print(" S: "); // Đơn vị Celsius
  lcd.print(ttbom); // In giá trị nhiệt độ
  lcd.print(" C"); // Đơn vị Celsius
  lcd.setCursor(0, 1); // Đặt con trỏ xuống dòng thứ hai
  lcd.print("Do am: "); // In nhãn độ ẩm
  lcd.print(h, 0); // In giá trị độ ẩm
  lcd.print(" %  "); // Đơn vị phần trăm
  lcd.print(chedo); // Đơn vị phần trăm

  /////////////Điều khiển thiết bị
  Serial.println(digitalRead(33));
  if (digitalRead(Mode) == 1) {

    tt = !tt;
    delay(300);
  }
  if (tt) {
    chedo = "Auto";
    if (digitalRead(doamdat) == 0) { ////Nếu độ ẩm cao hơn set thì
      digitalWrite(bom, LOW); ///tắt bơm
      dat ="DO AM TOT";
    } else {
      digitalWrite(bom, HIGH); ///nếu thấp hơn thì bật bơm
      dat ="DAT KHO";
    }
  } else {
    chedo = "Man ";
    if (digitalRead(BT) == 1) {
      digitalWrite(bom, !digitalRead(bom));
      delay(300);
    }
  }
}
void loop() {
  chay();
  // Tạo đối tượng JSON
  DynamicJsonDocument jsonDocument(200);
  
  if (digitalRead(bom) == 0) ttbom = "TAT";
  else ttbom = "BAT";
  // Điền dữ liệu vào đối tượng JSON
  jsonDocument["Mac"] = WiFi.macAddress();
  jsonDocument["nhiet_do"] = String(t, 2);
  jsonDocument["do_am"] = String(h, 0);
  jsonDocument["Che_do"] = chedo;
  jsonDocument["DAT"] = dat;
  jsonDocument["BOM"] = ttbom;
  
  // Chuyển đổi đối tượng JSON thành chuỗi
  char jsonString[200];
  serializeJson(jsonDocument, jsonString);
  if (millis() - times >= 5000) { ///20s kết nối và đẩy lên server 1 lần
    times = millis();
    if (!client.connected()) {
      reconnect();
    }
    Serial.println(millis() - times);
    client.loop();
    // Gửi chuỗi JSON lên broker MQTT
    client.publish(topic_publish, jsonString);
    
  }
  // Hiển thị dữ liệu lên LCD

}
/*
  void callback(char *topic, byte *payload, unsigned int length) {
  // Convert byte array to string
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Parse JSON
  DynamicJsonDocument jsonDocument(200);
  DeserializationError error = deserializeJson(jsonDocument, message);

  // Check for parsing errors
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  // Check if JSON contains "Mac" and "SET" keys
  if (jsonDocument.containsKey("Mac") && jsonDocument.containsKey("SET")) {
    // Compare MAC address from JSON with MAC address of ESP32
    String macAddress = WiFi.macAddress();
    String receivedMac = jsonDocument["Mac"].as<String>(); // Convert to String
    if (macAddress.equals(receivedMac)) {
      // If MAC addresses match, update the 'set' variable with the value from JSON
      set = jsonDocument["SET"].as<int>();
      chedo = jsonDocument["MODE"].as<String>();
      if(chedo = "THU_CONG"){
        tt = false;
      digitalWrite(bom, jsonDocument["DIEU_KHIEN"].as<int>());}
      if(chedo = "TU_DONG"){
        tt = true;
      }
    }
  }
  }*/
