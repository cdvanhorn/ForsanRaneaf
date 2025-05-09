# Building Raylib w/o X11 on Ubuntu WSL
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

If using WSL2 with an Intel card, https://www.intel.com/content/www/us/en/docs/oneapi/installation-guide-linux/2023-0/configure-wsl-2-for-gpu-workflows.html

Could not get it to work!

# Building Raylib w/X11 on Ubuntu WSL
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
