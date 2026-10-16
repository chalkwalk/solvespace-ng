#!/bin/sh -xe

sudo apt-get update -qq

sudo apt-get install -q -y \
  zlib1g-dev libpng-dev libcairo2-dev libfreetype6-dev libjson-c-dev \
  libfontconfig1-dev libgtkmm-3.0-dev libpangomm-1.4-dev libgl-dev \
  libgl-dev libglu-dev libspnav-dev

# Development headers for building the vendored SDL3 (solvespace-ng).
sudo apt-get install -q -y \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxfixes-dev \
  libxkbcommon-dev libwayland-dev wayland-protocols libegl-dev libdecor-0-dev

git submodule update --init extlib/libdxfrw extlib/mimalloc extlib/eigen extlib/SDL extlib/imgui
