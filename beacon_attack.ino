/*
  ===========================================
       Copyright (c) 2018 Stefan Kremser
              github.com/spacehuhn
  ===========================================
*/

// ===== Settings ===== //
const uint8_t channels[] = {1, 6, 11}; // used Wi-Fi channels (available: 1-14)
const bool wpa2 = false; // WPA2 networks
const bool appendSpaces = true; // makes all SSIDs 32 characters long to improve performance

/*
  SSIDs:
  - don't forget the \n at the end of each SSID!
  - max. 32 characters per SSID
  - don't add duplicates! You have to change one character at least
*/
const int maxN = 5000;
char ssids[maxN];
// ===== Includes ===== //
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
extern "C" {
#include "user_interface.h"
  typedef void (*freedom_outside_cb_t)(uint8 status);
  int wifi_register_send_pkt_freedom_cb(freedom_outside_cb_t cb);
  void wifi_unregister_send_pkt_freedom_cb(void);
  int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}
// ==================== //

// run-time variables
String invisible_chars[] = {

};
const int num_invisible_chars = sizeof(invisible_chars) / sizeof(invisible_chars[0]);

char emptySSID[32];
uint8_t channelIndex = 0;
uint8_t macAddr[6];
uint8_t wifi_channel = 1;
uint32_t currentTime = 0;
uint32_t packetSize = 0;
uint32_t packetCounter = 0;
uint32_t attackTime = 0;
uint32_t packetRateTime = 0;
AsyncWebServer server(80);
IPAddress apIP(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// beacon frame definition
uint8_t beaconPacket[109] = {
  /*  0 - 3  */ 0x80, 0x00, 0x00, 0x00,             // Type/Subtype: managment beacon frame
  /*  4 - 9  */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: broadcast
  /* 10 - 15 */ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // Source
  /* 16 - 21 */ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // Source

  // Fixed parameters
  /* 22 - 23 */ 0x00, 0x00,                         // Fragment & sequence number (will be done by the SDK)
  /* 24 - 31 */ 0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, // Timestamp
  /* 32 - 33 */ 0xe8, 0x03,                         // Interval: 0x64, 0x00 => every 100ms - 0xe8, 0x03 => every 1s
  /* 34 - 35 */ 0x31, 0x00,                         // capabilities Tnformation

  // Tagged parameters

  // SSID parameters
  /* 36 - 37 */ 0x00, 0x20,                         // Tag: Set SSID length, Tag length: 32
  /* 38 - 69 */ 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,                           // SSID

  // Supported Rates
  /* 70 - 71 */ 0x01, 0x08,                         // Tag: Supported Rates, Tag length: 8
  /* 72 */ 0x82,                    // 1(B)
  /* 73 */ 0x84,                    // 2(B)
  /* 74 */ 0x8b,                    // 5.5(B)
  /* 75 */ 0x96,                    // 11(B)
  /* 76 */ 0x24,                    // 18
  /* 77 */ 0x30,                    // 24
  /* 78 */ 0x48,                    // 36
  /* 79 */ 0x6c,                    // 54

  // Current Channel
  /* 80 - 81 */ 0x03, 0x01,         // Channel set, length
  /* 82 */      0x01,               // Current Channel

  // RSN information
  /*  83 -  84 */ 0x30, 0x18,
  /*  85 -  86 */ 0x01, 0x00,
  /*  87 -  90 */ 0x00, 0x0f, 0xac, 0x02,
  /*  91 -  92 */ 0x02, 0x00,
  /*  93 - 100 */ 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04, /*Fix: changed 0x02(TKIP) to 0x04(CCMP) is default. WPA2 with TKIP not supported by many devices*/
  /* 101 - 102 */ 0x01, 0x00,
  /* 103 - 106 */ 0x00, 0x0f, 0xac, 0x02,
  /* 107 - 108 */ 0x00, 0x00
};
 const char html[] PROGMEM  = R"***(
       <!DOCTYPE html>
  <html lang="en">
  
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Message Box</title>
    <style>
      body {
        font-family: Arial, sans-serif;
        display: flex;
        align-items: center;
        justify-content: center;
        height: 100vh;
        margin: 0;
        background-color: rgb(237, 235, 242);
      }
  
      input[type=text] {
        width: 70%;
        padding: 12px 20px;
        margin: 8px 0;
        box-sizing: border-box;
        border: 3px solid #ccc;
        -webkit-transition: 0.5s;
        transition: 0.5s;
        outline: none;
      }
  
      input[type=text]:focus {
        border: 3px solid #555;
      }
  
      .message-box {
        border: 1px solid #ccc;
        border-radius: 8px;
        padding: 20px;
        box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
        background-color: #fff;
        max-width: 400px;
        width: 100%;
      }
  
      .message-box h2 {
        color: #333;
      }
  
      .message-box p {
        color: #555;
        margin-bottom: 10px;
      }
  
  
      .range-container {
        margin-top: 10px;
      }
  
      .text_adjust {
        font-size: 17px;
      }
  
      .input-container {
        width: 100%;
      }
  
      .btn-submit {
        background-color: #4CAF50;
        display: flex;
        color: #fff;
        padding: 12px 18px;
        border-radius: 4px;
        text-align: center;
        margin-left: 20px;
        margin-top: 20px;
        /* cursor: pointer; */
        /* width: 100%; */
  
      }
    </style>
  </head>
  
  <body>
  
    <div class="input-container">
      <label for="userInput" class="text_adjust">Enter something: </label>
      <input type="text" id="userInput" class="user-input" placeholder="Type here...">
      <button class="btn-submit" onclick="wifi_ssid1()">Submit</button>
    </div>
  
    <script>
      var xhttp = new XMLHttpRequest();
  
      function wifi_ssid1() {
        var userInput = document.getElementById("userInput").value;
        xhttp.open("GET", "/wifi_ssid?ssid=" + userInput);
        xhttp.send();
      }
    </script>
  </body>
  
  </html>

 )***";
// Shift out channels one by one
void nextChannel() {
  if (sizeof(channels) > 1) {
    uint8_t ch = channels[channelIndex];
    channelIndex++;
    if (channelIndex > sizeof(channels)) channelIndex = 0;

    if (ch != wifi_channel && ch >= 1 && ch <= 14) {
      wifi_channel = ch;
      wifi_set_channel(wifi_channel);
    }
  }
}
void handleHTMLRequest(AsyncWebServerRequest *request) {
  
  request->send(200, "text/html", html);

}
void wifi_ssids(AsyncWebServerRequest *request) {
  memset(ssids, 0, sizeof(ssids));
  if (request->hasParam("ssid")) {
    String origin_ssid = request->getParam("ssid")->value();
    //    String quantity = request->getParam("quan")->value();
    int quan = 32;
    String origin_ssid1 = origin_ssid;
    Serial.println(quan);
    Serial.println();
    for (int i = 1; i <= quan; i++) {
      if (i == 8 || i == 10 || i == 13 || i == 12) continue;
      origin_ssid = origin_ssid1;
      origin_ssid += char(i);
      origin_ssid += "\n";
      strncat(ssids, origin_ssid.c_str(), origin_ssid.length());
    }
    //    for (int i = 1; i <= quan; i++) {
    //      if(i % 8 == 0 || i % 10 == 0 || i % 13 == 0 || i % 12 == 0) continue;
    //      origin_ssid = origin_ssid1;
    //      origin_ssid += char(i)+char(i);
    //      origin_ssid += "\n";
    //      strncat(ssids, origin_ssid.c_str(), origin_ssid.length());
    //    }

    int ssidsLen = strlen_P(ssids);
    Serial.println(ssidsLen);

  }
  request->send(200, "text/plain", "Ok");
}

// Random MAC generator
void randomMac() {
  for (int i = 0; i < 6; i++) {
    macAddr[i] = random(256);
  }
}

void setup() {
  // create empty SSID
  for (int i = 0; i < 32; i++)
    emptySSID[i] = ' ';

  // for random generator
  randomSeed(os_random());

  // set packetSize
  packetSize = sizeof(beaconPacket);
  if (wpa2) {
    beaconPacket[34] = 0x31;
  } else {
    beaconPacket[34] = 0x21;
    packetSize -= 26;
  }

  // generate random mac address
  randomMac();

  // start serial
  Serial.begin(115200);
  Serial.println();

  // get time
  currentTime = millis();

  // start WiFi
  WiFi.mode(WIFI_OFF);
  wifi_set_opmode(STATION_MODE);
  WiFi.softAPConfig(apIP, apIP, subnet);
  WiFi.softAP("WIFI_modify", NULL, NULL, NULL, 1);
  Serial.println(WiFi.softAPIP());

  // Set to default WiFi channel
  wifi_set_channel(channels[0]);
  //  if (!LittleFS.begin(true)) {
  //    Serial.println("Error!");
  //    return;
  //  }
 

  server.on("/", HTTP_GET, handleHTMLRequest);
  server.on("/wifi_ssid", HTTP_GET, wifi_ssids);
  server.begin();
}

void loop() {
  //  int i = 0;
  //  int len = sizeof(ssids);
  //    while (i < len) {
  //      Serial.print(ssids[i]);
  //      i++;
  //  }
  currentTime = millis();

  // send out SSIDs
  if (currentTime - attackTime > 100) {
    attackTime = currentTime;

    // temp variables
    int i = 0;
    int j = 0;
    int ssidNum = 1;
    char tmp;
    int ssidsLen = strlen_P(ssids);
    //    Serial.println(ssidsLen);
    bool sent = false;

    // Go to next channel
    nextChannel();

    while (i < ssidsLen) {
      // Get the next SSID
      j = 0;
      do {
        tmp = ssids[i + j];
        j++;
      } while (tmp != '\n' && j <= 32 && i + j < ssidsLen);

      uint8_t ssidLen = j - 1;

      // set MAC address
      macAddr[5] = ssidNum;
      ssidNum++;

      // write MAC address into beacon frame
      memcpy(&beaconPacket[10], macAddr, 6);
      memcpy(&beaconPacket[16], macAddr, 6);

      // reset SSID
      memcpy(&beaconPacket[38], emptySSID, 32);

      // write new SSID into beacon frame
      memcpy_P(&beaconPacket[38], &ssids[i], ssidLen);
      //      Serial.println(&ssids[i]);

      // set channel for beacon frame
      beaconPacket[82] = wifi_channel;

      // send packet  
      if (appendSpaces) {
        for (int k = 0; k < 3; k++) {
          packetCounter += wifi_send_pkt_freedom(beaconPacket, packetSize, 0) == 0;
          delay(1);
        }
      }

      // remove spaces
      else {

        uint16_t tmpPacketSize = (packetSize - 32) + ssidLen; // calc size
        uint8_t* tmpPacket = new uint8_t[tmpPacketSize]; // create packet buffer
        memcpy(&tmpPacket[0], &beaconPacket[0], 38 + ssidLen); // copy first half of packet into buffer
        tmpPacket[37] = ssidLen; // update SSID length byte
        memcpy(&tmpPacket[38 + ssidLen], &beaconPacket[70], wpa2 ? 39 : 13); // copy second half of packet into buffer

        // send packet
        for (int k = 0; k < 3; k++) {
          packetCounter += wifi_send_pkt_freedom(tmpPacket, tmpPacketSize, 0) == 0;
          delay(1);
        }

        delete tmpPacket; // free memory of allocated buffer
      }

      i += j;
    }
  }

  // show packet-rate each second
  if (currentTime - packetRateTime > 1000) {
    packetRateTime = currentTime;
    Serial.print("Packets/s: ");
    Serial.println(packetCounter);
    packetCounter = 0;
  }
}
