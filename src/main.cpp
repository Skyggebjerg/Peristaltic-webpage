#include <Arduino.h>
#include "M5AtomS3.h"
#include <M5GFX.h>
#include "Unit_Encoder.h"
#include "M5UnitHbridge.h"

#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

const char* ssid = "ESP32_Access_Point";
const char* password = "12345678";

WebServer server(80);

int ontime;
int forsink;
//uint64_t forsink = 1000; //delay between runs
int save_forsink = 10; // forsink saved in EEPROM as int (forsink divided by 100)
uint64_t tempus;
bool newpress = true; // monitor if button just pressed 
int mstatus = 0; // defines which state the system is in

signed short int last_value = 0;
signed short int last_btn = 1;

M5GFX display;
M5Canvas canvas(&display);
Unit_Encoder sensor;
M5UnitHbridge driver;

void handleRoot() {
    String html = "<html><body style=\"font-size: 18px;\">";
        html += "<meta name=\"viewport\" content=\"width=390, initial-scale=1\"/>";
    html += "<h1 style=\"font-size: 24px;\">Motor Control Settings</h1>";
    html += "<form action=\"/update\" method=\"POST\">";
    html += "Ontime: <input type=\"text\" name=\"ontime\" value=\"" + String(ontime) + "\" style=\"font-size: 18px;\"><br>";
    html += "Forsink: <input type=\"text\" name=\"forsink\" value=\"" + String(forsink) + "\" style=\"font-size: 18px;\"><br>";
    html += "<input type=\"submit\" value=\"Save\" style=\"font-size: 18px;\">";
    html += "</form>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleUpdate() {
    if (server.hasArg("ontime") && server.hasArg("forsink")) {
        ontime = server.arg("ontime").toInt();
        forsink = server.arg("forsink").toInt();

        EEPROM.put(0, ontime);
        EEPROM.put(sizeof(ontime), forsink);
        EEPROM.commit();

        server.send(200, "text/html", "<html><body><h1>Settings Saved</h1><a href=\"/\">Go Back</a></body></html>");
    } else {
        server.send(400, "text/html", "<html><body><h1>Invalid Input</h1><a href=\"/\">Go Back</a></body></html>");
    }
}

void setup() {
    Serial.begin(115200);
    EEPROM.begin(512);
    EEPROM.get(0, ontime);
    EEPROM.get(sizeof(ontime), forsink);
    WiFi.softAP(ssid, password);
    Serial.println("Access Point Started");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", handleRoot);
    server.on("/update", HTTP_POST, handleUpdate);
    server.begin();
    Serial.println("HTTP server started");

    Wire.begin(2, 1);
    auto cfg = M5.config();
    AtomS3.begin(cfg);
    sensor.begin();
    driver.begin(&Wire, HBRIDGE_I2C_ADDR, 2, 1, 100000L);

    AtomS3.Display.setTextColor(WHITE);
    AtomS3.Display.setTextSize(3);
    AtomS3.Display.clear();
    tempus = millis();
}

void loop() {
    server.handleClient();

    bool btn_status = sensor.getButtonStatus();
    if (last_btn != btn_status) {
        if (!btn_status) {
            mstatus = mstatus + 1;
            if (mstatus == 5) mstatus = 0;
            AtomS3.Display.clear();
            AtomS3.Display.drawString(String(mstatus), 10, 100);
            newpress = true;
        }
        last_btn = btn_status;
    }

switch (mstatus) {

        case 0: //run motor
        { 
            if (newpress) {
                AtomS3.Display.drawString("Running", 5, 0);
                newpress = false;
            }
            if (millis() - tempus >= forsink) // to be set by adjustment (100)
            {
                AtomS3.Display.drawString("Running", 5, 0);
                AtomS3.Display.drawString(String(ontime), 10, 30);
                AtomS3.Display.drawString(String(forsink), 10, 60);   
                driver.setDriverDirection(HBRIDGE_FORWARD); // Set peristaltic pump in forward to take out BR content
                //driver.setDriverDirection(HBRIDGE_BACKWARD)
                driver.setDriverSpeed8Bits(127); //Run pump in half speed
                delay(ontime); // to be set by adjustment (30)
                driver.setDriverDirection(HBRIDGE_STOP);
                driver.setDriverSpeed8Bits(0);  //Stop pump
                tempus = millis();
            }
            break;
        } // end of case 0

        case 1: // read encoder for ON time in ms
        {
            signed short int encoder_value = sensor.getEncoderValue();
            //ontime = encoder_value;

            if (newpress) {
                AtomS3.Display.drawString("On time", 5, 0);
                AtomS3.Display.drawString(String(ontime), 10, 30);
                last_value = encoder_value; // Update the last value
                newpress = false;
            }

            if (last_value != encoder_value) {
                int relative_change = encoder_value - last_value; // Calculate the relative change

                AtomS3.Display.setTextColor(BLACK);
                AtomS3.Display.drawString(String(ontime), 10, 30); // Clear the previous value
                AtomS3.Display.setTextColor(WHITE);
                ontime = ontime + relative_change; // Update the value
                AtomS3.Display.drawString(String(ontime), 10, 30); // Display the updated change

                last_value = encoder_value; // Update the last value
            }
            delay(20);
            break;
        } // end of case 1

        case 2: // read encoder for forsink in ms
        {
            signed short int encoder_value = sensor.getEncoderValue();
            //forsink = encoder_value * 100;

            if (newpress) {
                AtomS3.Display.drawString("Delay", 5, 0);
                AtomS3.Display.drawString(String(forsink), 10, 60);
                last_value = encoder_value; // Update the last value
                newpress = false;
            }

            if (last_value != encoder_value) {
                int relative_change = encoder_value - last_value; // Calculate the relative change
                AtomS3.Display.setTextColor(BLACK);
                AtomS3.Display.drawString(String(forsink), 10, 60);
                AtomS3.Display.setTextColor(WHITE);
                forsink = forsink + relative_change; // Update the value
                AtomS3.Display.drawString(String(forsink), 10, 60);
                last_value = encoder_value;
            }
            break;    
        } // end of case 2

        case 3: // check if we want to save ontime and forsink by pres Atom button
        {
            if (newpress) {
                AtomS3.Display.drawString("Save ?", 5, 0);
                AtomS3.Display.drawString(String(ontime), 10, 30);
                AtomS3.Display.drawString(String(forsink), 10, 60);
                newpress = false;
            }
            
            AtomS3.update();
            if (AtomS3.BtnA.wasPressed()) { // Save to EEPROM
                EEPROM.put(0, ontime);
                EEPROM.put(sizeof(ontime), forsink);
                EEPROM.commit();

                // Flash the display in black and green
                for (int i = 0; i < 3; i++) {
                    AtomS3.Display.fillScreen(TFT_BLACK);
                    delay(200);
                    AtomS3.Display.fillScreen(TFT_GREEN);
                    delay(200);
                }

                // Display "Saved" with a black background
                AtomS3.Display.fillScreen(TFT_BLACK);
                AtomS3.Display.setTextColor(TFT_WHITE, TFT_BLACK); // White text on black background
                AtomS3.Display.drawString(String(ontime), 10, 30);
                AtomS3.Display.drawString(String(forsink), 10, 60);
                AtomS3.Display.drawString("Saved", 30, 100);

            }
            break;    
        } // end of case 3

    

    } // end of switch cases
}