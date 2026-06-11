#!/usr/bin/env bash
set -euo pipefail

if ! command -v apt-get >/dev/null 2>&1; then
    echo "This setup script requires an apt-based Ubuntu environment." >&2
    exit 1
fi

sudo apt-get update
sudo apt-get install -y \
    build-essential \
    ca-certificates \
    cmake \
    curl \
    file \
    git \
    libgdal-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    liblaszip-dev \
    libopenscenegraph-dev \
    libproj-dev \
    libqt5opengl5-dev \
    libqt5waylandclient5 \
    libqt5waylandcompositor5 \
    libxkbcommon-x11-0 \
    libxcb-cursor0 \
    libxcb-xinerama0 \
    locales \
    mesa-utils \
    ninja-build \
    openscenegraph \
    pkg-config \
    proj-bin \
    qtbase5-dev \
    qtbase5-dev-tools \
    qttools5-dev-tools \
    qtwayland5 \
    unzip \
    xz-utils \
    zip

echo "Ubuntu 22.04 build dependencies are installed."
