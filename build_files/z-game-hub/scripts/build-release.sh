#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build/release}"
DIST_DIR="${DIST_DIR:-$ROOT/dist}"
STAGE_DIR="$BUILD_DIR/stage"
VERSION="1.0.0"

for tool in cmake ninja c++; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Erro: ferramenta obrigatória não encontrada: $tool" >&2
        echo "Instale CMake 3.21+, Ninja e um compilador C++20 no Toolbox/Distrobox." >&2
        exit 2
    fi
done

mkdir -p "$BUILD_DIR" "$DIST_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD_DIR" --parallel

BINARY="$BUILD_DIR/z-game-hub"
if [[ ! -x "$BINARY" ]]; then
    echo "Erro: build terminou sem gerar o executável esperado: $BINARY" >&2
    exit 3
fi

rm -rf "$STAGE_DIR"
cmake --install "$BUILD_DIR" --prefix "$STAGE_DIR/usr"
install -Dm644 "$ROOT/README.md" "$STAGE_DIR/usr/share/doc/z-game-hub/README.md"

ARCH="$(uname -m)"
PACKAGE="$DIST_DIR/z-game-hub-${VERSION}-linux-${ARCH}.tar.gz"
tar -C "$STAGE_DIR" -czf "$PACKAGE" .
echo "Executável: $BINARY"
echo "Pacote de instalação: $PACKAGE"
echo "Este pacote contém o binário e o arquivo .desktop; Qt 6.5+ e o driver Qt SQLite devem estar disponíveis no sistema de destino."
