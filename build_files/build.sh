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

### EXTERMINATE BAZZITE BRANDING (Aggressive Search & Replace)
echo "Exterminating Bazzite branding from system files..."
# Patch os-release
for f in /etc/os-release /usr/lib/os-release; do
    if [ -f "$f" ]; then
        sed -i 's/Bazzite/ZConsole OS/g' "$f"
        sed -i 's/bazzite/zconsole/g' "$f"
        sed -i 's/^NAME=.*/NAME="ZConsole OS"/' "$f"
        sed -i 's/^PRETTY_NAME=.*/PRETTY_NAME="ZConsole OS 1.0"/' "$f"
        # Keep ZConsole branding while retaining Fedora's supported distro definition.
        sed -i 's/^ID=.*/ID=fedora/' "$f"
        if grep -q '^ID_LIKE=' "$f"; then
            sed -i 's/^ID_LIKE=.*/ID_LIKE=fedora/' "$f"
        else
            printf '%s\n' 'ID_LIKE=fedora' >> "$f"
        fi
    fi
done

# Patch GRUB Distributor
if [ -f /etc/default/grub ]; then
    sed -i 's/GRUB_DISTRIBUTOR=.*/GRUB_DISTRIBUTOR="ZConsole OS"/' /etc/default/grub
fi

# Patch GRUB templates to remove "Booting Bazzite"
if [ -d /etc/grub.d ]; then
    grep -lR "Bazzite" /etc/grub.d/ | xargs sed -i 's/Bazzite/ZConsole OS/g' || true
fi

### PLYMOUTH BRANDING (Nuclear Option)
echo "Nuking Bazzite Plymouth and forcing ZConsole..."
BAZZITE_THEME_DIR="/usr/share/plymouth/themes/bazzite"
ZCONSOLE_THEME_DIR="/usr/share/plymouth/themes/zconsole"

mkdir -p "$ZCONSOLE_THEME_DIR"
cp /usr/share/zconsole/boot_splash.png "$ZCONSOLE_THEME_DIR/zconsole.png"
cat << 'EOF' > "$ZCONSOLE_THEME_DIR/zconsole.plymouth"
[Plymouth Theme]
Name=ZConsole OS
Description=ZConsole OS Boot Splash
ModuleName=script

[script]
ImageDir=/usr/share/plymouth/themes/zconsole
ScriptFile=/usr/share/plymouth/themes/zconsole/zconsole.script
EOF

cat << 'EOF' > "$ZCONSOLE_THEME_DIR/zconsole.script"
logo_image = Image("zconsole.png");
logo_sprite = Sprite(logo_image);
logo_sprite.SetX(Window.GetWidth() / 2 - logo_image.GetWidth() / 2);
logo_sprite.SetY(Window.GetHeight() / 2 - logo_image.GetHeight() / 2);
EOF

# If bazzite theme exists, replace its assets too as a backup
if [ -d "$BAZZITE_THEME_DIR" ]; then
    find "$BAZZITE_THEME_DIR" -name "*.png" -exec cp -f "$ZCONSOLE_THEME_DIR/zconsole.png" {} \; || true
fi

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
chmod +x /usr/bin/zgsdk /usr/bin/zconsole-cloud-sync /usr/bin/zgamestore /usr/bin/zgamestore-gui /usr/bin/zgamestore-gui-legacy /usr/bin/zconsole-setup /usr/bin/zconsole-startup-animation.sh /usr/lib/zconsole/zconsole_local_api.py

# Ensure the startup service is robust
cat << 'EOF' > /usr/lib/systemd/system/zconsole-startup.service
[Unit]
Description=ZConsole OS Startup Animation
After=plymouth-quit-wait.service
Before=display-manager.service
DefaultDependencies=no

[Service]
Type=oneshot
ExecStart=/usr/bin/zconsole-startup-animation.sh
StandardOutput=null
StandardError=null
TimeoutStartSec=15
RemainAfterExit=no

[Install]
WantedBy=multi-user.target
EOF

# Ensure a graphical login target exists after installation. Never assume one display manager.
systemctl set-default graphical.target || true
for display_manager in sddm gdm; do
    if [ -f "/usr/lib/systemd/system/${display_manager}.service" ]; then
        systemctl enable "${display_manager}.service" || true
        break
    fi
done

# Disable Bazzite's startup animation and enable ours
systemctl disable bazzite-startup-animation.service || true
systemctl enable zconsole-startup.service

# Local API for Z-Overlay telemetry and controls. It is loopback-only and fail-safe.
systemctl enable zconsole-local-api.service || true

# Start the Big Picture UI only after a graphical user session exists.
mkdir -p /etc/xdg/autostart
cat << 'EOF' > /etc/xdg/autostart/zconsole-gamestore.desktop
[Desktop Entry]
Type=Application
Name=Z-GameStore
Comment=Interface Big Picture do ZConsole OS
Exec=/usr/bin/zgamestore-gui
Terminal=false
X-GNOME-Autostart-enabled=true
OnlyShowIn=GNOME;KDE;XFCE;
EOF

### PACKAGE INSTALLATION
echo "Installing Packages..."
dnf install -y --skip-unavailable \
    retroarch retroarch-assets \
    python3-pyserial python3-tkinter \
    rclone htop fastfetch tmux vim wget curl flatpak zip unzip \
    plymouth-scripts mpv python3-fastapi python3-uvicorn brightnessctl wireplumber power-profiles-daemon || true

# Set Plymouth Theme
plymouth-set-default-theme zconsole || true

echo "ZConsole OS Identity Patch Completed!"
