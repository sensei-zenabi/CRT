#!/usr/bin/env bash
set -euo pipefail

APT_GET=$(command -v apt-get || true)
if [[ -z "$APT_GET" ]]; then
  echo "apt-get not found. Please install dependencies manually." >&2
  exit 1
fi

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  SUDO="sudo"
else
  SUDO=""
fi

DEPS=(
  build-essential
  pkg-config
  libsdl2-dev
  libgl1-mesa-dev
  libx11-dev
  libxext-dev
)

set -x
$SUDO "$APT_GET" update
$SUDO "$APT_GET" install -y "${DEPS[@]}"
