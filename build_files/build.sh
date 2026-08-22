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

# Plymouth Branding (Boot Splash)
echo "Configuring Plymouth theme..."
mkdir -p /usr/share/plymouth/themes/zconsole
cp /usr/share/zconsole/boot_splash.png /usr/share/plymouth/themes/zconsole/zconsole.png
cat << 'EOF' > /usr/share/plymouth/themes/zconsole/zconsole.plymouth
[Plymouth Theme]
Name=ZConsole OS
Description=ZConsole OS Boot Splash
ModuleName=script

[script]
ImageDir=/usr/share/plymouth/themes/zconsole
ScriptFile=/usr/share/plymouth/themes/zconsole/zconsole.script
EOF

cat << 'EOF' > /usr/share/plymouth/themes/zconsole/zconsole.script
logo_image = Image("zconsole.png");
logo_sprite = Sprite(logo_image);
logo_sprite.SetX(Window.GetWidth() / 2 - logo_image.GetWidth() / 2);
logo_sprite.SetY(Window.GetHeight() / 2 - logo_image.GetHeight() / 2);
EOF

# Desktop Branding (GSettings Overrides)
echo "Applying Desktop Environment Overrides..."
mkdir -p /etc/dconf/db/local.d
cat << 'EOF' > /etc/dconf/db/local.d/00-zconsole-branding
[org/gnome/desktop/background]
picture-uri='file:///usr/share/wallpapers/zconsole/wallpaper.png'
picture-uri-dark='file:///usr/share/wallpapers/zconsole/wallpaper.png'
picture-options='zoom'

[org/gnome/desktop/interface]
icon-theme='zconsole'
EOF
dconf update || true

# KDE Plasma Overrides
mkdir -p /etc/skel/.config
cat << 'EOF' > /etc/skel/.config/plasmarc
[Wallpaper][org.kde.image][General]
Image=file:///usr/share/wallpapers/zconsole/wallpaper.png
EOF

# Edit existing os-release to maintain compatibility with bootc-image-builder
sed -i 's/^NAME=.*/NAME="ZConsole OS"/' /etc/os-release
sed -i 's/^PRETTY_NAME=.*/PRETTY_NAME="ZConsole OS 1.0 (Powered by Bazzite)"/' /etc/os-release
echo 'BAZZITE_CREDITS="Based on Bazzite (https://bazzite.gg) and the Universal Blue project. Special thanks to the Bazzite team for the incredible gaming base."' >> /etc/os-release

# Symbolic link for the logo
ln -sf /usr/share/zconsole/logo.png /usr/share/pixmaps/zconsole-logo.png

# Anaconda Installer Branding
echo "Branding the Anaconda Installer..."
mkdir -p /usr/share/anaconda/pixmaps
cp /usr/share/zconsole/logo.png /usr/share/anaconda/pixmaps/sidebar-logo.png
cp /usr/share/zconsole/logo.png /usr/share/anaconda/pixmaps/topbar-logo.png
# Patch Anaconda configuration if it exists
if [ -f /etc/anaconda/anaconda.conf ]; then
    sed -i 's/productname = .*/productname = ZConsole OS/' /etc/anaconda/anaconda.conf
fi

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
    unzip \
    plymouth-scripts

# Set Plymouth Theme
plymouth-set-default-theme zconsole -R || true

# Enable necessary services
systemctl enable podman.socket

echo "ZConsole OS build completed successfully! Branding applied via safe patching."
