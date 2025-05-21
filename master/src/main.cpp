#include <Arduino.h>
#include <LoRa.h>
#include <WiFi.h>
#include <SPI.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>

const char *ssid_ap = "ESP32_Config";
const char *password_ap = "123456789";
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1); 
IPAddress subnet(255, 255, 255, 0); 
String server_url = "http://172.20.10.2:5000/";
WebServer server(80);  
Preferences preferences;
#define ss 5
#define rst 17
#define dio0 16
byte master = 0x01;
byte slave_1 = 0x02;
byte slave_2 = 0x03;
String Incoming = "";
String Message = "";

unsigned long previousSendMillis = 0;  
unsigned long previousReceiveMillis = 0; 
const long sendInterval = 1000;          
const long receiveInterval = 2000;     
String id_sp = "43";                    
int node_id_slave1 = 42;
int node_id_slave2 = 43;
int node1_temperature;
float node1_humidity;
float node2_temperature;
float node2_humidity;
byte SL_Address;
String GetValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++)
    {
        if (data.charAt(i) == separator || i == maxIndex)
        {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
void handleRoot() {
    String html = "<html><body><h1>ESP32 WiFi Configuration</h1>";
    html += "<form action=\"/setWiFi\" method=\"GET\">";
    html += "SSID: <input type=\"text\" name=\"ssid\" value=\"" + preferences.getString("ssid", "") + "\"><br>";
    html += "Password: <input type=\"text\" name=\"password\" value=\"" + preferences.getString("password", "") + "\"><br>";
    html += "<input type=\"submit\" value=\"Set WiFi\">";
    html += "</form><br>";
    server.send(200, "text/html", html);
}

void handle_Set() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    if (ssid != "" && password != "") {
        WiFi.begin(ssid.c_str(), password.c_str());

        unsigned long startMillis = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startMillis < 10000) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConnected to new WiFi!");
            preferences.putString("ssid", ssid);  
            preferences.putString("password", password);  
            server.send(200, "text/html", "<html><body><h1>WiFi Configured Successfully</h1></body></html>");
        } else {
            Serial.println("\nFailed to connect to the new WiFi!");
            server.send(200, "text/html", "<html><body><h1>WiFi Configuration Failed</h1></body></html>");
        }
    } else {
        server.send(400, "text/html", "<html><body><h1>Error: SSID and Password are required!</h1></body></html>");
    }
}
void Processing_incoming_data()
{
    if (SL_Address == slave_1)
    {
        node1_temperature = GetValue(Incoming, ',', 0).toInt();
        node1_humidity = GetValue(Incoming, ',', 1).toFloat();
        Serial.println("Node 1 - Temperature: " + String(node1_temperature));
        Serial.println("Node 1 - Humidity: " + String(node1_humidity));
    }
    if (SL_Address == slave_2)
    {
        node2_temperature = GetValue(Incoming, ',', 0).toInt();
        node2_humidity = GetValue(Incoming, ',', 1).toFloat();
        Serial.println("Node 2 - Temperature: " + String(node2_temperature));
        Serial.println("Node 2 - Humidity: " + String(node2_humidity));
    }
}

void send_data()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;

        http.begin(server_url + String("receive_data"));
        http.addHeader("Content-Type", "application/json");

        String payload = "{\"id_sp\":\"" + id_sp + "\", \"nodes\":[{\"node_id\": 42, \"temperature\":" + String(node1_temperature) + ", \"humidity\":" + String(node1_humidity) + "}, {\"node_id\": 43, \"temperature\":" + String(node2_temperature) + ", \"humidity\":" + String(node2_humidity) + "}]}" ;

        Serial.println("Sending payload: " + payload); 

        unsigned long startMillis = millis(); 
        int httpResponseCode = -1;
        
        while (httpResponseCode == -1 && millis() - startMillis < 10000)
        {
            httpResponseCode = http.POST(payload);

            if (httpResponseCode > 0)
            {
                String response = http.getString(); 
                Serial.println("HTTP Response Code: " + String(httpResponseCode));
                Serial.println("Response: " + response);
                break; 
            }
            delay(500); 
        }

        if (httpResponseCode <= 0)
        {
            Serial.println("Error on sending POST: ");
            Serial.println(httpResponseCode);
        }

        http.end(); 
    }
    else
    {
        Serial.println("WiFi not connected");
    }
}

void onReceive(int packetSize)
{
    if (packetSize == 0)
        return;                     
    int recipient = LoRa.read();      
    byte sender = LoRa.read();    
    byte incomingLength = LoRa.read(); 
    SL_Address = sender;
    Serial.print(SL_Address);
    Incoming = "";

    while (LoRa.available())
    {
        Incoming += (char)LoRa.read();
    }

    if (incomingLength != Incoming.length())
    {
        Serial.println();
        Serial.println("er"); 
        return; 
    }
    if (recipient != master)
    {
        Serial.println();
        Serial.println("!");
        Serial.println("This message is not for me.");
        return; //--> skip rest of function
    }
    Serial.println();
    Serial.println("Rc from: 0x" + String(sender, HEX));
    Serial.println("Message: " + Incoming);
    Processing_incoming_data();
}
void sendMessage(String Outgoing, byte Destination)
{
    LoRa.beginPacket();
    LoRa.write(Destination);
    LoRa.write(master);
    LoRa.write(Outgoing.length());
    LoRa.print(Outgoing);
    LoRa.endPacket();

    Serial.print("Gửi từ địa chỉ: ");
    Serial.print(master);
    Serial.print(", Đến địa chỉ: ");
    Serial.print(Destination);
    Serial.print(", Nội dung: ");
    Serial.print(Outgoing);
    Serial.print(", Độ dài: ");
    Serial.println(Outgoing.length());
}
void receive_data()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;

        http.begin(server_url + "/send_data");
        http.addHeader("Content-Type", "application/json"); 

        DynamicJsonDocument jsonDoc(256);
        jsonDoc["id_sp"] = id_sp.toInt();

        String requestBody;
        serializeJson(jsonDoc, requestBody);

        int httpResponseCode = http.POST(requestBody); 

        if (httpResponseCode > 0)
        {
            String response = http.getString();
            Serial.println("HTTP Response Code: " + String(httpResponseCode));
            Serial.println("Response: " + response); 

            // Xử lý phản hồi JSON nhận được
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, response);

            if (error)
            {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.f_str());
                return;
            }

            JsonArray nodes = doc["nodes"].as<JsonArray>();
            for (JsonObject node : nodes)
            {
                int node_id = node["node_id"];
                int fan_control = node["fan_control"];
                int light_control = node["light_control"];
                int motor_control = node["motor_control"];
                if (node_id == node_id_slave1)
                {
                    String message = String(fan_control) + ',' + String(light_control) + ',' + String(motor_control);
                    sendMessage(message, slave_1);
                }
                else if (node_id == node_id_slave2)
                {
                    String message = String(fan_control) + ',' + String(light_control) + ',' + String(motor_control);
                    sendMessage(message, slave_2);
                }
            }
        }
        else
        {
            Serial.print("Error on sending POST: ");
            Serial.println(httpResponseCode);
        }

        http.end();
    }
    else
    {
        Serial.println("WiFi not connected");
    }
}

void setup()
{
    Serial.begin(115200);
    preferences.begin("wifi-config", false); 
    String saved_ssid = preferences.getString("ssid", "");
    String saved_password = preferences.getString("password", ""); 
    if (saved_ssid != "" && saved_password != "") {
        WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
        unsigned long startMillis = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startMillis < 10000) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConnected to WiFi: " + saved_ssid);
        } else {
            Serial.println("\nFailed to connect to the WiFi, switching to Access Point mode...");
            WiFi.softAP(ssid_ap, password_ap);
            WiFi.softAPConfig(local_ip, gateway, subnet);
            Serial.println("AP Mode Started");
        }
    } else {

        WiFi.softAP(ssid_ap, password_ap);
        WiFi.softAPConfig(local_ip, gateway, subnet);
        Serial.println("AP Mode Started");
    }

    server.on("/", HTTP_GET, handleRoot);
    server.on("/setWiFi", HTTP_GET, handle_Set);
    server.begin();
    Serial.println("Web server started");
    // Khởi tạo LoRa
    LoRa.setPins(ss, rst, dio0);
    Serial.println("Start LoRa init...");
    while (!LoRa.begin(433E6))
    {
        Serial.println("Reconnecting to LoRa...");
        delay(1000);
    }
    Serial.println("LoRa init succeeded.");
}

void loop()
{
    unsigned long currentMillis = millis(); 

    int packetSize = LoRa.parsePacket();
    if (packetSize)
    {                                     
        onReceive(packetSize);            
        Processing_incoming_data();      
        if (currentMillis - previousSendMillis >= sendInterval)
        {
            previousSendMillis = currentMillis; 
            send_data();                   
        }
    }
    if (currentMillis - previousReceiveMillis >= receiveInterval)
    {
        previousReceiveMillis = currentMillis; 
        receive_data();                     
    }
    server.handleClient();
}