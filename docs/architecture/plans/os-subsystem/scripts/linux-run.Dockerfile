# Linux image for os-subsystem's linux-run (D75): the packages of CI's Linux
# setup (.github/actions/native-setup/action.yml), plus git and a Python venv
# for CMake 3.31.6, and ccache so the three checkouts share compiled objects.
# Go is left out: only Go tests (label "go") and explicit service targets use it.
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      gcc-14 g++-14 ninja-build pkg-config \
      libasound2-dev libpulse-dev libudev-dev libx11-dev libxext-dev \
      libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
      libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
      libdrm-dev libgbm-dev libegl1-mesa-dev libgl1-mesa-dev libdbus-1-dev \
      mesa-vulkan-drivers vulkan-tools xvfb xauth \
      git python3 python3-venv ca-certificates ccache \
    && rm -rf /var/lib/apt/lists/*
RUN python3 -m venv /opt/cmake && /opt/cmake/bin/pip install --no-cache-dir cmake==3.31.6
ENV PATH=/opt/cmake/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    CC=gcc-14 CXX=g++-14 CCACHE_DIR=/work/ccache CCACHE_BASEDIR=/work CCACHE_NOHASHDIR=1
