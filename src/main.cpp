/*
  This sketch is based on the default WifiClient example that comes with the Arduino ESP core.
  And was adapted for use with the ESPNexUpload library.

  You need to change wifi cred, host(server) and url (file name)

  Created on: 19 Dec 2018
  by Onno Dirkzwager
*/

#if defined ESP8266
  #include <ESP8266WiFi.h>
#elif defined ESP32
  #include <WiFi.h>
#endif

#include <ESPNexUpload.h>
#include <esp_task_wdt.h>
#include <ArduinoOTA.h>

/*
  ESP8266 uses Software serial RX:5, TX:4 Wemos D1 mini RX:D1, TX:D2 
  ESP32 uses Hardware serial RX:16, TX:17
  Serial pins are defined in the ESPNexUpload.cpp file
*/

const char* ssid      = "EspShelly";
const char* password  = "@motdepasseanepasoublier";
const char* host      = "192.168.1.99";
// path to access file is /var/www/ 
const char* url       = "/nextion/sejour.tft";
bool updated          = false;

// name of the device for OTA update
const char *ESPNAME = "esp32_sejour";

//watchdog timer
#define WDT_TIMEOUT 30 //watchdog after 30s


void setup(){
  Serial.begin(115200);

  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);               //add current thread to WDT watch

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Explicitly set the ESP to be a WiFi-client, otherwise, it by default,
  // would try to act as both a client and an access-point and could cause
  // network-issues with your other WiFi-devices on your WiFi-network.
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for Wifi connection to establish
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  // Report connection details
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("\n");

  ArduinoOTA.setHostname(ESPNAME);

  ArduinoOTA
      .onStart([]()
               {
                   String type;
                   if (ArduinoOTA.getCommand() == U_FLASH)
                       type = "sketch";
                   else // U_SPIFFS
                       type = "filesystem";

                   // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                   Serial.println("Start updating " + type);
                   //esp_task_wdt_deinit();
                   esp_task_wdt_init(WDT_TIMEOUT, false);
               })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
                   Serial.printf("Error[%u]: ", error);
                   if (error == OTA_AUTH_ERROR)
                       Serial.println("Auth Failed");
                   else if (error == OTA_BEGIN_ERROR)
                       Serial.println("Begin Failed");
                   else if (error == OTA_CONNECT_ERROR)
                       Serial.println("Connect Failed");
                   else if (error == OTA_RECEIVE_ERROR)
                       Serial.println("Receive Failed");
                   else if (error == OTA_END_ERROR)
                       Serial.println("End Failed");
               });

  ArduinoOTA.begin();

  Serial.println("OTA Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


void loop(){
  delay(2000);
  ArduinoOTA.handle();
  esp_task_wdt_reset(); //reinit watchdog
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
      ESP.restart();
  }
    Serial.print("in loop - ");
    Serial.println(updated);
  if(!updated){
    Serial.print("connecting to ");
    Serial.println(host);
  
    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(host, httpPort)){
      Serial.println("connection failed");
      //return;
    } else 
    {
        Serial.print("Requesting URL: ");
        Serial.println(url);
    
        // This will send the (get) request to the server
        client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
        unsigned long timeout = millis();
        while(client.available() == 0){
            if(millis() - timeout > 5000){
                Serial.println(">>> Client Timeout !");
                client.stop();
                break;
            }
        }

        // Scan reply header for succes (code 200 OK) and the content lenght
        int contentLength;
        int code;
        while(client.available()){
  
            String line = client.readStringUntil('\n');
            Serial.println(line); // Read all the lines of the reply from server and print them to Serial
      
            if(line.startsWith("HTTP/1.1 ")){
                line.remove(0, 9);
                code = line.substring(0, 3).toInt();
  
                if(code != 200){
                    line.remove(0, 4);
                    Serial.println(line);
                    break;
                }
  
            } else if(line.startsWith("Content-Length: ")){
                line.remove(0, 16);
                contentLength = line.toInt();
  
            } else if(line == "\r"){
                line.trim();
                break;
            } 
        }

        // Update the nextion display
        if(code == 200){
            Serial.println("File received. Update Nextion...");

            bool result;

            // initialize ESPNexUpload
            ESPNexUpload nextion(115200);

            // set callback: What to do / show during upload.... Optional!
            nextion.setUpdateProgressCallback([](){
                Serial.print(".");
                esp_task_wdt_reset(); });
                // prepare upload: setup serial connection, send update command and send the expected update size
                result = nextion.prepareUpload(contentLength);

                if (!result)
                {
                    Serial.println("Error: " + nextion.statusMessage);
            }else{
                Serial.print(F("Start upload. File size is: "));
                Serial.print(contentLength);
                Serial.println(F(" bytes"));
          
                // Upload the received byte Stream to the nextion
                result = nextion.upload(client);
          
                if(result){
                    updated = true;
                    Serial.println("\nSuccesfully updated Nextion!");
                }else{
                    Serial.println("\nError updating Nextion: " + nextion.statusMessage);
                }
                // end: wait(delay) for the nextion to finish the update process, send nextion reset command and end the serial connection to the nextion
                nextion.end();
            }
        }
        Serial.println("Closing connection\n");
    }
  }
}
