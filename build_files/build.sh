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

# Set permissions for visual assets
chmod 644 /usr/share/wallpapers/zconsole/wallpaper.png
chmod 644 /usr/share/icons/zconsole/icons.png
chmod 644 /usr/share/zconsole/*.png
chmod 644 /etc/os-release

# Symbolic link for the logo to be used by the system
ln -sf /usr/share/zconsole/logo.png /usr/share/pixmaps/zconsole-logo.png

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
    htop \
    fastfetch \
    tmux \
    vim \
    wget \
    curl

# Enable necessary services
systemctl enable podman.socket

echo "ZConsole OS build completed successfully! Powered by Bazzite."
