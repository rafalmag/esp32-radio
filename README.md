# esp32-radio

work in progress

## Hardware used:
- ESP32 - ESP32 DevKit
- VS1053 board
  - connected to: MISO=19, MOSI=23, SCLK=18, (X)CS=32, (X)DCS=33, DREQ=35, (X)RST=EN, GND, 5V (pins configured in main.c)
- 2.4" TFT SPI 240x320 using ILI9341 IC
  - connected to: SDO(MISO)=19, SDI(MOSI)=23, SCK=18, CS=27, D/C=33, RST=EN, VCC=3.3V, GND, LED through 1k potentiometer to 3.3V (pins configured in platformio.ini via build_flags)
- mono 18W audio amplifier (TDA2030A, working voltage: 6 ~ 12V, peak output current is 3A, volume regulated by 10k potentiometer)
- rotary encoder (with home made hardware debouncer)
  - connected to pins 15,16,17
- power source 230V=>5V (25W)
- DC-DC step up converter 5V=>12V (LM2587, should easily handle 3A)

## Reference:
![esp32](ESP32-DOIT-DEVKIT-V1-Board-Pinout-36-GPIOs-updated.jpg "ESP32")


## Features

### Auto-recover from lost WiFi signal:
```
537: void initMP3Decoder() 112 Init player
3724: void connectToWIFI() 82 Wifi connecting..
4723: void connectToWIFI() 89 WiFi connected, IP address:
4724: void connectToWIFI() 90 192.168.1.8
4776: void printSignalStrength() 76 WiFi isConnected: 1, signal: -84dBm
4915: void station_connect(int) 104 Connected now to 149.255.59.162:8062
79772: void loop() 146 Reconnecting...
79772: void printSignalStrength() 76 WiFi isConnected: 0, signal: 0dBm
79788: void connectToWIFI() 82 Wifi connecting..
80803: void connectToWIFI() 89 WiFi connected, IP address:
80804: void connectToWIFI() 90 192.168.1.8
80804: void printSignalStrength() 76 WiFi isConnected: 1, signal: -76dBm
80919: void station_connect(int) 104 Connected now to 149.255.59.162:8062
```
