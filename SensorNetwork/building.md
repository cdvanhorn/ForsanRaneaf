### Setup CLion for ESP32 Development

- https://www.jetbrains.com/help/clion/esp-idf.html
- https://developer.espressif.com/blog/clion/
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/linux-macos-setup.html#standard-toolchain-setup-for-linux-and-macos

```shell
sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
cd Projects
mkdir esp32 && cd esp32
git clone -b v5.4.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. $HOME/Projects/esp/esp-idf/export.sh # to put esp32 tools in path
```