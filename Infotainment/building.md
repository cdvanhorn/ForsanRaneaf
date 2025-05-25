# Building Raylib w/o X11 on Ubuntu
```shell
sudo apt install build-essential git
sudo apt install cmake
sudo apt install libasound2-dev libgl1-mesa-dev libglu1-mesa-dev libgbm-dev libdrm-dev
```

If cmake is too old, get it from their repository, https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line

```shell
git clone https://github.com/raysan5/raylib.git raylib
cd raylib
mkdir build && cd build
cmake -DPLATFORM=DRM -DSUPPORT_FILEFORMAT_FLAC=ON ..
make
```
Could not get it to work! :(

# Building Raylib w/X11 on Ubuntu
```shell
sudo apt install build-essential git
sudo apt install cmake
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev libwayland-dev libxkbcommon-dev
```
If cmake is too old, get it from their repository, https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line

```shell
git clone https://github.com/raysan5/raylib.git raylib
cd raylib
mkdir build && cd build
cmake -DSUPPORT_FILEFORMAT_FLAC=ON ..
make
```

# Building Raylib w/o X11 on Raspberry Pi
```shell
sudo apt install libdrm-dev libegl1-mesa-dev libgles2-mesa-dev libgbm-dev
git clone https://github.com/raysan5/raylib.git raylib
cd raylib
mkdir build && cd build
cmake -DPLATFORM=DRM -DSUPPORT_FILEFORMAT_FLAC=ON ..
make
```
Worked without issue.

# Building Raygui on Ubuntu

You don't have to build raygui just copy the header to the project.

# Building SQLite on Ubuntu

Doesn't need to be built, just download the amalgamation file (https://www.sqlite.org/download.html) and include it in the project.
https://www.sqlite.org/howtocompile.html

# Building Turbo-Base64

```shell
git clone https://github.com/powturbo/Turbo-Base64
cd Turbo-Base64
mkdir build && cd build
cmake ..
make
```
