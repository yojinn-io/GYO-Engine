# A Linux build and measurement environment that mirrors the L1 linux-x64 row
# (.github/workflows/cross-platform.yml with .github/actions/native-setup).
# Used to measure on Linux where the gate decides (kernel migration plan U8:
# a Docker Linux container is the gate machine; it runs under the host's VM).
# Versions are the ones the L1 row used (run 43244933991, 2026-11-12): the
# hosted ubuntu-24.04 image's cmake 3.31.6 and Python 3.12, apt gcc-14 and
# ninja-build, setup-go spec 1.23. When the L1 row changes, change this file
# with it. Nothing in CI uses this file.
FROM ubuntu:24.04@sha256:2198e4c65076e9cb0b3d695219df667c0e3b0072da64ca3c2648aca17dcecaf5

ARG CMAKE_VERSION=3.31.6
ARG GO_VERSION=1.23.12
ENV DEBIAN_FRONTEND=noninteractive

# The native-setup package list, plus what the hosted image already carries
# (git, curl, python3, procps for ps).
RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc-14=14.2.0-4ubuntu2~24.04.1 g++-14=14.2.0-4ubuntu2~24.04.1 ninja-build=1.11.1-2 pkg-config \
        libasound2-dev libpulse-dev libudev-dev libx11-dev libxext-dev \
        libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
        libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
        libdrm-dev libgbm-dev libegl1-mesa-dev libgl1-mesa-dev libdbus-1-dev \
        mesa-vulkan-drivers vulkan-tools xvfb xauth \
        ca-certificates curl git python3 procps make \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
        | tar -xz -C /opt && ln -s /opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/* /usr/local/bin/ \
    && curl -fsSL "https://go.dev/dl/go${GO_VERSION}.linux-amd64.tar.gz" | tar -xz -C /usr/local \
    && ln -s /usr/local/go/bin/go /usr/local/bin/go

ENV CC=gcc-14 CXX=g++-14
WORKDIR /work
