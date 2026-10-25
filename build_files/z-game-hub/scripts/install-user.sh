#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
if [[ ! -x "$BUILD/z-game-hub" ]]; then
  "$ROOT/scripts/build.sh"
fi
DEST="$HOME/.local"
mkdir -p "$DEST/bin" "$DEST/share/applications"
install -m 0755 "$BUILD/z-game-hub" "$DEST/bin/z-game-hub"
install -m 0644 "$ROOT/packaging/z-game-hub.desktop" "$DEST/share/applications/z-game-hub.desktop"
echo "Instalado em ~/.local/bin/z-game-hub"
echo "Nota: recursos Qt podem precisar de instalação/empacotamento adicional conforme o ambiente."
