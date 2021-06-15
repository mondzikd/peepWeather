#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"

#include "PMS.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <LiquidCrystal.h>
//#include "BluetoothSerial.h"

//#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
//#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
//#endif

//BluetoothSerial SerialBT;
PMS pms(Serial2);
PMS::DATA data;
Adafruit_BME280 bme;
LiquidCrystal lcd(19, 23, 5, 4, 2, 15);

#define AWS_IOT_PUBLISH_TOPIC   "peep/user1/esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "peep/user1/esp32/sub"

WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);


void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  client.onMessage(messageHandler);

  Serial.print("Connecting to AWS IOT");
  
  client.setKeepAlive(65);

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(100);
  }

  if(!client.connected()){
    Serial.println("AWS IoT Timeout!");
    return;
  }

  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

  Serial.println("AWS IoT Connected!");
}

void printLcd(float temperature, float humidity, float pressure) {
  lcd.setCursor(0, 0);
  lcd.print(millis() / 1000);
  lcd.setCursor(8, 0);
  lcd.print((String)temperature + "*C");

  lcd.setCursor(0, 1);
  lcd.print((String)humidity + "%");
  lcd.setCursor(8, 1);
  lcd.print((String)pressure + "hPa");
}

void publishMessage()
{ 
  float temperature, humidity, pressure;
  StaticJsonDocument<200> doc;

  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;
  
  if (pms.readUntil(data, 1000)) {
    doc["PM_AE_UG_1_0"] = data.PM_AE_UG_1_0;
    doc["PM_AE_UG_2_5"] = data.PM_AE_UG_2_5;
    doc["PM_AE_UG_10_0"] = data.PM_AE_UG_10_0;
  } else {
    Serial.println("No data.");
  }
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["pressure"] = pressure;
  
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);
  
  Serial.println(jsonBuffer);
  printLcd(temperature, humidity, pressure);
//  SerialBT.print(jsonBuffer);
  
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void messageHandler(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);
}

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  lcd.begin(16, 2);
//  SerialBT.begin("peepWeather");
  
  bool status = bme.begin(0x76);
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  
  connectAWS();
}

void loop() {
  publishMessage();
  
  client.loop();
  if(!client.connected()){
    Serial.println("AWS disconnected. Connecting again.");
    connectAWS();
  }
  
  delay(60000);
}
