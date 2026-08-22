#!/bin/bash
set -ouex pipefail

# Copy the contents of system_files/ of the git repo to /
if [ -d "/ctx/system_files" ]; then
    cp -avf "/ctx/system_files"/. /
fi

### Fix GPG key issues
echo "Fixing GPG key issues..."
if [ -d /etc/yum.repos.d/ ]; then
    sed -i 's/gpgcheck=1/gpgcheck=0/g' /etc/yum.repos.d/*.repo || true
fi

### DEEP REBRANDING (OS-RELEASE)
echo "Applying Deep Branding to os-release..."
for f in /etc/os-release /usr/lib/os-release; do
    if [ -f "$f" ]; then
        sed -i 's/^NAME=.*/NAME="ZConsole OS"/' "$f"
        sed -i 's/^PRETTY_NAME=.*/PRETTY_NAME="ZConsole OS 1.0 (Powered by Bazzite)"/' "$f"
        sed -i 's/^ID=.*/ID=zconsole/' "$f"
        sed -i 's/^ID_LIKE=.*/ID_LIKE="bazzite fedora"/' "$f"
        sed -i 's/^VARIANT=.*/VARIANT="Gaming Console"/' "$f"
        sed -i 's/^HOME_URL=.*/HOME_URL="https:\/\/github.com\/ZystemZ\/zconsole-os"/' "$f"
    fi
done

### GRUB REBRANDING
echo "Branding the GRUB Menu..."
if [ -f /etc/default/grub ]; then
    sed -i 's/^GRUB_DISTRIBUTOR=.*/GRUB_DISTRIBUTOR="ZConsole OS"/' /etc/default/grub
fi
# Remove Bazzite custom grub configs if they exist to avoid "Booting Bazzite"
rm -f /etc/grub.d/99-bazzite.cfg || true

### PLYMOUTH BRANDING (Aggressive)
echo "Forcing ZConsole Plymouth Theme..."
# Replace default bazzite theme files if they exist
BAZZITE_THEME_DIR="/usr/share/plymouth/themes/bazzite"
if [ -d "$BAZZITE_THEME_DIR" ]; then
    cp -f /usr/share/zconsole/boot_splash.png "$BAZZITE_THEME_DIR/bazzite.png" || true
    cp -f /usr/share/zconsole/boot_splash.png "$BAZZITE_THEME_DIR/watermark.png" || true
fi

# Also set our own theme
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

### ANACONDA INSTALLER BRANDING
echo "Branding the Anaconda Installer..."
mkdir -p /usr/share/anaconda/pixmaps
cp -f /usr/share/zconsole/logo.png /usr/share/anaconda/pixmaps/sidebar-logo.png
cp -f /usr/share/zconsole/logo.png /usr/share/anaconda/pixmaps/topbar-logo.png
if [ -f /etc/anaconda/anaconda.conf ]; then
    sed -i 's/productname = .*/productname = ZConsole OS/' /etc/anaconda/anaconda.conf
fi

### DESKTOP BRANDING
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

mkdir -p /etc/skel/.config
cat << 'EOF' > /etc/skel/.config/plasmarc
[Wallpaper][org.kde.image][General]
Image=file:///usr/share/wallpapers/zconsole/wallpaper.png
EOF

### EXECUTABLES AND SERVICES
echo "Configuring ZConsole Executables and Services..."
chmod +x /usr/bin/zgsdk /usr/bin/zconsole-cloud-sync /usr/bin/zgamestore /usr/bin/zgamestore-gui /usr/bin/zconsole-setup /usr/bin/zconsole-startup-animation.sh

# Disable Bazzite's startup animation and enable ours
systemctl disable bazzite-startup-animation.service || true
systemctl enable zconsole-startup.service

### PACKAGE INSTALLATION
echo "Installing Packages..."
# Bazzite uses dnf or rpm-ostree depending on context, we try dnf first
dnf install -y --skip-unavailable \
    retroarch retroarch-assets \
    python3-pyserial python3-tkinter \
    rclone htop fastfetch tmux vim wget curl flatpak zip unzip \
    plymouth-scripts mpv || true

# Set Plymouth Theme
plymouth-set-default-theme zconsole -R || true

echo "ZConsole OS Deep Branding Completed!"
