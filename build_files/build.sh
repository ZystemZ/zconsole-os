#!/bin/bash

set -ouex pipefail

# Copy the contents of system_files/ of the git repo to /
# Ensure /ctx/system_files exists before copying
if [ -d "/ctx/system_files" ]; then
    cp -avf "/ctx/system_files"/. /
fi

### Language and Locale Configuration
echo "Configuring Portuguese Brazilian as default language..."
# Generate locales
echo "LANG=pt_BR.UTF-8" > /etc/locale.conf
echo "KEYMAP=br-abnt2" > /etc/vconsole.conf

### Install ZConsole OS Packages
echo "Installing ZConsole OS components..."

# Install emulators and tools
dnf5 install -y \
    retroarch \
    retroarch-assets-ozone \
    pcsx2 \
    dolphin-emu \
    duckstation \
    ppsspp \
    dosbox \
    scummvm \
    mame \
    python3-pyserial \
    htop \
    neofetch \
    tmux

# Enable necessary services
systemctl enable podman.socket

echo "ZConsole OS build completed!"
