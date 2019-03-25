#include <VS1053.h> //https://github.com/baldram/ESP_VS1053_Library
#include <WiFi.h>
#include <WiFiMulti.h>
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

volatile int8_t volume = 80; // volume level 0-100
int8_t lastVolume = volume;

volatile int8_t radioStation = 0;
int8_t previousRadioStation = -1;

// char ssid[] = "yourSSID";         //  your network SSID (name)
// char password[] = "yourWifiPassword"; // your network password
#include "myWifi.h"
WiFiMulti wifiMulti;

// Few Radio Stations

struct RadioStation
{
  char *name;
  char *host;
  char *path;
  int port;
};

#define RADIO_STATIONS 6
RadioStation radioStations[RADIO_STATIONS] = {
    {"Trojka D", "d.dktr.pl", "/trojka.ogg", 8000},                                 // 224 kbps, 32bit/sample, 48kHz
    {"Trojka 41", "41.dktr.pl", "/trojka.ogg", 8000},                               // 224 kbps, 32bit/sample, 48kHz
    {"Trojka W", "w.dktr.pl", "/trojka.ogg", 8000},                                 // 224 kbps, 32bit/sample, 48kHz
    {"Trojka S3", "stream3.polskieradio.pl", "/", 8904},                            // 96 kbps, 32bit/sample, 44.1kHz
    {"Chili Zet", "n-6-1.dcs.redcdn.pl", "/sc/o2/Eurozet/live/chillizet.livx", 80}, // 128 kbps, 32bit/sample, 44.1kHz
    {"The UK 1940s", "149.255.59.162", "/1", 8062}                                  // 128 kbps, 32bit/sample, 44.1kHz, mono
                                                                                    // left empty for formatting
};

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
  ESP_LOGI(TAG, "Wifi connecting");
  while (wifiMulti.run() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  ESP_LOGI(TAG, "WiFi connected, IP address: %s", WiFi.localIP().toString().c_str());
  printSignalStrength();
}

void resetMp3Decoder()
{
  ESP_LOGI(TAG, "Reset VS1053...");
  digitalWrite(VS1053_DCS, LOW); // Low & Low will bring reset pin low
  digitalWrite(VS1053_CS, LOW);
  delay(100);
  ESP_LOGI(TAG, "End reset VS1053...");
  digitalWrite(VS1053_DCS, HIGH); // Back to normal again
  digitalWrite(VS1053_CS, HIGH);
  delay(100);
  player.softReset();
}

void station_connect(int station_no)
{
  //TODO double check station no ??
  player.stopSong();
  client.stop();
  updateStation(radioStations[station_no].name);
  resetMp3Decoder();
  printSignalStrength();
  if (!WiFi.isConnected())
  {
    WiFi.disconnect();
    connectToWIFI();
  }
  if (client.connect(radioStations[station_no].host, radioStations[station_no].port))
  {
    ESP_LOGI(TAG, "Connected now to %s:%d", radioStations[station_no].host, radioStations[station_no].port);
    client.print(String("GET ") + radioStations[station_no].path + " HTTP/1.1\r\n" +
                 "Host: " + radioStations[station_no].host + "\r\n" +
                 "Connection: close\r\n\r\n");
    player.startSong();
  }
  else
    ESP_LOGI(TAG, "Connection FAILED radioStation %d, host:port %s:%d", station_no, radioStations[station_no].host, radioStations[station_no].port);
}

void initMP3Decoder()
{
  ESP_LOGI(TAG, "Init player");
  player.begin();
  player.switchToMp3Mode(); // optional, some boards require this
  player.setVolume(volume);
}

void IRAM_ATTR leftRotationHandler(ESPRotary &r)
{
  if (b.isPressed())
  {
    int8_t tempVolume = volume - 10;
    if (tempVolume < 0)
      tempVolume = 0;
    volume = tempVolume;
  }
  else
  {
    int8_t tempRadioStation = radioStation - 1;
    if (tempRadioStation <= 0)
      tempRadioStation = RADIO_STATIONS - 1;
    radioStation = tempRadioStation;
  }
}

void IRAM_ATTR rightRotationHandler(ESPRotary &r)
{
  if (b.isPressed())
  {
    int8_t tempVolume = volume + 10;
    if (tempVolume > 100)
      tempVolume = 100;
    volume = tempVolume;
  }
  else
  {
    int8_t tempRadioStation = radioStation + 1;
    if (tempRadioStation >= RADIO_STATIONS)
      tempRadioStation = 0;
    radioStation = tempRadioStation;
  }
}

void setup()
{
  Serial.begin(9600);
  delay(500);
  Serial.println();

  SPI.begin();

  initTft();
  initMP3Decoder();

  wifiMulti.addAP(ssid1, password1);
  wifiMulti.addAP(ssid2, password2);
  wifiMulti.addAP(ssid3, password3);
  wifiMulti.addAP(ssid4, password4);
  connectToWIFI();

  r.setLeftRotationHandler(leftRotationHandler);
  r.setRightRotationHandler(rightRotationHandler);
}

void loop()
{
  r.loop();
  b.loop();

  // it must be more often in case the previous attempt was failure
  EVERY_N_MILLISECONDS(100)
  {
    updateTimeIfNeeded();
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
