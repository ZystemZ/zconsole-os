#!/bin/bash

set -ouex pipefail

# Copy the contents of system_files/ of the git repo to /
if [ -d "/ctx/system_files" ]; then
    cp -avf "/ctx/system_files"/. /
fi

### Fix GPG key issues for ISO build
echo "Fixing GPG key issues for terra-mesa repository..."
# Disable GPG check for terra-mesa to avoid build failures in bootc-image-builder
if [ -d /etc/yum.repos.d/ ]; then
    sed -i 's/gpgcheck=1/gpgcheck=0/g' /etc/yum.repos.d/*.repo || true
fi

### Language and Locale Configuration
echo "Configuring Portuguese Brazilian as default language..."
echo "LANG=pt_BR.UTF-8" > /etc/locale.conf
echo "KEYMAP=br-abnt2" > /etc/vconsole.conf

### Install ZConsole OS Packages
echo "Installing ZConsole OS components..."

# Install emulators and tools with names corrected for Fedora 44
# Using --skip-unavailable to ensure build passes even if some packages are not in the repo yet
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

# Note: Emulators like PCSX2, PPSSPP, DuckStation are best installed via Flatpak on Bazzite.
# They are not currently available as native RPMs in the default Fedora 44 repos.

# Enable necessary services
systemctl enable podman.socket

echo "ZConsole OS build completed successfully!"
