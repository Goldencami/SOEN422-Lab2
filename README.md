# SOEN422-Lab2
The purpose of this lab was to create an IoT Jukebox using a TTGO LoRa32 which plays a song based on the preference of the user in the room using a song data API (part1). Then the functionality of the jukebox is extended with a bluetooth controller that enables actions like play/pause, next song, previous song. The lab assignment focuses more on networking and programming than the hardware and circuits.

## Setup
### Adding ESP32 Board Manager URL
```bash
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

### Libraries
Please install esp32 and Arduino_JSON libraries.

## Baud Rate
For MacOS, we need to lower the baud rate to 115200 as it is initially at 921600. This will stop the error messages when deploying the code
```Tools → Upload Speed → 115200```

<img width="469" height="114" alt="Image" src="https://github.com/user-attachments/assets/ba84d6d0-5f07-4f21-97fa-840a8535f21c" />
