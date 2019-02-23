/////////////////////////////////////////////////////////////////
//         ESP32 Internet Radio Project     v1.00              //
//       Get the latest version of the code here:              //
//          http://educ8s.tv/esp32-internet-radio              //
/////////////////////////////////////////////////////////////////

#include <VS1053.h> //https://github.com/baldram/ESP_VS1053_Library
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_wifi.h>

static const char TAG[] = "MyModule";
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
char *host[4] = {"149.255.59.162", "w.dktr.pl", "41.dktr.pl", "secure1.live24.gr"};
char *path[4] = {"/1", "/trojka3.ogg", "/trojka.ogg", "/skai1003"};
int port[4] = {8062, 8000, 8000, 80};

int status = WL_IDLE_STATUS;
WiFiClient client;
uint8_t mp3buff[32]; // vs1053 likes 32 bytes at a time

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

void IRAM_ATTR previousButtonInterrupt()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 200)
  {
    if (radioStation > 0)
      radioStation--;
    else
      radioStation = 3;
  }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR nextButtonInterrupt()
{

  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 200)
  {
    if (radioStation < 4)
      radioStation++;
    else
      radioStation = 0;
  }
  last_interrupt_time = interrupt_time;
}

void printSignalStrength()
{
  // https://www.netspotapp.com/what-is-rssi-level.html
  ESP_LOGI(TAG, "WiFi isConnected: %c, signal: %d dBm", WiFi.isConnected()?'y':'n', WiFi.RSSI());
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
}

void initMP3Decoder()
{
  ESP_LOGI(TAG, "Init player");
  player.begin();
  // player.switchToMp3Mode(); // optional, some boards require this
  player.setVolume(VOLUME);
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

  connectToWIFI();
}

void loop()
{
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
