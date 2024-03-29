/*
  ArtGateOne LED v1.4.5
*/

#include <Adafruit_NeoPixel.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiAvrI2c.h"
#include <EEPROM.h>
// On a Trinket or Gemma we suggest changing this to 1:
#define LED_PIN 6
#define LED_COUNT 170      // How many NeoPixels are attached to the Arduino? - MAX 170 RGB
#define LED_BRIGHTNESS 50  // 1-255
#define analogPin A3       //Factory default
#define I2C_ADDRESS 0x3C   // OLED I2C Addres

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)

SSD1306AsciiAvrI2c oled;

bool invert = false;
int post = 0;
unsigned int datalen;
int data;
String strwww = String();

byte ArtPollReply[239];

// Get data from EEPROM
byte intN = EEPROM.read(531);  // NET
byte intS = EEPROM.read(532);  // Subnet
byte intU = EEPROM.read(533);  // Universe

unsigned int intUniverse = ((intS * 16) + intU);

byte mac[] = { EEPROM.read(525), EEPROM.read(526), EEPROM.read(527), EEPROM.read(528), EEPROM.read(529), EEPROM.read(530) };
IPAddress ip(EEPROM.read(513), EEPROM.read(514), EEPROM.read(515), EEPROM.read(516));
unsigned int localPort = 6454;  // local port to listen on
unsigned char readBuffer[18];   // readBuffer to hold incoming packet,

EthernetUDP Udp;
EthernetServer server(80);

void setup() {
  strip.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();   // Turn OFF all pixels ASAP
  strip.setBrightness(LED_BRIGHTNESS);

  oled.begin(&Adafruit128x32, I2C_ADDRESS);
  oled.setFont(Arial14);
  oled.set1X();
  Ethernet.init(10);

  pinMode(analogPin, INPUT_PULLUP);
  int pinValue = digitalRead(analogPin);
  if (EEPROM.read(550) == 0 || EEPROM.read(550) == 255 || pinValue == LOW)  // check first run or PIN3 to GND (FACTORY RESET)
  {                                                                         // write default config
    EEPROM.update(512, 1);                                                  // DHCP 1 off, 0 on
    EEPROM.update(513, 2);                                                  // IP
    EEPROM.update(514, 0);
    EEPROM.update(515, 0);
    EEPROM.update(516, 10);
    EEPROM.update(517, 255);  // SubNetMask
    EEPROM.update(518, 0);
    EEPROM.update(519, 0);
    EEPROM.update(520, 0);
    EEPROM.update(521, 0);  // gateway
    EEPROM.update(522, 0);
    EEPROM.update(523, 0);
    EEPROM.update(524, 0);
    EEPROM.update(525, 222);  // mac adres
    EEPROM.update(526, 173);  // mac
    EEPROM.update(527, 190);  // mac
    EEPROM.update(528, 239);  // mac
    EEPROM.update(529, 254);  // mac
    EEPROM.update(530, 240);  // mac
    EEPROM.update(531, 0);    // Art-Net Net
    EEPROM.update(532, 0);    // Art-Net Sub
    EEPROM.update(533, 0);    // Art-Net Uni
    EEPROM.update(534, 0);    // boot scene
    EEPROM.update(535, 1);    // LED mode/not used
    EEPROM.update(550, 1);    // komórka kontrolna
    oled.clear();
    oled.print("RESET");
    delay(1500);
  } else if (EEPROM.read(512) == 1) {  // uruchomienie ethernet w zależności od ustawień dhcp 1=statyczny , 0=dhcp
    IPAddress ip(EEPROM.read(513), EEPROM.read(514), EEPROM.read(515), EEPROM.read(516));
    Ethernet.begin(mac, ip);
    IPAddress newSubnet(EEPROM.read(517), EEPROM.read(518), EEPROM.read(519), EEPROM.read(520));
    Ethernet.setSubnetMask(newSubnet);
    IPAddress newGateway(EEPROM.read(521), EEPROM.read(522), EEPROM.read(523), EEPROM.read(524));
    Ethernet.setGatewayIP(newGateway);
    oled.clear();
    oled.print("ArtGateOne LED 1.4");
    delay(1500);
  } else {  // dhcp
    oled.clear();
    oled.println("ArtGateOne LED 1.4");
    oled.print("DHCP...");
    delay(1500);
    Ethernet.begin(mac);
    Ethernet.maintain();
    delay(1500);
  }

  Udp.begin(localPort);

  displaydata();

  if (EEPROM.read(534) == 1) {
    for (unsigned int i = 0; i <= strip.numPixels(); i++) {
      strip.setPixelColor(i, EEPROM.read(i * 3), EEPROM.read((i * 3) + 1), EEPROM.read((i * 3) + 2));
    }
    strip.show();
  }

  makeArtPollReply();

}  // end setup()

void loop() {

  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    // Serial.println("new client");
    //  an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        strwww += c;
        // Serial.write(c);
        //  if you've gotten to the end of the line (received a newline
        //  character) and the line is blank, the http request has ended,
        //  so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          if (strwww[0] == 71 && strwww[5] == 32) {
            client.println(F("HTTP/1.1 200 OK"));
            client.println(F("Content-Type: text/html, charset=utf-8"));
            client.println(F("Connection: close"));  // the connection will be closed after completion of the response
            client.println(F("User-Agent: ArtGateOne"));
            client.println();
            client.println(F("<!DOCTYPE HTML>"));
            client.println(F("<html>"));
            client.println(F("<head>"));
            client.println(F("<link rel=\"icon\" type=\"image/png\" sizes=\"16x16\" href=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAQAAAC1+jfqAAAAE0lEQVR42mP8X8+AFzCOKhhJCgAePhfxCE5/6wAAAABJRU5ErkJggg==\">"));
            client.println(F("<title>ArtGateOne</title>"));
            client.println(F("<meta charset=\"UTF-8\">"));
            client.println(F("<meta name=\"description\" content=\"ArtGateOne\">"));
            client.println(F("<meta name=\"keywords\" content=\"HTML,CSS,XML,JavaScript\">"));
            client.println(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"));
            client.println(F("<meta name=\"author\" content=\"KK\">"));
            client.println(F("<style>"));
            client.println(F("body {text-align: center;}"));
            client.println(F("div {width:340px; display: inline-block; text-align: center;}"));
            client.println(F("label {width:130px; display: inline-block;}"));
            client.println(F("input {width:130px; display: inline-block;}"));
            client.println(F("</style>"));
            client.println(F("</head>"));
            client.println(F("<body>"));
            client.println(F("<div>"));
            client.println(F("<h2>ArtGateOne LED</h2>"));
            client.println(F("<form action=\"/ok\">"));
            client.println(F("<fieldset>"));
            client.println(F("<legend>Ethernet:</legend>"));
            client.println(F("<label for=\"quantity\">Mode:</label>"));
            client.println(F("<select id=\"mode\" name=\"dhcp\">"));
            if (EEPROM.read(512) == 0) {
              client.println(F("<option value=\"0\" selected>DHCP</option>"));
              client.println(F("<option value=\"1\">Static</option>"));
            } else {
              client.println(F("<option value=\"0\">DHCP</option>"));
              client.println(F("<option value=\"1\" selected>Static</option>"));
            }
            client.println(F("</select><br>"));
            client.println(F("<label for=\"quantity\">IP Addres:</label>"));
            client.print(F("<input type=\"tel\" id=\"ethernet\" name=\"ipaddres\" value=\""));
            client.print(Ethernet.localIP());
            client.println(F("\" pattern=\"((^|\\.)((25[0-5])|(2[0-4]\\d)|(1\\d\\d)|([1-9]?\\d))){4}$\" required><br>"));
            client.println(F("<label for=\"quantity\">Subnet mask:</label>"));
            client.print(F("<input type=\"tel\" id=\"ethernet\" name=\"subnet\" value=\""));
            client.print(Ethernet.subnetMask());
            client.println(F("\" pattern=\"((^|\\.)((25[0-5])|(2[0-4]\\d)|(1\\d\\d)|([1-9]?\\d))){4}$\" required><br>"));
            client.println(F("<label for=\"quantity\">Gateway:</label>"));
            client.print(F("<input type=\"tel\" id=\"ethernet\" name=\"gateway\" value=\""));
            client.print(Ethernet.gatewayIP());
            client.println(F("\" pattern=\"((^|\\.)((25[0-5])|(2[0-4]\\d)|(1\\d\\d)|([1-9]?\\d))){4}$\" required><br>"));
            client.println(F("<label for=\"quantity\">MAC Addres:</label>"));
            client.print(F("<input type=\"text\" id=\"ethernet\" name=\"mac\" value=\""));
            if (EEPROM.read(525) <= 15) {
              client.print(F("0"));
            }
            client.print(EEPROM.read(525), HEX);
            client.print(F(":"));
            if (EEPROM.read(526) <= 15) {
              client.print(F("0"));
            }
            client.print(EEPROM.read(526), HEX);
            client.print(F(":"));
            if (EEPROM.read(527) <= 15) {
              client.print(F("0"));
            }
            client.print(EEPROM.read(527), HEX);
            client.print(F(":"));
            if (EEPROM.read(528) <= 15) {
              client.print(F("0"));
            }
            client.print(EEPROM.read(528), HEX);
            client.print(F(":"));
            if (EEPROM.read(529) <= 15) {
              client.print(F("0"));
            }
            client.print(EEPROM.read(529), HEX);
            client.print(F(":"));
            if (EEPROM.read(530) <= 15) {
              client.print(F("0"));
            }
            client.print(EEPROM.read(530), HEX);
            client.println(F("\" pattern=\"[A-F0-9]{2}:[A-F0-9]{2}:[A-F0-9]{2}:[A-F0-9]{2}:[A-F0-9]{2}:[A-F0-9]{2}$\" required><br>"));
            client.println(F("</fieldset><br>"));
            client.println(F("<fieldset>"));
            client.println(F("<legend>ArtNet:</legend>"));
            client.println(F("<label for=\"quantity\">Net:</label>"));
            client.print(F("<input type=\"number\" id=\"artnet\" name=\"net\" min=\"0\" max=\"127\" required value=\""));
            client.print(EEPROM.read(531));
            client.println(F("\"><br>"));
            client.println(F("<label for=\"quantity\">Subnet:</label>"));
            client.print(F("<input type=\"number\" id=\"artnet\" name=\"subnet\" min=\"0\" max=\"15\" required value=\""));
            client.print(EEPROM.read(532));
            client.println(F("\"><br>"));
            client.println(F("<label for=\"quantity\">Universe:</label>"));
            client.print(F("<input type=\"number\" id=\"artnet\" name=\"universe\" min=\"0\" max=\"15\" required value=\""));
            client.print(EEPROM.read(533));
            client.println(F("\"><br>"));
            client.println(F("</fieldset><br>"));
            client.println(F("<fieldset>"));
            client.println(F("<legend>Boot:</legend>"));
            client.println(F("<label for=\"quantity\">Startup scene:</label>"));
            client.println(F("<select id=\"scene\" name=\"scene\" value=\"Enable\">"));
            if (EEPROM.read(534) == 0) {
              client.println(F("<option value=\"0\" selected>Disabled</option>"));
              client.println(F("<option value=\"1\">Enable</option>"));
            } else {
              client.println(F("<option value=\"0\">Disable</option>"));
              client.println(F("<option value=\"1\" selected>Enabled</option>"));
            }
            client.println(F("<option value=\"2\">Record new scene</option>"));
            client.println(F("</select><br>"));
            client.println(F("</fieldset><br>"));
            client.println(F("<input type=\"reset\" value=\"Reset\">"));
            client.println(F("<input type=\"submit\" value=\"Submit\" formmethod=\"post\"><br><br><br>"));
            client.println(F("</form>"));
            client.println(F("</div>"));
            client.println(F("</body>"));
            client.println(F("</html>"));
            delay(10);
            strwww = String();
            client.stop();
            displaydata();
            break;
          }
          if (strwww[0] == 71 && strwww[5] == 102) {  // Sprawdza czy ramka favicon
            client.println("HTTP/1.1 200 OK");
            client.println();
            client.stop();
            strwww = String();
            break;
          }
          if (strwww[0] == 80) {  // sprawdza czy ramka POST
            datalen = 0;
            char *position = strstr(strwww.c_str(), "Content-Length");
            if (position != NULL) {
              int startIndex = position - strwww.c_str() + 15;  // Adjust the starting index based on the pattern
              char *endLine = strchr(position, '\n');           // Search for the end of the line
              if (endLine != NULL) {
                int endIndex = endLine - strwww.c_str();
                char lengthValue[10];  // Assuming the length value is within 10 digits
                strncpy(lengthValue, strwww.c_str() + startIndex, endIndex - startIndex);
                lengthValue[endIndex - startIndex] = '\0';
                datalen = atoi(lengthValue);
              }
            }
            post = 1;  // ustawia odbior danych
            strwww = String();
            client.println("HTTP/1.1 200 OK");
            // client.println();
            break;
          }
        }
        if (post == 1 && strwww.length() == datalen) {  // odbior danych
          datadecode();
          delay(1);
          // PRZETWARZA ODEBRANE DANE I WYŚWIETLA STRONE KONCOWA
          client.println(F("HTTP/1.1 200 OK"));
          client.println(F("Content-Type: text/html, charset=utf-8"));
          client.println(F("Connection: close"));  // the connection will be closed after completion of the response
          client.println(F("User-Agent: ArtGateOne"));
          client.println();
          client.println(F("<!DOCTYPE HTML>"));
          client.println(F("<html>"));
          client.println(F("<head>"));
          client.println(F("<link rel=\"icon\" type=\"image/png\" sizes=\"16x16\" href=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAQAAAC1+jfqAAAAE0lEQVR42mP8X8+AFzCOKhhJCgAePhfxCE5/6wAAAABJRU5ErkJggg==\">"));
          client.println(F("<title>ArtGateOne setup</title>"));
          client.println(F("<meta charset=\"UTF-8\">"));
          client.print(F("<meta http-equiv=\"refresh\" content=\"5; url=http://"));
          client.print(EEPROM.read(513));
          client.print(F("."));
          client.print(EEPROM.read(514));
          client.print(F("."));
          client.print(EEPROM.read(515));
          client.print(F("."));
          client.print(EEPROM.read(516));
          client.println(F("\">"));
          client.println(F("<meta name=\"description\" content=\"ArtGateOne\">"));
          client.println(F("<meta name=\"keywords\" content=\"HTML,CSS,XML,JavaScript\">"));
          client.println(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"));
          client.println(F("<meta name=\"author\" content=\"KK\">"));
          client.println(F("<style>"));
          client.println(F("body {text-align: center;}"));
          client.println(F("div {width:340px; display: inline-block; text-align: center;}"));
          client.println(F("label {width:120px; display: inline-block;}"));
          client.println(F("input {width:120px; display: inline-block;}"));
          client.println(F("</style>"));
          client.println(F("</head>"));
          client.println(F("<body>"));
          client.println(F("<div>"));
          client.println(F("<h2>ArtGateOne</h2>"));
          client.println(F("<h2>WAIT ...</h2>"));
          client.println(F("</div>"));
          client.println(F("</form>"));
          client.println(F("</body>"));
          client.println(F("</html>"));
          delay(1);
          client.stop();
          strwww = String();
          post = 0;
          if (EEPROM.read(512) == 0) {
            // Ethernet.begin(mac);
            Ethernet.maintain();
          } else {
            IPAddress newIp(EEPROM.read(513), EEPROM.read(514), EEPROM.read(515), EEPROM.read(516));
            Ethernet.setLocalIP(newIp);
            IPAddress newSubnet(EEPROM.read(517), EEPROM.read(518), EEPROM.read(519), EEPROM.read(520));
            Ethernet.setSubnetMask(newSubnet);
            IPAddress newGateway(EEPROM.read(521), EEPROM.read(522), EEPROM.read(523), EEPROM.read(524));
            Ethernet.setGatewayIP(newGateway);
          }
          IPAddress localIP = Ethernet.localIP();
          makeArtPollReply();
          delay(500);
          displaydata();
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
  }

  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if (packetSize == 14 || packetSize == 18) {
    invert = !invert;
    oled.invertDisplay(invert);
    // send a ArtPollReply to the IP address and port that sent us the packet we received
    // Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(ArtPollReply, 239);  //  ArtPollReplyLen = 239;
    Udp.endPacket();
  } else if (packetSize == 530) {

    // read the packet into packetArtPollReplyffer
    Udp.read(readBuffer, 18);
    if (readBuffer[15] == intN && readBuffer[14] == intUniverse) {  // check artnet universe

      for (unsigned int i = 0; i < LED_COUNT; i++) {
        Udp.read(readBuffer, 3);
        strip.setPixelColor(i, readBuffer[0], readBuffer[1], readBuffer[2]);
      }
      Udp.read(readBuffer, 2);
      // strip.setBrightness(readBuffer[1]);//Master Dimmer? readBuffer[0] - ch511, readBuffer[1] - ch512
      strip.show();
    }
  }
}  // end loop()

void makeArtPollReply() {
  ArtPollReply[0] = byte('A');  // A
  ArtPollReply[1] = byte('r');  // r
  ArtPollReply[2] = byte('t');  // t
  ArtPollReply[3] = byte('-');  // -
  ArtPollReply[4] = byte('N');  // N
  ArtPollReply[5] = byte('e');  // e
  ArtPollReply[6] = byte('t');  // t
  ArtPollReply[7] = 0x00;       // 0x00

  ArtPollReply[8] = 0x00;  // OpCode[0]
  ArtPollReply[9] = 0x21;  // OpCode[1]

  ArtPollReply[10] = Ethernet.localIP()[0];  // IPV4 [0]
  ArtPollReply[11] = Ethernet.localIP()[1];  // IPV4 [1]
  ArtPollReply[12] = Ethernet.localIP()[2];  // IPV4 [2]
  ArtPollReply[13] = Ethernet.localIP()[3];  // IPV4 [3]

  ArtPollReply[14] = 0x36;  // IP Port Low
  ArtPollReply[15] = 0x19;  // IP Port Hi

  ArtPollReply[16] = 0x01;  // High byte of Version
  ArtPollReply[17] = 0x04;  // Low byte of Version

  ArtPollReply[18] = intN;  // NetSwitch
  ArtPollReply[19] = intS;  // Net Sub Switch
  ArtPollReply[20] = 0xFF;  // 0x04; // OEMHi
  ArtPollReply[21] = 0xFF;  // 0xB9; // OEMLow
  ArtPollReply[22] = 0x00;  // Ubea Version
  ArtPollReply[23] = 0xF0;  // Status1
  ArtPollReply[24] = 0x00;  // ESTA LO 0x41; //
  ArtPollReply[25] = 0x00;  // ESTA HI  0x4D; //

  ArtPollReply[26] = byte('A');  // A  //Short Name
  ArtPollReply[27] = byte('r');  // r
  ArtPollReply[28] = byte('t');  // t
  ArtPollReply[29] = byte('G');  // G
  ArtPollReply[30] = byte('a');  // a
  ArtPollReply[31] = byte('t');  // t
  ArtPollReply[32] = byte('e');  // e
  ArtPollReply[33] = byte('O');  // a
  ArtPollReply[34] = byte('n');  // t
  ArtPollReply[35] = byte('e');  // e

  for (int i = 36; i <= 43; i++) {  // Short Name
    ArtPollReply[i] = 0x00;
  }

  ArtPollReply[44] = byte('A');  // A  //Long Name
  ArtPollReply[45] = byte('r');  // r
  ArtPollReply[46] = byte('t');  // t
  ArtPollReply[47] = byte('G');  // G
  ArtPollReply[48] = byte('a');  // a
  ArtPollReply[49] = byte('t');  // t
  ArtPollReply[50] = byte('e');  // e
  ArtPollReply[51] = byte('O');  // O
  ArtPollReply[52] = byte('n');  // n
  ArtPollReply[53] = byte('e');  // e
  ArtPollReply[54] = byte(' ');  //
  ArtPollReply[55] = byte('L');  // L
  ArtPollReply[56] = byte('E');  // E
  ArtPollReply[57] = byte('D');  // D
  ArtPollReply[58] = byte(' ');  //
  ArtPollReply[59] = byte('1');  // 1
  ArtPollReply[60] = byte('.');  // .
  ArtPollReply[61] = byte('4');  // 4

  for (int i = 62; i <= 107; i++) {  // Long Name
    ArtPollReply[i] = 0x00;
  }

  for (int i = 108; i <= 171; i++) {  // NodeReport
    ArtPollReply[i] = 0x00;
  }

  ArtPollReply[172] = 0x00;  // NumPorts Hi
  ArtPollReply[173] = 0x01;  // NumPorts Lo
  ArtPollReply[174] = 0x80;  // Port 0 Type
  ArtPollReply[175] = 0x00;  // Port 1 Type
  ArtPollReply[176] = 0x00;  // Port 2 Type
  ArtPollReply[177] = 0x00;  // Port 3 Type
  ArtPollReply[178] = 0x08;  // GoodInput 0
  ArtPollReply[179] = 0x00;  // GoodInput 1
  ArtPollReply[180] = 0x00;  // GoodInput 2
  ArtPollReply[181] = 0x00;  // GoodInput 3
  ArtPollReply[182] = 0x80;  // GoodOutput 0
  ArtPollReply[183] = 0x00;  // GoodOutput 1
  ArtPollReply[184] = 0x00;  // GoodOutput 2
  ArtPollReply[185] = 0x00;  // GoodOutput 3
  ArtPollReply[186] = 0x00;  // SwIn 0
  ArtPollReply[187] = 0x00;  // SwIn 1
  ArtPollReply[188] = 0x00;  // SwIn 2
  ArtPollReply[189] = 0x00;  // SwIn 3
  ArtPollReply[190] = intU;  // SwOut 0//ODBIERA UNIVERSE NR
  ArtPollReply[191] = 0x00;  // SwOut 1
  ArtPollReply[192] = 0x00;  // SwOut 2
  ArtPollReply[193] = 0x00;  // SwOut 3
  ArtPollReply[194] = 0x01;  // SwVideo
  ArtPollReply[195] = 0x00;  // SwMacro
  ArtPollReply[196] = 0x00;  // SwRemote
  ArtPollReply[197] = 0x00;  // Spare
  ArtPollReply[198] = 0x00;  // Spare
  ArtPollReply[199] = 0x00;  // Spare
  ArtPollReply[200] = 0x00;  // Style
  // MAC ADDRESS
  ArtPollReply[201] = mac[0];  // MAC HI
  ArtPollReply[202] = mac[1];  // MAC
  ArtPollReply[203] = mac[2];  // MAC
  ArtPollReply[204] = mac[3];  // MAC
  ArtPollReply[205] = mac[4];  // MAC
  ArtPollReply[206] = mac[5];  // MAC LO

  ArtPollReply[207] = 0x00;  // BIND IP 0
  ArtPollReply[208] = 0x00;  // BIND IP 1
  ArtPollReply[209] = 0x00;  // BIND IP 2
  ArtPollReply[210] = 0x00;  // BIND IP 3
  ArtPollReply[211] = 0x00;  // BInd Index

  ArtPollReply[212] = 0x05;  // Status2
  if (EEPROM.read(512) == 0) {
    ArtPollReply[212] = 0x07;  // DHCP USED
  }

  /*for (int i = 213; i <= 239; i++) {  //Filler
    ArtPollReply[i] = 0x00;
    }*/
  return;
}

void datadecode() {
  int j = 0;
  for (unsigned int i = 0; i <= datalen; i++) {
    if (strwww[i] == 61) {  // jeśli znajdzie znak równości
      j++;
      i++;
      if (j == 1) {  // DHCP
        EEPROM.update(512, (strwww[i] - 48));
      }
      if (j == 2) {  // IP ADDRES
        data = dataadd(i);
        EEPROM.update(513, data);
        i = i + 2;
        if (data >= 10) {
          i++;
        }
        if (data >= 100) {
          i++;
        }
        data = dataadd(i);
        EEPROM.update(514, data);
        i = i + 2;
        if (data >= 10) {
          i++;
        }
        if (data >= 100) {
          i++;
        }
        data = dataadd(i);
        EEPROM.update(515, data);
        i = i + 2;
        if (data >= 10) {
          i++;
        }
        if (data >= 100) {
          i++;
        }
        data = dataadd(i);
        EEPROM.update(516, data);
      }
      if (j == 3) {  // SUBNET
        data = dataadd(i);
        EEPROM.update(517, data);
        i = i + 2;
        if (data >= 10) {
          i++;
        }
        if (data >= 100) {
          i++;
        }
        data = dataadd(i);
        EEPROM.update(518, data);
        i = i + 2;
        if (data >= 10) {
          i++;
        }
        if (data >= 100) {
          i++;
        }
        data = dataadd(i);
        EEPROM.update(519, data);
        i = i + 2;
        if (data >= 10) {
          i++;
        }
        if (data >= 100) {
          i++;
        }
        data = dataadd(i);
        EEPROM.update(520, data);
      }
      if (j == 4) {  // GATEWAY
        data = dataadd(i);
        EEPROM.update(521, data);
        i = i + 2;
        if (data >= 10) {
          i++;
        }
        if (data >= 100) {
          i++;
        }
        data = dataadd(i);
        EEPROM.update(522, data);
        i = i + 2;
        if (data >= 10) {
          i++;
        }
        if (data >= 100) {
          i++;
        }
        data = dataadd(i);
        EEPROM.update(523, data);
        i = i + 2;
        if (data >= 10) {
          i++;
        }
        if (data >= 100) {
          i++;
        }
        data = dataadd(i);
        EEPROM.update(524, data);
      }
      if (j == 5) {  // MAC
        EEPROM.update(525, datamac(i));
        i = i + 5;
        EEPROM.update(526, datamac(i));
        i = i + 5;
        EEPROM.update(527, datamac(i));
        i = i + 5;
        EEPROM.update(528, datamac(i));
        i = i + 5;
        EEPROM.update(529, datamac(i));
        i = i + 5;
        EEPROM.update(530, datamac(i));
      }
      if (j == 6) {  // NET
        data = dataadd(i);
        EEPROM.update(531, data);
        intN = data;  // Phisical/NET
      }
      if (j == 7) {  // SUBNET
        data = dataadd(i);
        EEPROM.update(532, data);
        intS = data;  // Subnet
      }
      if (j == 8) {  // UNIVERSE
        data = dataadd(i);
        EEPROM.update(533, data);
        intU = data;  // Universe
        intUniverse = ((intS * 16) + intU);
      }
      if (j == 9) {  // SCENE
        int data = (strwww[i] - 48);
        if (data <= 1) {
          EEPROM.update(534, data);
        } else {
          EEPROM.update(534, 1);
          // save piexel color data to eeprom
          /*for ( i = 0; i <= 511; i++) {
            //EEPROM.update(i, ArduinoDmx0.TxArtPollReplyfer[i]);
            }*/
          for (i = 0; i <= (strip.numPixels() - 1); i++) {
            EEPROM.update((i * 3), (strip.getPixelColor(i) >> 16));
            EEPROM.update(((i * 3) + 1), (strip.getPixelColor(i) >> 8));
            EEPROM.update(((i * 3) + 2), strip.getPixelColor(i));
          }
        }
      }
    }
  }
}

int dataadd(int i) {
  data = 0;
  while (strwww[i] != 38 && strwww[i] != 46) {
    data = ((data * 10) + (strwww[i] - 48));
    i++;
  }
  return data;
}  // end dataadd()

int datamac(int i) {
  data = strwww[i];
  if (data <= 57) {
    data = data - 48;
  } else if (data >= 65) {
    data = data - 55;
  }

  data = data * 16;

  if (strwww[i + 1] <= 57) {
    data = data + (strwww[i + 1] - 48);
  } else if (strwww[i + 1] >= 65) {
    data = data + (strwww[i + 1] - 55);
  }
  return data;
}  // end datamac()

void displaydata() {
  oled.clear();
  oled.print(" IP : ");
  oled.print(Ethernet.localIP());
  if (EEPROM.read(534) == 1) {
    oled.println("  BS");
  } else {
    oled.println();
  }
  /*
  oled.print("Net ");
  oled.print(intN, HEX);
  oled.print("  Sub ");
  oled.print(intS, HEX);
  oled.print("  Uni ");
  oled.print(intU, HEX);
  */
  oled.print(" Universe ");
  oled.print(intUniverse);
  return;
}  // end displaydata()
