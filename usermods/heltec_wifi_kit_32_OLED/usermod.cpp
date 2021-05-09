
/*
 * This file allows you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 * EEPROM bytes 2750+ are reserved for your custom use case. (if you extend #define EEPSIZE in const.h)
 * bytes 2400+ are currently ununsed, but might be used for future wled features
 */

/*
 * Pin 2 of the TTGO T-Display serves as the data line for the LED string.
 * Pin 35 is set up as the button pin in the platformio_overrides.ini file.
 * The button can be set up via the macros section in the web interface.
 * I use the button to cycle between presets.
 * The Pin 35 button is the one on the RIGHT side of the USB-C port on the board,
 * when the port is oriented downwards.  See readme.md file for photo.
 * The display is set up to turn off after 5 minutes, and turns on automatically 
 * when a change in the dipslayed info is detected (within a 5 second interval).
 */
 

//Use userVar0 and userVar1 (API calls &U0=,&U1=, uint16_t)

#include "wled.h"
#include <SPI.h>
#include <U8g2lib.h>

#ifndef OLED_PIXEL_HEIGHT
#define OLED_PIXEL_HEIGHT 64
#endif
#ifndef OLED_PIXEL_WIDTH
#define OLED_PIXEL_WIDTH  128
#endif

#ifndef OLED_I2C_ADDR
#define OLED_I2C_ADDR 0x3c
#endif

#ifndef OLED_RST
#define OLED_RST 16
#endif
#ifndef OLED_SCL
#define OLED_SCL 15
#endif
#ifndef OLED_SDA
#define OLED_SDA 4
#endif

#ifndef OLED_UPDATE_RATE_MS
#define OLED_UPDATE_RATE_MS 1000
#endif

// below must be larger than above...

#ifndef OLED_REDRAW_RATE_MS
#define OLED_REDRAW_RATE_MS 2000
#endif

#ifndef OLED_DISPLAY_SLEEP_TIMEOUT_MS
#define OLED_DISPLAY_SLEEP_TIMEOUT_MS 2*60*1000
#endif

// prototypes
uint8_t getWifiQuality(long rssi);
void updateInfo();
String getEffectPaletteName(uint8_t effectPaletteIndex);
String getEffectModeName(uint8_t effectModeIndex);
String getStringFromJSON(const char* JSONArray, uint8_t index);
void oled_drawDisplay();

bool oled_firstRun;
bool oled_redrawRequired;
long oled_lastRedrawTime;
long oled_lastUpdateTime;
bool oled_displayIsAsleep;

String oled_knownSSID;
long oled_knownRSSI;
IPAddress oled_knownIP;
uint8_t oled_knownBrightness;
uint8_t oled_knownEffectModeIndex;
uint8_t oled_knownEffectPaletteIndex;
String oled_knownEffectModeName;
String oled_knownEffectPaletteName;

char chBuffer[128];

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R2, OLED_RST, OLED_SCL, OLED_SDA);

//gets called once at boot. Do all initialization that doesn't depend on network here
void userSetup() {
  oled_firstRun = true;
  oled_redrawRequired = true;

  oled_displayIsAsleep = false;

  oled_lastRedrawTime = 0;
  oled_lastUpdateTime = 0;

  oled_knownBrightness = 0;
  oled_knownEffectModeIndex = 0;
  oled_knownEffectPaletteIndex = 0;

  DEBUG_PRINTLN("OLED Starting");
  oled.begin();
  oled.setFont(u8g2_font_6x10_tr);
  oled.setFontRefHeightExtendedText();
  oled.setDrawColor(1);
  oled.setFontPosTop();
  oled.setFontDirection(0);
  oled.clearBuffer();
  sprintf(chBuffer, "%s", "WLED Starting...");
  oled.drawStr(64 - (oled.getStrWidth(chBuffer) / 2), 0, chBuffer);
  oled.sendBuffer();
}

// gets called every time WiFi is (re-)connected. Initialize own network
// interfaces here
void userConnected() {
  updateInfo();
}

void userLoop()
{
  
  if ((millis() - oled_lastUpdateTime) > OLED_UPDATE_RATE_MS) {
    updateInfo();
    oled_lastUpdateTime = millis();
  }

  if (oled_redrawRequired)
  {
    oled_displayIsAsleep = false;
    oled.sleepOff();
  }

  if (!oled_redrawRequired) {
    if (oled_displayIsAsleep)
    {
      return;
    } else {
      if ((millis() - oled_lastRedrawTime) > OLED_DISPLAY_SLEEP_TIMEOUT_MS)
      {
        DEBUG_PRINTLN("OLED putting display to sleep");
        oled.sleepOn();
        oled_displayIsAsleep = true;
        return;
      }
    }
  } 

  if (oled_redrawRequired)
  {
    if ((millis() - oled_lastRedrawTime) > OLED_REDRAW_RATE_MS) {
      oled_drawDisplay();
      oled_lastRedrawTime = millis();
    }
  }

}

void updateInfo()
{
//  DEBUG_PRINTLN("OLED info update");

  String currentSSID;
  long currentRSSI;
  IPAddress currentIP;
  uint8_t currentBrightness;
  uint8_t currentEffectModeIndex;
  uint8_t currentEffectPaletteIndex;
  String currentEffectModeName;
  String currentPaletteModeName;

  if(WiFi.status() == WL_CONNECTED)
  {
    // Update last known values.
#if defined(ESP8266)
    oled_knownSSID = apActive ? WiFi.softAPSSID() : WiFi.SSID();
#else
    currentSSID = WiFi.SSID();
#endif
  } else {
    currentSSID = "~NOT CONNECTED~";
  }

  if (oled_firstRun || (currentSSID != oled_knownSSID)) { oled_knownSSID = currentSSID; oled_redrawRequired = true; }

  currentRSSI = WiFi.RSSI();
  if (oled_firstRun || (currentRSSI != oled_knownRSSI)) { oled_knownRSSI = currentRSSI; /* oled_redrawRequired = true; this changes _a lot_ */ }

  currentIP = apActive ? IPAddress(0, 0, 0, 0) : WiFi.localIP();
  if (oled_firstRun || (currentIP != oled_knownIP)) { oled_knownIP = currentIP; oled_redrawRequired = true; }

  currentBrightness = bri;
  if (oled_firstRun || (currentBrightness != oled_knownBrightness)) { oled_knownBrightness = currentBrightness; oled_redrawRequired = true; }

  currentEffectModeIndex = strip.getMode();
  if (oled_firstRun || (currentEffectModeIndex != oled_knownEffectModeIndex)) {
    oled_knownEffectModeIndex = currentEffectModeIndex;
    oled_knownEffectModeName = getEffectModeName(oled_knownEffectModeIndex);  
    oled_redrawRequired = true;
  }

  currentEffectPaletteIndex = strip.getSegment(0).palette;
  if (oled_firstRun || (currentEffectPaletteIndex != oled_knownEffectPaletteIndex)) {
    oled_knownEffectPaletteIndex = currentEffectPaletteIndex;
    oled_knownEffectPaletteName = getEffectPaletteName(oled_knownEffectPaletteIndex);
    oled_redrawRequired = true;
  }

  if (oled_firstRun) oled_firstRun = false;
}

void oled_drawDisplay()
{
  DEBUG_PRINTLN("OLED draw display");

  oled.clearBuffer();

  sprintf(chBuffer, "Wifi:%s", oled_knownSSID.c_str());
  oled.drawStr(0,0, chBuffer);

  sprintf(chBuffer, "Signal strength:%d%%", getWifiQuality(oled_knownRSSI));
  oled.drawStr(0,10, chBuffer);

  char chIP[81];
  oled_knownIP.toString().toCharArray(chIP, sizeof(chIP) -1);
  sprintf(chBuffer, "IP:%s", chIP);
  oled.drawStr(0,20, chBuffer);

  sprintf(chBuffer, "Mode:%s", oled_knownEffectModeName.c_str());
  oled.drawStr(0,30, chBuffer);

  sprintf(chBuffer, "Palette:%s", oled_knownEffectPaletteName.c_str());
  oled.drawStr(0,40, chBuffer);

  sprintf(chBuffer, "Brightness:%d", oled_knownBrightness);
  oled.drawStr(0,50, chBuffer);

  oled.sendBuffer();

  oled_redrawRequired = false;
}

String getEffectPaletteName(uint8_t effectPaletteIndex)
{
  return getStringFromJSON(JSON_palette_names, effectPaletteIndex);
}

String getEffectModeName(uint8_t effectModeIndex)
{
  return getStringFromJSON(JSON_mode_names, effectModeIndex);
}

String getStringFromJSON(const char* JSONArray, uint8_t index)
{
  String jsonItem = "";

  uint8_t qComma = 0;
  bool insideQuotes = false;
  uint8_t printedChars = 0;

  char singleJsonSymbol;

  // Find the mode name in JSON
  for (size_t i = 0; i < strlen_P(JSONArray); i++) {
    singleJsonSymbol = pgm_read_byte_near(JSONArray + i);
    switch (singleJsonSymbol) {
      case '"':
        insideQuotes = !insideQuotes;
        continue;
      case '[':
      case ']':
        continue;
      case ',':
        qComma++;
      default:
        if (!insideQuotes || (qComma != index))
          continue;
        jsonItem += singleJsonSymbol;
        printedChars++;
    }
    if ((qComma > index)) // || (printedChars > tftcharwidth - 1))
      break;
  }

  return jsonItem;

}

  // // Fourth row with palette name
  // tft.setCursor(1, 100);
  // qComma = 0;
  // insideQuotes = false;
  // printedChars = 0;
  // // Looking for palette name in JSON.
  // for (size_t i = 0; i < strlen_P(JSON_palette_names); i++) {
  //   singleJsonSymbol = pgm_read_byte_near(JSON_palette_names + i);
  //   switch (singleJsonSymbol) {
  //   case '"':
  //     insideQuotes = !insideQuotes;
  //     break;
  //   case '[':
  //   case ']':
  //     break;
  //   case ',':
  //     qComma++;
  //   default:
  //     if (!insideQuotes || (qComma != knownPalette))
  //       break;
  //     tft.print(singleJsonSymbol);
  //     printedChars++;
  //   }
  //   // The following is modified from the code from the u8g2/u8g8 based code (knownPalette was knownMode)
  //   if ((qComma > knownPalette) || (printedChars > tftcharwidth - 1))
  //     break;
  // }

uint8_t getWifiQuality(long rssi)
{
  long quality = 0;
  // dBm to Quality:
    if(rssi <= -100)
        quality = 0;
    else if(rssi >= -50)
        quality = 100;
    else
        quality = 2 * (rssi + 100);
  
  uint8_t value = 0;

  if (quality>100) value = 100;
  if (quality<0) value = 0;

  value = (uint8_t) quality;

  return value;
}
