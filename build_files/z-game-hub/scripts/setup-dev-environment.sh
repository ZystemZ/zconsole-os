#!/usr/bin/env bash
set -euo pipefail

CONTAINER="${ZGAMEHUB_CONTAINER:-z-game-hub-qt6}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-auto}"
ACTION="${2:-build}"
IMAGE="${ZGAMEHUB_IMAGE:-registry.fedoraproject.org/fedora:41}"

usage() {
    cat <<EOF
Uso: $0 [auto|toolbox|distrobox] [build|shell]

Cria um ambiente isolado com Qt 6.5+, CMake e Ninja sem alterar a imagem base.
Variáveis opcionais: ZGAMEHUB_CONTAINER e ZGAMEHUB_IMAGE.
EOF
}

if [[ "$MODE" == "--help" || "$MODE" == "-h" ]]; then
    usage
    exit 0
fi

if [[ "$MODE" == "auto" ]]; then
    if command -v toolbox >/dev/null 2>&1; then
        MODE="toolbox"
    elif command -v distrobox >/dev/null 2>&1; then
        MODE="distrobox"
    else
        echo "Erro: instale Toolbox ou Distrobox no Bazzite antes de continuar." >&2
        exit 1
    fi
fi

case "$MODE" in
    toolbox)
        command -v toolbox >/dev/null 2>&1 || { echo "Toolbox não encontrado." >&2; exit 1; }
        if ! toolbox list --containers 2>/dev/null | awk '{print $1}' | grep -qx "$CONTAINER"; then
            toolbox create --container "$CONTAINER" --image "$IMAGE"
        fi
        RUNNER=(toolbox run --container "$CONTAINER")
        ENTER=(toolbox enter --container "$CONTAINER")
        ;;
    distrobox)
        command -v distrobox >/dev/null 2>&1 || { echo "Distrobox não encontrado." >&2; exit 1; }
        if ! distrobox list --no-color 2>/dev/null | awk -F'|' '{gsub(/[[:space:]]/, "", $2); print $2}' | grep -qx "$CONTAINER"; then
            distrobox create --name "$CONTAINER" --image "$IMAGE" --yes
        fi
        RUNNER=(distrobox enter --name "$CONTAINER" --)
        ENTER=(distrobox enter --name "$CONTAINER")
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

INSTALL='sudo dnf install -y cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtquickcontrols2-devel qt6-qttools-devel qt6-qtwayland-devel sqlite sqlite-devel git pkgconf-pkg-config'
"${RUNNER[@]}" bash -lc "$INSTALL"

if [[ "$ACTION" == "shell" ]]; then
    exec "${ENTER[@]}"
fi

"${RUNNER[@]}" bash -lc "cd '$PROJECT_DIR' && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel"
echo "Build concluído dentro de $MODE/$CONTAINER: $PROJECT_DIR/build"
