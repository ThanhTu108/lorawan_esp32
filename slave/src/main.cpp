#include <Arduino.h>
#include <LoRa.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#define ss 5 
#define rst 17
#define dio0 16

#define DHTPIN 4      
#define DHTTYPE DHT11  
#define FAN_PIN 25
#define LIGHT_PIN 26
#define MOTOR_PIN 27
DHT dht(DHTPIN, DHTTYPE); 
byte SL_Address;
String GetValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;
  
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
int fan_control = 0, light_control = 0,motor_control = 0, h;
float t;
byte master = 0x01;        
byte slave_1 = 0x03; 
String Incoming = "";
String Message = ""; 
unsigned long previousMillis_SendMSG = 0;
const long interval_SendMSG = 2000;
byte sender;
void sendMessage(String Outgoing, byte Destination) {
  LoRa.beginPacket();          
  LoRa.write(Destination);     
  LoRa.write(slave_1);        
  LoRa.write(Outgoing.length());  
  LoRa.print(Outgoing);           
  LoRa.endPacket();               

  Serial.print("Gửi từ địa chỉ: ");
  Serial.print(slave_1);
  Serial.print(", Đến địa chỉ: ");
  Serial.print(Destination);
  Serial.print(", Nội dung: ");
  Serial.print(Outgoing);
  Serial.print(", Độ dài: ");
  Serial.println(Outgoing.length());
}
void control()
{
  if (fan_control == 1) {
        Serial.println("Quạt BẬT");
        digitalWrite(FAN_PIN, HIGH);  
    } else {
        Serial.println("Quạt TẮT");
        digitalWrite(FAN_PIN, LOW); 
    }


    if (light_control == 1) {
        Serial.println("Đèn BẬT");
        digitalWrite(LIGHT_PIN, HIGH);
    } else {
        Serial.println("Đèn TẮT");
        digitalWrite(LIGHT_PIN, LOW);  
    }

    // Điều khiển motor
    if (motor_control == 1) {
        Serial.println("Motor BẬT");
        digitalWrite(MOTOR_PIN, HIGH); 
    } else {
        Serial.println("Motor TẮT");
        digitalWrite(MOTOR_PIN, LOW);  // Tắt motor
    }
}
void Processing_incoming_data() {
    fan_control = GetValue(Incoming, ',', 0).toInt();
    light_control = GetValue(Incoming, ',', 1).toInt();
    motor_control = GetValue(Incoming, ',', 2).toInt();
    Serial.println("Fan: " + String(fan_control));
    Serial.println("L: " + String(fan_control));
    Serial.println("M: " + String(fan_control));
    control();
}
void onReceive(int packetSize) {
  if (packetSize == 0) return;        
  int recipient = LoRa.read();     
  byte sender = LoRa.read();         

  byte incomingLength = LoRa.read();
  if (sender != master) {
    Serial.println();
    Serial.println("i"); 
    return; 
  }
  Incoming = "";

  while (LoRa.available()) {
    Incoming += (char)LoRa.read();
  }

  if (incomingLength != Incoming.length()) {
    Serial.println();
    Serial.println("er");
    return;
  }
  if (recipient != slave_1) {
    Serial.println();
    Serial.println("!");
    return; 
  } else {
    Processing_incoming_data(); 
    Serial.println();
    Serial.println("Rc from: 0x" + String(sender, HEX));
    Serial.println("Message: " + Incoming);
  }
}
void setup() {
  Serial.begin(115200);
  LoRa.setPins(ss, rst, dio0);  
  Serial.println("start");
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  while (!LoRa.begin(433E6)) {  
    Serial.println("try");
    delay(1000); 
  }
  Serial.println("OKe");
  dht.begin();
}

void loop() {
  unsigned long currentMillis_SendMSG = millis();  
  if (currentMillis_SendMSG - previousMillis_SendMSG >= interval_SendMSG) {
    previousMillis_SendMSG = currentMillis_SendMSG; 

  t = dht.readTemperature();
  t=round(t);
  h = dht.readHumidity();
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
    }
  Message = String(t)+',' + String(h); 
  sendMessage(Message, master); 
  }
  int packetSize = LoRa.parsePacket();
    if (packetSize) {
        onReceive(packetSize);  
        Processing_incoming_data();
    }
}
