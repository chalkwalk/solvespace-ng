#!/bin/sh -xe

sudo apt-get update -qq

sudo apt-get install -q -y \
  zlib1g-dev libpng-dev libcairo2-dev libfreetype6-dev libjson-c-dev \
  libfontconfig1-dev libgl-dev libglu-dev libspnav-dev

# Development headers for building the vendored SDL3 (solvespace-ng), and Mesa's EGL for the
# UI interaction tests; Xvfb, xdotool and python3 for the X11 tests.
sudo apt-get install -q -y \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxfixes-dev \
  libxkbcommon-dev libwayland-dev wayland-protocols libegl-dev libdecor-0-dev \
  libegl-mesa0 libgl1-mesa-dri xvfb xdotool python3

git submodule update --init extlib/libdxfrw extlib/mimalloc extlib/eigen extlib/SDL \
  extlib/imgui extlib/lunasvg extlib/occt
