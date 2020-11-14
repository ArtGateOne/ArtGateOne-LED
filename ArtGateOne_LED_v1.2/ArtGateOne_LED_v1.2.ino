/*
  ArtGateOne LED v1.2
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
#define LED_PIN    6

// How many NeoPixels are attached to the Arduino?
#define LED_COUNT 170

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


// 0X3C+SA0 - 0x3C or 0x3D
#define I2C_ADDRESS 0x3C

SSD1306AsciiAvrI2c oled;

int post = 0;
unsigned int datalen;
int data;
String strwww = String();

byte ArtPoolReply[239];

//Get data from EEPROM
byte intN = EEPROM.read(531); //NET
byte intS = EEPROM.read(532); //Subnet
byte intU = EEPROM.read(533); //Universe

unsigned int intUniverse = ((intS * 16) + intU);

byte mac[] = {EEPROM.read(525), EEPROM.read(526), EEPROM.read(527), EEPROM.read(528), EEPROM.read(529), EEPROM.read(530)};
IPAddress ip(EEPROM.read(513), EEPROM.read(514), EEPROM.read(515), EEPROM.read(516));
unsigned int localPort = 6454;      // local port to listen on
unsigned char packetArtPoolReplyfer[18];  // ArtPoolReplyfer to hold incoming packet,

EthernetUDP Udp;
EthernetServer server(80);

void setup()
{
  strip.begin();            // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();             // Turn OFF all pixels ASAP
  strip.setBrightness(10);  //strip.setBrightness(255); // Set BRIGHTNESS to 10 (max = 255)

  oled.begin(&Adafruit128x32, I2C_ADDRESS);
  oled.setFont(Arial14);
  oled.set1X();
  Ethernet.init(10);

  if (EEPROM.read(550) == 0 || EEPROM.read(550) == 255) {// komórka kontrolna - jesli inny reset & save
    EEPROM.update(512, 1);    //zapis DHCP off
    EEPROM.update(513, 10);   //IP
    EEPROM.update(514, 0);
    EEPROM.update(515, 0);
    EEPROM.update(516, 10);
    EEPROM.update(517, 255);  //SubNetMask
    EEPROM.update(518, 0);
    EEPROM.update(519, 0);
    EEPROM.update(520, 0);
    EEPROM.update(521, 10);   //gateway
    EEPROM.update(522, 0);
    EEPROM.update(523, 0);
    EEPROM.update(524, 1);
    EEPROM.update(525, 222);  //mac adres
    EEPROM.update(526, 173);  //mac
    EEPROM.update(527, 190);  //mac
    EEPROM.update(528, 239);  //mac
    EEPROM.update(529, 254);  //mac
    EEPROM.update(530, 240);  //mac
    EEPROM.update(531, 0);    //Art-Net Net
    EEPROM.update(532, 0);    //Art-Net Sub
    EEPROM.update(533, 0);    //Art-Net Uni
    EEPROM.update(534, 0);    //boot scene
    EEPROM.update(535, 1);    //LED mode/not used
    EEPROM.update(550, 1);    // komórka kontrolna
    oled.clear();
    oled.print("RESET");
    delay(1500);
  }
  else if (EEPROM.read(512) == 1) { //uruchomienie ethernet w zależności od ustawień dhcp 1=statyczny , 0=dhcp
    IPAddress ip(EEPROM.read(513), EEPROM.read(514), EEPROM.read(515), EEPROM.read(516));
    Ethernet.begin(mac, ip);
    IPAddress newSubnet(EEPROM.read(517), EEPROM.read(518), EEPROM.read(519), EEPROM.read(520));
    Ethernet.setSubnetMask(newSubnet);
    IPAddress newGateway(EEPROM.read(521), EEPROM.read(522), EEPROM.read(523), EEPROM.read(524));
    Ethernet.setGatewayIP(newGateway);
    oled.clear();
    oled.print("ArtGateOne LED 1.2");
    delay(1500);
  }
  else {//dhcp
    oled.clear();
    oled.println("ArtGateOne LED 1.2");
    oled.print  ("DHCP...");
    delay(1500);
    Ethernet.begin(mac);
    Ethernet.maintain();
    delay(1500);
  }

  Udp.begin(localPort);
  displaydata();

  if (EEPROM.read(534) == 1) {
    for (unsigned int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, EEPROM.read(i * 3), EEPROM.read((i * 3) + 1), EEPROM.read((i * 3) + 2));
    }
    strip.show();
  }

  makeArtPoolReply();

}//end setup()

void loop()
{

  // listen for incoming clients
  EthernetClient client = server.available();
    if (client) {
    //Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        strwww += c;
        //Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
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
          if (strwww[0] == 71 && strwww[5] == 102) {//Sprawdza czy ramka favicon
            client.println("HTTP/1.1 200 OK");
            client.println();
            client.stop();
            strwww = String();
            break;
          }
          if (strwww[0] == 80) {//sprawdza czy ramka POST
            datalen = 0;
            for (int i = 70; i <= 200; i++) { //wyszukanie konca lini
              if (strwww[(i)] == 13) { //jesli znajdzie oblicz ilosc danych
                datalen = ((int)strwww[i - 3] - 48) * 100;
                datalen += ((int)strwww[i - 2] - 48) * 10;
                datalen += ((int)strwww[i - 1] - 48);
                break;
              }
            }
            post = 1;//ustawia odbior danych
            strwww = String();
            client.println("HTTP/1.1 200 OK");
            //client.println();
            break;
          }
        }
        if (post == 1 && strwww.length() == datalen) { //odbior danych
          datadecode();
          delay(1);
          //PRZETWARZA ODEBRANE DANE I WYŚWIETLA STRONE KONCOWA
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
          //client.println(F("<h2>ArtGateOne</h2>"));
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
            //Ethernet.begin(mac);
            Ethernet.maintain();
          } else {
            IPAddress newIp(EEPROM.read(513), EEPROM.read(514), EEPROM.read(515), EEPROM.read(516));
            Ethernet.setLocalIP(newIp);
            IPAddress newSubnet(EEPROM.read(517), EEPROM.read(518), EEPROM.read(519), EEPROM.read(520));
            Ethernet.setSubnetMask(newSubnet);
            IPAddress newGateway(EEPROM.read(521), EEPROM.read(522), EEPROM.read(523), EEPROM.read(524));
            Ethernet.setGatewayIP(newGateway);
          }
          makeArtPoolReply();
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
  if (packetSize == 14) {
    // send a ArtPoolReply to the IP address and port that sent us the packet we received
    //Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.beginPacket(0xFFFFFFFF, Udp.remotePort());
    Udp.write(ArtPoolReply, 239);    //  ArtPoolReplyLen = 239;
    Udp.endPacket();
  }
  if (packetSize == 530) {

    // read the packet into packetArtPoolReplyffer
    Udp.read(packetArtPoolReplyfer, 18);
    if (packetArtPoolReplyfer[15] == intN && packetArtPoolReplyfer[14] == intUniverse) { //check artnet universe

      int ch = 0;
      for (unsigned int i = 0; i <= strip.numPixels(); i = i + 3) {
        Udp.read(packetArtPoolReplyfer, 3);
        strip.setPixelColor(ch, packetArtPoolReplyfer[0], packetArtPoolReplyfer[1], packetArtPoolReplyfer[2]);
        ch++;
      }
      Udp.read(packetArtPoolReplyfer, 2);
      //strip.setBrightness(packetArtPoolReplyfer[1]);//Master Dimmer
      strip.show();
    }
  }
}//end loop()


void makeArtPoolReply(){
      ArtPoolReply[0] = byte('A'); // A
    ArtPoolReply[1] = byte('r'); // r
    ArtPoolReply[2] = byte('t'); // t
    ArtPoolReply[3] = byte('-'); // -
    ArtPoolReply[4] = byte('N'); // N
    ArtPoolReply[5] = byte('e'); // e
    ArtPoolReply[6] = byte('t'); // t
    ArtPoolReply[7] = 0x00;      // 0x00

    ArtPoolReply[8] = 0x00;      // OpCode[0]
    ArtPoolReply[9] = 0x21;      // OpCode[1]

    ArtPoolReply[10] = Ethernet.localIP()[0]; // IPV4 [0]
    ArtPoolReply[11] = Ethernet.localIP()[1]; // IPV4 [1]
    ArtPoolReply[12] = Ethernet.localIP()[2]; // IPV4 [2]
    ArtPoolReply[13] = Ethernet.localIP()[3]; // IPV4 [3]

    ArtPoolReply[14] = 0x36; // IP Port Low
    ArtPoolReply[15] = 0x19; // IP Port Hi

    ArtPoolReply[16] = 0x01; // High byte of Version
    ArtPoolReply[17] = 0x02; // Low byte of Version

    ArtPoolReply[18] = intN; // NetSwitch
    ArtPoolReply[19] = intS; // Net Sub Switch
    ArtPoolReply[20] = 0xFF; // 0x04; // OEMHi
    ArtPoolReply[21] = 0xFF; // 0xB9; // OEMLow
    ArtPoolReply[22] = 0x00; // Ubea Version
    ArtPoolReply[23] = 0xF0; // Status1
    ArtPoolReply[24] = 0x00; // ESTA LO 0x41; //
    ArtPoolReply[25] = 0x00; // ESTA HI  0x4D; //

    ArtPoolReply[26] = byte('A');  // A  //Short Name
    ArtPoolReply[27] = byte('r');  // r
    ArtPoolReply[28] = byte('t');  // t
    ArtPoolReply[29] = byte('G');  // G
    ArtPoolReply[30] = byte('a');  // a
    ArtPoolReply[31] = byte('t');  // t
    ArtPoolReply[32] = byte('e');  // e
    ArtPoolReply[33] = byte('O');  // a
    ArtPoolReply[34] = byte('n');  // t
    ArtPoolReply[35] = byte('e');  // e

    for (int i = 36; i <= 43; i++) {// Short Name
      ArtPoolReply[i] = 0x00;
    }

    ArtPoolReply[44] = byte('A');  // A  //Long Name
    ArtPoolReply[45] = byte('r');  // r
    ArtPoolReply[46] = byte('t');  // t
    ArtPoolReply[47] = byte('G');  // G
    ArtPoolReply[48] = byte('a');  // a
    ArtPoolReply[49] = byte('t');  // t
    ArtPoolReply[50] = byte('e');  // e
    ArtPoolReply[51] = byte('O');  // O
    ArtPoolReply[52] = byte('n');  // n
    ArtPoolReply[53] = byte('e');  // e
    ArtPoolReply[54] = byte(' ');  //
    ArtPoolReply[55] = byte('L');  // L
    ArtPoolReply[56] = byte('E');  // E
    ArtPoolReply[57] = byte('D');  // D
    ArtPoolReply[58] = byte(' ');  //
    ArtPoolReply[59] = byte('1');  // 1
    ArtPoolReply[60] = byte('.');  // .
    ArtPoolReply[61] = byte('2');  // 2

    for (int i = 62; i <= 107; i++) { //Long Name
      ArtPoolReply[i] = 0x00;
    }

    for (int i = 108; i <= 171; i++) {  //NodeReport
      ArtPoolReply[i] = 0x00;
    }

    ArtPoolReply[172] = 0x00; // NumPorts Hi
    ArtPoolReply[173] = 0x01; // NumPorts Lo
    ArtPoolReply[174] = 0x80; // Port 0 Type
    ArtPoolReply[175] = 0x00; // Port 1 Type
    ArtPoolReply[176] = 0x00; // Port 2 Type
    ArtPoolReply[177] = 0x00; // Port 3 Type
    ArtPoolReply[178] = 0x00; // GoodInput 0
    ArtPoolReply[179] = 0x00; // GoodInput 1
    ArtPoolReply[180] = 0x00; // GoodInput 2
    ArtPoolReply[181] = 0x00; // GoodInput 3
    ArtPoolReply[182] = 0x80; // GoodOutput 0
    ArtPoolReply[183] = 0x00; // GoodOutput 1
    ArtPoolReply[184] = 0x00; // GoodOutput 2
    ArtPoolReply[185] = 0x00; // GoodOutput 3
    ArtPoolReply[186] = 0x00; // SwIn 0
    ArtPoolReply[187] = 0x00; // SwIn 1
    ArtPoolReply[188] = 0x00; // SwIn 2
    ArtPoolReply[189] = 0x00; // SwIn 3
    ArtPoolReply[190] = intU; // SwOut 0//ODBIERA UNIVERSE NR
    ArtPoolReply[191] = 0x00; // SwOut 1
    ArtPoolReply[192] = 0x00; // SwOut 2
    ArtPoolReply[193] = 0x00; // SwOut 3
    ArtPoolReply[194] = 0x01; // SwVideo
    ArtPoolReply[195] = 0x00; // SwMacro
    ArtPoolReply[196] = 0x00; // SwRemote
    ArtPoolReply[197] = 0x00; // Spare
    ArtPoolReply[198] = 0x00; // Spare
    ArtPoolReply[199] = 0x00; // Spare
    ArtPoolReply[200] = 0x00; // Style
    // MAC ADDRESS
    ArtPoolReply[201] = mac[0]; // MAC HI
    ArtPoolReply[202] = mac[1]; // MAC
    ArtPoolReply[203] = mac[2]; // MAC
    ArtPoolReply[204] = mac[3]; // MAC
    ArtPoolReply[205] = mac[4]; // MAC
    ArtPoolReply[206] = mac[5]; // MAC LO

    ArtPoolReply[207] = 0x00; // BIND IP 0
    ArtPoolReply[208] = 0x00; // BIND IP 1
    ArtPoolReply[209] = 0x00; // BIND IP 2
    ArtPoolReply[210] = 0x00; // BIND IP 3
    ArtPoolReply[211] = 0x00; // BInd Index

    
    ArtPoolReply[212] = 0x05; // Status2
    if (EEPROM.read(512) == 0){ArtPoolReply[212] = 0x07;} //DHCP USED
    

    /*for (int i = 213; i <= 239; i++) {  //Filler
      ArtPoolReply[i] = 0x00;
    }*/
  return;
  }




void datadecode() {
  int j = 0;
  for (unsigned int i = 0; i <= datalen; i++) {
    if (strwww[i] == 61) { //jeśli znajdzie znak równości
      j++;
      i++;
      if ( j == 1) { //DHCP
        EEPROM.update(512, (strwww[i] - 48));
      }
      if ( j == 2) { //IP ADDRES
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
      if ( j == 3) { //SUBNET
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
      if ( j == 4) { //GATEWAY
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
      if ( j == 5) { //MAC
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
      if ( j == 6) { //NET
        data = dataadd(i);
        EEPROM.update(531, data);
        intN = data; //Phisical/NET
      }
      if ( j == 7) { //SUBNET
        data = dataadd(i);
        EEPROM.update(532, data);
        intS = data; //Subnet
      }
      if ( j == 8) { //UNIVERSE
        data = dataadd(i);
        EEPROM.update(533, data);
        intU = data; //Universe
        intUniverse = ((intS * 16) + intU);
      }
      if ( j == 9) { //SCENE
        int data = (strwww[i] - 48);
        if (data <= 1) {
          EEPROM.update(534, data);
        } else {
          EEPROM.update(534, 1);
          // nagraj data do eprom
          /*for ( i = 0; i <= 511; i++) {
            //EEPROM.update(i, ArduinoDmx0.TxArtPoolReplyfer[i]);
            }*/
          for (i = 0; i <= (strip.numPixels() - 1); i++) {
            EEPROM.update((i * 3), (strip.getPixelColor(i) >> 16));
            EEPROM.update(((i * 3) + 1), (strip.getPixelColor(i) >>  8));
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
}//end dataadd()

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
}//end datamac()

void displaydata() {
  oled.clear();
  oled.print("IP : ");
  oled.print(Ethernet.localIP());
  if (EEPROM.read(534) == 1) {
    oled.println("  BS");
  } else {
    oled.println();
  }
  oled.print("Net ");
  oled.print(intN, HEX);
  oled.print("  Sub ");
  oled.print(intS, HEX);
  oled.print("  Uni ");
  oled.print(intU, HEX);
  return;
}//end displaydata()
