## Requirements
* C GUI DRM (direct rendering manager) application using raylib (https://www.raylib.com/index.html).
* The application will display warnings and errors (MVP)
* The application will display vehicle status (MVP)
* The application will play audio files from filesystem using cmus and cmus-remote (https://cmus.github.io/#home)
* The application will have an interface for an FM tuner
* The application will run on a Raspberry Pi 4 with the Raspberry Pi touch display 2 (https://www.raspberrypi.com/products/touch-display-2/) with Raspbian or DietPi OS (MVP)
* The application will run in "kiosk" mode on the Raspberry Pi (MVP)
* When the car is plugged in and can access the home wifi, the Raspberry Pi will rsync the music library
* The application will drive a serial display to display current speed for driver (https://www.digikey.com/en/products/detail/newhaven-display-intl/NHD-3-12-25664UMY3/3712528)
* The application will drive a serial display to display current RPM for driver
* The application will use a serial connection to the ESP32 cluster to retrieve vehicle status, warnings and errors (MVP)
* Custom raylib audio player
* MusicBrainz raylib player integration (https://musicbrainz.org/doc/MusicBrainz_API)

## Guides
* https://madwonder.com/raylib-basic-setup/

## Parts
* https://geekworm.com/products/x825?variant=39330313568344
* 1TB 2.5 SSD Drive
