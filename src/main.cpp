#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <VS1053.h> //https://github.com/baldram/ESP_VS1053_Library
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_wifi.h>

static const char TAG[] = "radio";
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"


#define VS1053_CS 32
#define VS1053_DCS 33
#define VS1053_DREQ 35

#define VOLUME 95 // volume level 0-100

int radioStation = 0;
int previousRadioStation = -1;
const int previousButton = 12;
const int nextButton = 13;

// char ssid[] = "yourSSID";         //  your network SSID (name)
// char password[] = "yourWifiPassword"; // your network password
#include "myWifi.h"

// Few Radio Stations
// only one not working: w.dktr.pl
char *host[5] = {"149.255.59.162", "d.dktr.pl", "41.dktr.pl", "w.dktr.pl", "stream3.polskieradio.pl"};
char *path[5] = {"/1", "/trojka.ogg", "/trojka.ogg", "/trojka.ogg","/"};
int port[5] = {8062, 8000, 8000, 8000,8904};

int status = WL_IDLE_STATUS;
WiFiClient client;
uint8_t mp3buff[32]; // vs1053 likes 32 bytes at a time

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

// TFT


TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

float sx = 0, sy = 1, mx = 1, my = 0, hx = -1, hy = 0;    // Saved H, M, S x & y multipliers
float sdeg=0, mdeg=0, hdeg=0;
uint16_t osx=120, osy=120, omx=120, omy=120, ohx=120, ohy=120;  // Saved H, M, S x & y coords
uint16_t x0=0, x1=0, yy0=0, yy1=0;
uint32_t targetTime = 0;                    // for next 1 second timeout

static uint8_t conv2d(const char* p); // Forward declaration needed for IDE 1.6.x
uint8_t hh=conv2d(__TIME__), mm=conv2d(__TIME__+3), ss=conv2d(__TIME__+6);  // Get H, M, S from compile time

boolean initial = 1;

word ConvertRGB( byte R, byte G, byte B)
{
  return ( ((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3) );
}
#define TFT_BACKGROUND TFT_BLACK
word CLOCK_BEZEL_COLOR = ConvertRGB(160,82,45); // sienna
// TFT

void initTft(){
  tft.init();
  tft.setRotation(0);
  
  tft.fillScreen(TFT_BACKGROUND);
  
  tft.setTextColor(TFT_WHITE, TFT_BACKGROUND);  // Adding a background colour erases previous text automatically
  
  // Draw clock face
  tft.fillCircle(120, 120, 118, CLOCK_BEZEL_COLOR);
  tft.fillCircle(120, 120, 110, TFT_BLACK);

  // Draw 12 lines
  for(int i = 0; i<360; i+= 30) {
    sx = cos((i-90)*0.0174532925);
    sy = sin((i-90)*0.0174532925);
    x0 = sx*114+120;
    yy0 = sy*114+120;
    x1 = sx*100+120;
    yy1 = sy*100+120;

    tft.drawLine(x0, yy0, x1, yy1, CLOCK_BEZEL_COLOR);
  }

  // Draw 60 dots
  for(int i = 0; i<360; i+= 6) {
    sx = cos((i-90)*0.0174532925);
    sy = sin((i-90)*0.0174532925);
    x0 = sx*102+120;
    yy0 = sy*102+120;
    // Draw minute markers
    tft.drawPixel(x0, yy0, TFT_WHITE);
    
    // Draw main quadrant dots
    if(i==0 || i==180) tft.fillCircle(x0, yy0, 2, TFT_WHITE);
    if(i==90 || i==270) tft.fillCircle(x0, yy0, 2, TFT_WHITE);
  }

  tft.fillCircle(120, 121, 3, TFT_WHITE);

  // Draw text at position 120,260 using fonts 4
  // Only font numbers 2,4,6,7 are valid. Font 6 only contains characters [space] 0 1 2 3 4 5 6 7 8 9 : . - a p m
  // Font 7 is a 7 segment font and only contains characters [space] 0 1 2 3 4 5 6 7 8 9 : .
  tft.drawCentreString(host[radioStation],120,260,4);

  targetTime = millis() + 1000; 
}

void IRAM_ATTR previousButtonInterrupt()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 200)
  {
    if (radioStation > 0)
      radioStation--;
    else
      radioStation = 4;
  }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR nextButtonInterrupt()
{

  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 200)
  {
    if (radioStation < 5)
      radioStation++;
    else
      radioStation = 0;
  }
  last_interrupt_time = interrupt_time;
}

int8_t rssiToStrength(int8_t rssiDb)
{
  if (rssiDb >= -50)
    return 100;
  else if (rssiDb <= -100)
    return 0;
  else
    return 2 * (rssiDb + 100);
}

void printSignalStrength()
{
  // https://www.netspotapp.com/what-is-rssi-level.html
  ESP_LOGI(TAG, "WiFi isConnected: %c, signal: %d %%", WiFi.isConnected() ? 'y' : 'n', rssiToStrength(WiFi.RSSI()));
}

void connectToWIFI()
{
  WiFi.begin(ssid, password);
  ESP_LOGI(TAG, "Wifi connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  ESP_LOGI(TAG, "WiFi connected, IP address: %s", WiFi.localIP().toString().c_str());
  printSignalStrength();
}

void station_connect(int station_no)
{
  player.stopSong();
  client.stop();
  printSignalStrength();
  if (!WiFi.isConnected())
  {
    WiFi.disconnect();
    connectToWIFI();
  }
  if (client.connect(host[station_no], port[station_no]))
    ESP_LOGI(TAG, "Connected now to %s:%d", host[station_no], port[station_no]);
  client.print(String("GET ") + path[station_no] + " HTTP/1.1\r\n" +
               "Host: " + host[station_no] + "\r\n" +
               "Connection: close\r\n\r\n");
  player.startSong();
}

void initMP3Decoder()
{
  ESP_LOGI(TAG, "Init player");
  player.begin();
  // player.switchToMp3Mode(); // optional, some boards require this
  player.setVolume(VOLUME);
}

static uint8_t conv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9')
    v = *p - '0';
  return 10 * v + *++p - '0';
}

void updateTft(){
 if (targetTime < millis()) {
    targetTime += 1000;
    ss++;              // Advance second
    if (ss==60) {
      ss=0;
      mm++;            // Advance minute
      if(mm>59) {
        mm=0;
        hh++;          // Advance hour
        if (hh>23) {
          hh=0;
        }
      }
    }

    // Pre-compute hand degrees, x & y coords for a fast screen update
    sdeg = ss*6;                  // 0-59 -> 0-354
    mdeg = mm*6+sdeg*0.01666667;  // 0-59 -> 0-360 - includes seconds
    hdeg = hh*30+mdeg*0.0833333;  // 0-11 -> 0-360 - includes minutes and seconds
    hx = cos((hdeg-90)*0.0174532925);    
    hy = sin((hdeg-90)*0.0174532925);
    mx = cos((mdeg-90)*0.0174532925);    
    my = sin((mdeg-90)*0.0174532925);
    sx = cos((sdeg-90)*0.0174532925);    
    sy = sin((sdeg-90)*0.0174532925);

    if (ss==0 || initial) {
      initial = 0;
      // Erase hour and minute hand positions every minute
      tft.drawLine(ohx, ohy, 120, 121, TFT_BLACK);
      ohx = hx*62+121;    
      ohy = hy*62+121;
      tft.drawLine(omx, omy, 120, 121, TFT_BLACK);
      omx = mx*84+120;    
      omy = my*84+121;
    }

      // Redraw new hand positions, hour and minute hands not erased here to avoid flicker
      tft.drawLine(osx, osy, 120, 121, TFT_BLACK);
      osx = sx*90+121;    
      osy = sy*90+121;
      tft.drawLine(osx, osy, 120, 121, TFT_RED);
      tft.drawLine(ohx, ohy, 120, 121, TFT_WHITE);
      tft.drawLine(omx, omy, 120, 121, TFT_WHITE);
      tft.drawLine(osx, osy, 120, 121, TFT_RED);

    tft.fillCircle(120, 121, 3, TFT_RED);
    //TODO to avoid flickering maybe redraw less often
    tft.fillRect(0,260,240,30,TFT_BACKGROUND);
    // tft.drawCentreString("                                        ",120,260,4);
    tft.drawCentreString(host[radioStation],120,260,4);
    char text[255];
    sprintf(text," Wifi %d %%",rssiToStrength(WiFi.RSSI()));
    tft.drawCentreString(text,120,290,4);
  }
}

void setup()
{
  Serial.begin(9600);
  delay(500);
  Serial.println();
  SPI.begin();

  pinMode(previousButton, INPUT_PULLUP);
  pinMode(nextButton, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(previousButton), previousButtonInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(nextButton), nextButtonInterrupt, FALLING);

  initMP3Decoder();
  initTft();

  connectToWIFI();
}

void loop()
{
  updateTft();
  if (radioStation != previousRadioStation)
  {
    station_connect(radioStation);
    previousRadioStation = radioStation;
  }

  if (!client.connected())
  {
    ESP_LOGI(TAG, "Reconnecting...");
    station_connect(radioStation);
  }

  if (client.available() > 0)
  {
    uint8_t bytesread = client.read(mp3buff, 32);
    player.playChunk(mp3buff, bytesread);
  }
}
