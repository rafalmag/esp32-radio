#include <VS1053.h> //https://github.com/baldram/ESP_VS1053_Library
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_wifi.h>
#include "display.h"
#include "helper.h"

static const char TAG[] = "radio";
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#define VS1053_CS 32
#define VS1053_DCS 33
#define VS1053_DREQ 35

int volume = 80; // volume level 0-100
int lastVolume = volume;

volatile int8_t radioStation = 0;
int previousRadioStation = -1;

// char ssid[] = "yourSSID";         //  your network SSID (name)
// char password[] = "yourWifiPassword"; // your network password
#include "myWifi.h"

// Few Radio Stations
char *host[5] = {"149.255.59.162", "d.dktr.pl", "41.dktr.pl", "w.dktr.pl", "stream3.polskieradio.pl"};
char *path[5] = {"/1", "/trojka.ogg", "/trojka.ogg", "/trojka.ogg", "/"};
int port[5] = {8062, 8000, 8000, 8000, 8904};

int status = WL_IDLE_STATUS;
WiFiClient client;
uint8_t mp3buff[32]; // vs1053 likes 32 bytes at a time

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

// issue with pin2 it status "0" all the time and breaks in the middle of interrupt,
// proably caused by being a ADC2 and Wifi is using those pins as well
#define ROTARY_ENCODER_A_PIN 16
#define ROTARY_ENCODER_B_PIN 17
// pull up required
#define ROTARY_ENCODER_BUTTON_PIN 15

#include "ESPRotary.h"
#include "Button2.h"

ESPRotary r = ESPRotary(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, 4);
Button2 b = Button2(ROTARY_ENCODER_BUTTON_PIN, INPUT_PULLUP);

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
    delay(100);
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
  updateStation(host[radioStation]);
}

void initMP3Decoder()
{
  ESP_LOGI(TAG, "Init player");
  player.begin();
  // player.switchToMp3Mode(); // optional, some boards require this
  player.setVolume(volume);
}

void IRAM_ATTR leftRotationHandler(ESPRotary &r)
{
  if (b.isPressed())
  {
    volume -= 10;
    if (volume < 0)
      volume = 0;
  }
  else
  {
    if (radioStation > 0)
      radioStation--;
    else
      radioStation = 4;
  }
}

void IRAM_ATTR rightRotationHandler(ESPRotary &r)
{
  if (b.isPressed())
  {
    volume += 10;
    if (volume > 100)
      volume = 100;
  }
  else
  {
    if (radioStation < 5)
      radioStation++;
    else
      radioStation = 0;
  }
}

void setup()
{
  Serial.begin(9600);
  delay(500);
  Serial.println();

  esp_log_level_set("ESP_VS1053", ESP_LOG_INFO); 

  SPI.begin();

  initMP3Decoder();
  initTft();

  connectToWIFI();

  r.setLeftRotationHandler(leftRotationHandler);
  r.setRightRotationHandler(rightRotationHandler);
}

void loop()
{
  r.loop();
  b.loop();

  EVERY_N_MINUTES(30)
  {
    initTimeFromNtp();
  }
  EVERY_N_MILLISECONDS(1000)
  {
    updateClock();
  }
  EVERY_N_MILLISECONDS(5000)
  {
    updateWifi(rssiToStrength(WiFi.RSSI()));
  }
  EVERY_N_MILLISECONDS(50)
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
  }

  if (client.available() > 0)
  {
    uint8_t bytesread = client.read(mp3buff, 32);
    if (lastVolume != volume)
      player.setVolume(volume);
    lastVolume = volume;
    player.playChunk(mp3buff, bytesread);
  }
}
