#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library

float sx = 0, sy = 1, mx = 1, my = 0, hx = -1, hy = 0; // Saved H, M, S x & y multipliers
float sdeg = 0, mdeg = 0, hdeg = 0;
uint16_t osx = 120, osy = 120, omx = 120, omy = 120, ohx = 120, ohy = 120; // Saved H, M, S x & y coords
uint16_t x0 = 0, x1 = 0, yy0 = 0, yy1 = 0;
uint32_t targetTime = 0; // for next 1 second timeout

static uint8_t conv2d(const char *p)
{
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9')
    v = *p - '0';
  return 10 * v + *++p - '0';
}

// uint8_t hh = conv2d(__TIME__), mm = conv2d(__TIME__ + 3), ss = conv2d(__TIME__ + 6); // Get H, M, S from compile time
uint8_t hh = 0, mm = 0, ss = 0;

boolean initial = 1;

word ConvertRGB(byte R, byte G, byte B)
{
  return (((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3));
}
#define TFT_BACKGROUND TFT_BLACK
word CLOCK_BEZEL_COLOR = ConvertRGB(160, 82, 45); // sienna
// TFT

void initTft()
{
  ESP_LOGI(TAG, "Init TFT");
  tft.init();
  tft.setRotation(0);

  tft.fillScreen(TFT_BACKGROUND);

  tft.setTextColor(TFT_WHITE, TFT_BACKGROUND); // Adding a background colour erases previous text automatically

  // Draw clock face
  tft.fillCircle(120, 120, 118, CLOCK_BEZEL_COLOR);
  tft.fillCircle(120, 120, 110, TFT_BLACK);

  // Draw 12 lines
  for (int i = 0; i < 360; i += 30)
  {
    sx = cos((i - 90) * 0.0174532925);
    sy = sin((i - 90) * 0.0174532925);
    x0 = sx * 114 + 120;
    yy0 = sy * 114 + 120;
    x1 = sx * 100 + 120;
    yy1 = sy * 100 + 120;

    tft.drawLine(x0, yy0, x1, yy1, CLOCK_BEZEL_COLOR);
  }

  // Draw 60 dots
  for (int i = 0; i < 360; i += 6)
  {
    sx = cos((i - 90) * 0.0174532925);
    sy = sin((i - 90) * 0.0174532925);
    x0 = sx * 102 + 120;
    yy0 = sy * 102 + 120;
    // Draw minute markers
    tft.drawPixel(x0, yy0, TFT_WHITE);

    // Draw main quadrant dots
    if (i == 0 || i == 180)
      tft.fillCircle(x0, yy0, 2, TFT_WHITE);
    if (i == 90 || i == 270)
      tft.fillCircle(x0, yy0, 2, TFT_WHITE);
  }

  tft.fillCircle(120, 121, 3, TFT_WHITE);

  targetTime = millis() + 1000;
}

/**
 * TODO NTP clock:
 * http://embedded-lab.com/blog/tutorial-8-esp8266-internet-clock/
 * ntp + winter time:
 * https://github.com/marcudanf/esp-ntp/blob/master/NTPtimeESP-master/NTPtimeESP.cpp
 */
void updateClock()
{
  if (targetTime < millis())
  {
    targetTime += 1000;
    ss++; // Advance second
    if (ss == 60)
    {
      ss = 0;
      mm++; // Advance minute
      if (mm > 59)
      {
        mm = 0;
        hh++; // Advance hour
        if (hh > 23)
        {
          hh = 0;
        }
      }
    }

    // Pre-compute hand degrees, x & y coords for a fast screen update
    sdeg = ss * 6;                     // 0-59 -> 0-354
    mdeg = mm * 6 + sdeg * 0.01666667; // 0-59 -> 0-360 - includes seconds
    hdeg = hh * 30 + mdeg * 0.0833333; // 0-11 -> 0-360 - includes minutes and seconds
    hx = cos((hdeg - 90) * 0.0174532925);
    hy = sin((hdeg - 90) * 0.0174532925);
    mx = cos((mdeg - 90) * 0.0174532925);
    my = sin((mdeg - 90) * 0.0174532925);
    sx = cos((sdeg - 90) * 0.0174532925);
    sy = sin((sdeg - 90) * 0.0174532925);

    if (ss == 0 || initial)
    {
      initial = 0;
      // Erase hour and minute hand positions every minute
      tft.drawLine(ohx, ohy, 120, 121, TFT_BLACK);
      ohx = hx * 62 + 121;
      ohy = hy * 62 + 121;
      tft.drawLine(omx, omy, 120, 121, TFT_BLACK);
      omx = mx * 84 + 120;
      omy = my * 84 + 121;
    }

    // Redraw new hand positions, hour and minute hands not erased here to avoid flicker
    tft.drawLine(osx, osy, 120, 121, TFT_BLACK);
    osx = sx * 90 + 121;
    osy = sy * 90 + 121;
    tft.drawLine(osx, osy, 120, 121, TFT_RED);
    tft.drawLine(ohx, ohy, 120, 121, TFT_WHITE);
    tft.drawLine(omx, omy, 120, 121, TFT_WHITE);
    tft.drawLine(osx, osy, 120, 121, TFT_RED);

    tft.fillCircle(120, 121, 3, TFT_RED);
  }
}

void updateStation(char *station)
{
  tft.fillRect(0, 260, 240, 30, TFT_BACKGROUND);
  // Draw text at position 120,260 using fonts 4
  // Only font numbers 2,4,6,7 are valid. Font 6 only contains characters [space] 0 1 2 3 4 5 6 7 8 9 : . - a p m
  // Font 7 is a 7 segment font and only contains characters [space] 0 1 2 3 4 5 6 7 8 9 : .
  tft.drawCentreString(station, 120, 260, 4);
}

void updateWifi(int8_t wifiStrength)
{
  tft.fillRect(0, 290, 240, 30, TFT_BACKGROUND);
  char text[255];
  sprintf(text, "Wifi %d %%", wifiStrength);
  tft.drawCentreString(text, 120, 290, 4);
}

#include <time.h>

unsigned long A_30_MINS_IN_MS = 30 * 60 * 1000;

inline void updateTimeIfNeeded()
{
  static unsigned long lastNtpUpdated = 0;
  if (lastNtpUpdated == 0 || millis() - lastNtpUpdated >= A_30_MINS_IN_MS)
  {
    ESP_LOGI(TAG, "tryToUpdateTimeFromNtp, lastNtpUpdated = %d", lastNtpUpdated);
    // https://www.pool.ntp.org/zone/pl
    // Set timezone to CEST
    configTzTime("CET-1CEST,M3.5.0,M10.5.0", "2.pl.pool.ntp.org", "1.pl.pool.ntp.org", "0.pl.pool.ntp.org");

    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
      ss = timeinfo.tm_sec;
      mm = timeinfo.tm_min;
      hh = timeinfo.tm_hour;
      ESP_LOGI(TAG, "Updated time %d:%d:%d", hh, mm, ss);
      lastNtpUpdated = millis();
      initial = 1;
    }
  }
}
