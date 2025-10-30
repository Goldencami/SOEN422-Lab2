# SOEN422-Lab2
The purpose of this lab was to create an IoT Jukebox using a TTGO LoRa32-OLED which plays a song based on the preference of the user in the room using a song data API (part1). Then the functionality of the jukebox is extended with a bluetooth controller that enables actions like play/pause, next song, previous song. The lab assignment focuses more on networking and programming than the hardware and circuits.

## Setup
### Adding ESP32 Board Manager URL
```bash
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

### Libraries
Please install the following libraries:
- BluetoothSerial
- Arduino_JSON
- NimBLE Arduino

### Boards Manager
Please install the following board:
- esp32

## Baud Rate
For MacOS, we need to lower the baud rate to 115200 as it is initially at 921600. This will stop the error messages when deploying the code
```Tools → Upload Speed → 115200```

<img width="469" height="114" alt="Image" src="https://github.com/user-attachments/assets/ba84d6d0-5f07-4f21-97fa-840a8535f21c" />

## Part 1
This first part of the lab consist of scanning nearby bluetooth devices until it finds two desired devices (`DEVICE1_NAME` and `DEVICE2_NAME`). The preferred song of these devices gets fetched and starts playing through the buzzer.

## Part 2
In the second part of the lab, we recreate a playlist by using an interface through the application Bluefruit Connect. When the system starts, 5 random songs are fetched and stored in a queue to recreate a playlist. Later we can start using the following functions through BLE: play, pause, next song, previous song. These functionalities allow us to keep track of the songs in the list.

In addition, if there are songs ahead of the current song in the queue it should play those in order, if we are already at the tail of the queue the next song will be a random song using the GET `/song` endpoint.

# Notes
When adding both Bluetooth and wifi packages to the TTGO there is a possibility your program exceeds the memory limitation on the TTGO. Do not worry the configuration limits the memory at the software level, in other words there is more memory to be used!
1. Edit the 'boards.txt' file so that the value of the maximum upload size changes from its default 1310720 to 3407872. This .txt file can be found in your ArduinoData folder if you properly installed ESP32 in the boards manager for the firsts labs:
`\ArduinoData\packages\esp32\hardware\esp32\2.0.5\boards.txt`

2. Create a new .csv file named "default" or “partition” inside the folder of your sketch, with the following partition scheme:
```bash
# Name, Type, SubType, Offset, Size, Flags
nvs, data, nvs, 0x9000, 0x5000,
otadata, data, ota, 0xe000, 0x2000,
app0, app, ota_0, 0x10000, 0x340000,
eeprom, data, 0x99, 0x350000, 0x1000,
spiffs, data, spiffs, 0x351000, 0xAF000,
```
