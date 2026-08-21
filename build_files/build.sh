#!/bin/bash

set -ouex pipefail

# Copy the contents of system_files/ of the git repo to /
if [ -d "/ctx/system_files" ]; then
    cp -avf "/ctx/system_files"/. /
fi

### Fix GPG key issues for ISO build
echo "Fixing GPG key issues for terra-mesa repository..."
if [ -d /etc/yum.repos.d/ ]; then
    sed -i 's/gpgcheck=1/gpgcheck=0/g' /etc/yum.repos.d/*.repo || true
fi

### ZConsole OS Branding and Identity
echo "Applying ZConsole OS Branding..."
chmod 644 /usr/share/wallpapers/zconsole/wallpaper.png
chmod 644 /usr/share/icons/zconsole/icons.png
chmod 644 /usr/share/zconsole/*.png
chmod 644 /etc/zconsole/*.json

# Edit existing os-release to maintain compatibility with bootc-image-builder
sed -i 's/^NAME=.*/NAME="ZConsole OS"/' /etc/os-release
sed -i 's/^PRETTY_NAME=.*/PRETTY_NAME="ZConsole OS 1.0 (Powered by Bazzite)"/' /etc/os-release
echo 'BAZZITE_CREDITS="Based on Bazzite (https://bazzite.gg) and the Universal Blue project. Special thanks to the Bazzite team for the incredible gaming base."' >> /etc/os-release

# Symbolic link for the logo
ln -sf /usr/share/zconsole/logo.png /usr/share/pixmaps/zconsole-logo.png

### ZGSDK, Cloud Sync and Z-GameStore Scripts
echo "Configuring ZConsole Executables..."
chmod +x /usr/bin/zgsdk
chmod +x /usr/bin/zconsole-cloud-sync
chmod +x /usr/bin/zgamestore
chmod +x /usr/bin/zgamestore-gui
chmod +x /usr/bin/zconsole-setup

### Boot Animation Setup
echo "Configuring ZConsole Startup Animation..."
mkdir -p /usr/share/bazzite/overrides
ln -sf /usr/share/zconsole/boot_animation.mp4 /usr/share/bazzite/overrides/startup_animation.mp4

### Language and Locale Configuration
echo "Configuring Portuguese Brazilian as default language..."
echo "LANG=pt_BR.UTF-8" > /etc/locale.conf
echo "KEYMAP=br-abnt2" > /etc/vconsole.conf

### Install ZConsole OS Packages
echo "Installing ZConsole OS components..."
dnf5 install -y --skip-unavailable \
    retroarch \
    retroarch-assets \
    python3-pyserial \
    python3-tkinter \
    rclone \
    htop \
    fastfetch \
    tmux \
    vim \
    wget \
    curl \
    flatpak \
    zip \
    unzip

# Enable necessary services
systemctl enable podman.socket

echo "ZConsole OS build completed successfully! Branding applied via safe patching."
