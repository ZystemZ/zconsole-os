#!/bin/bash
# ZConsole OS - Robust Startup Animation Script
# Plays the boot video with a fail-safe timeout.

VIDEO_PATH="/usr/share/zconsole/boot_animation.mp4"
TIMEOUT=15

if [ -f "$VIDEO_PATH" ]; then
    echo "Playing ZConsole startup animation..."
    # Use mpv if available, otherwise try ffplay
    if command -v mpv &> /dev/null; then
        timeout $TIMEOUT mpv --fs --no-osc --no-osd-bar --no-input-default-bindings --input-vo-keyboard=no --really-quiet "$VIDEO_PATH" || true
    elif command -v ffplay &> /dev/null; then
        timeout $TIMEOUT ffplay -fs -autoexit -nodisp -loglevel quiet "$VIDEO_PATH" || true
    fi
fi

echo "Startup animation finished or timed out. Continuing boot..."
exit 0
