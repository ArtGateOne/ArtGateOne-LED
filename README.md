# ArtGateOne-LED
ArtGateOne-LED
This is simple art-net node to controll ws28xx led strip 170 x rgb.

Required: 

Arduino UNO

Ethernet Shield with W5100/W5500

OLED display 128x32 i2c

WS2812 led strip

-------------------------

Universal Ethernet to WS LED Interface

ONE Universe

Can be used with any Art-Net compliant console or DMX software

Universe selectable by web browser

Stand-alone buffer mode

-----------

WIRING

OLED DISPLAY I2C 

VCC --> 5v or 3.3v

GND --> GND

SCL --> SCL

SDA --> SDA

-----------

LED WS2812

6 pin  ---- Led Di

GND --> GND

---

default
ip 2.0.0.10
mask 255.0.0.0

------------

U can configure it - use web browser - enter node ip

A3 pin to Groun = Factory Reset

-----------------------


Library required

SSD1306Ascii

Andafruit_NeoPixel

------------------------

"Art-Net™ Designed by and Copyright Artistic Licence Engineering Ltd"
