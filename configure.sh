#!/bin/sh
set -eux;

apt-get update; apt-get upgrade -y
apt-get install -y --no-install-recommends \
  gpg \
  wget

wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null

apt-get update; apt-get upgrade -y

apt-get install -y --no-install-recommends \
  ca-certificates \
  build-essential \
  net-tools \
  cmake \
  gdb \
  git \
  vim \
  wget \
  curl \
  file \
  autotools-dev \
  automake \
  openssh-client \
  pkg-config \
  bison \
  ccache \
  dfu-util \
  flex \
  gperf \
  libffi-dev \
  libssl-dev \
  libusb-1.0-0-dev \
  python3 \
  python3-dev \
  python3-pip \
  python3-venv && \
  rm -rf /var/lib/apt/lists/*;

wget https://dl.espressif.com/github_assets/espressif/idf-im-ui/releases/download/v0.16.0/eim-cli-linux-x64.deb
dpkg -i eim-cli-linux-x64.deb
rm eim-cli-linux-x64.deb

eim install -i v5.4.2

echo "Finished Ubuntu configuration for ESP-IDF!"