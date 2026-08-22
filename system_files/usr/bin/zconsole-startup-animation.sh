#!/bin/bash
# ZConsole OS - Ultra-Robust Startup Animation Script
# Plays the boot video with multiple fail-safes.

VIDEO_PATH="/usr/share/zconsole/boot_animation.mp4"
TIMEOUT=10 # Reduced timeout to 10s for better user experience if it fails

# Check if we are in a virtual machine (sometimes mpv fails in basic VMs)
IS_VM=$(hostnamectl | grep -i "Chassis: vm" || echo "")

if [ -f "$VIDEO_PATH" ] && [ -z "$IS_VM" ]; then
    echo "Playing ZConsole startup animation..."
    if command -v mpv &> /dev/null; then
        # Added --vo=null if it fails, but we want visual output. 
        # Added --no-config to avoid user config interference.
        timeout $TIMEOUT mpv --fs --no-osc --no-osd-bar --no-input-default-bindings --input-vo-keyboard=no --really-quiet --no-config "$VIDEO_PATH" || true
    fi
else
    echo "Skipping animation (VM detected or file missing)."
fi

exit 0
