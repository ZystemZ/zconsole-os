#!/usr/bin/env python3
"""ZBuild Godot — Godot 4.x exporter and legacy-ZCARD packager.

The tool deliberately uses the ZIP-based .zcard container already accepted by
ZConsole OS. It does not convert Godot binaries into ZEXE.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable

VERSION = "0.1.0"
DEFAULT_PRESETS = {
    "windows": "Windows Desktop",
    "linux": "Linux/X11",
    "android": "Android",
}
TARGET_ALIASES = {
    "windows": ("windows",),
    "linux": ("linux",),
    "dual": ("windows", "linux"),
    "android": ("android",),
    "all": ("windows", "linux", "android"),
    "multiplatform": ("windows", "linux", "android"),
}


class BuildError(RuntimeError):
    pass


def parse_targets(value: str) -> list[str]:
    requested: list[str] = []
    for item in value.lower().split(","):
        item = item.strip()
        if not item:
            continue
        expanded = TARGET_ALIASES.get(item, (item,))
        for target in expanded:
            if target not in DEFAULT_PRESETS:
                raise BuildError(f"Destino desconhecido: {item}")
            if target not in requested:
                requested.append(target)
    if not requested:
        raise BuildError("Informe ao menos um destino.")
    return requested


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def is_within(path: Path, parent: Path) -> bool:
    try:
        path.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False


def find_godot(explicit: str | None) -> str:
    candidate = explicit or os.environ.get("GODOT_BIN")
    if candidate:
        resolved = shutil.which(candidate) or (candidate if Path(candidate).is_file() else None)
        if not resolved:
            raise BuildError(f"Executável Godot não encontrado: {candidate}")
        return str(resolved)
    names = ("godot", "godot4", "godot.exe", "Godot_v4.0-stable_win64.exe")
    for name in names:
        resolved = shutil.which(name)
        if resolved:
            return resolved
    raise BuildError("Godot não encontrado. Use --godot ou defina GODOT_BIN.")


def godot_version(binary: str) -> str:
    result = subprocess.run([binary, "--version"], capture_output=True, text=True, timeout=20)
    output = (result.stdout + result.stderr).strip()
    match = re.search(r"(\d+)\.(\d+)(?:\.([0-9]+))?", output)
    if result.returncode != 0 or not match:
        raise BuildError(f"Não foi possível identificar a versão do Godot: {output}")
    if int(match.group(1)) != 4:
        raise BuildError(f"ZBuild 0.1 requer Godot 4.x; encontrado: {match.group(0)}")
    return match.group(0)


def load_presets(project: Path) -> set[str]:
    config = project / "export_presets.cfg"
    if not config.is_file():
        raise BuildError("Projeto sem export_presets.cfg. Configure os presets de exportação no Godot.")
    text = config.read_text(encoding="utf-8", errors="replace")
    return set(re.findall(r'(?m)^name="([^"]+)"\s*$', text))


def run_command(command: list[str], cwd: Path, timeout: int = 3600) -> None:
    print("+", subprocess.list2cmdline(command) if os.name == "nt" else " ".join(command), flush=True)
    result = subprocess.run(command, cwd=cwd, timeout=timeout)
    if result.returncode:
        raise BuildError(f"Comando falhou ({result.returncode}): {command[0]}")


def copy_project(source: Path, destination: Path) -> None:
    ignored = {".git", ".godot", ".zbuild", "__pycache__"}
    def ignore(_directory: str, names: list[str]) -> set[str]:
        return {name for name in names if name in ignored}
    shutil.copytree(source, destination, ignore=ignore, symlinks=False)


def find_export_files(root: Path, suffixes: tuple[str, ...]) -> list[Path]:
    return sorted(p for p in root.rglob("*") if p.is_file() and p.suffix.lower() in suffixes)


def export_desktop(binary: str, project: Path, preset: str, target: str, out: Path) -> list[Path]:
    out.mkdir(parents=True, exist_ok=True)
    filename = "Game.exe" if target == "windows" else "Game.x86_64"
    executable = out / filename
    run_command([binary, "--headless", "--path", str(project), "--export-release", preset, str(executable)], project)
    if not executable.is_file():
        raise BuildError(f"Exportação {target} não gerou o executável esperado: {executable.name}")
    sidecars = sorted(p for p in out.iterdir() if p.is_file() and p != executable)
    return [executable, *sidecars]


def export_android(binary: str, project: Path, preset: str, out: Path) -> list[Path]:
    # Gradle mode produces a project that can generate both APK and AAB.
    gradle_project = out / "gradle-project"
    gradle_project.mkdir(parents=True, exist_ok=True)
    run_command([binary, "--headless", "--path", str(project), "--export-release", preset, str(gradle_project)], project)
    wrapper = gradle_project / ("gradlew.bat" if os.name == "nt" else "gradlew")
    if wrapper.is_file():
        if os.name != "nt":
            wrapper.chmod(wrapper.stat().st_mode | 0o111)
        run_command([str(wrapper), "assembleRelease", "bundleRelease"], gradle_project, timeout=7200)
        apk = find_export_files(gradle_project, (".apk",))
        aab = find_export_files(gradle_project, (".aab",))
        if not apk or not aab:
            raise BuildError("Gradle terminou, mas não produziu APK e AAB. Verifique o preset Android e as credenciais de assinatura.")
        results = []
        for source, label in ((apk[-1], "Game.apk"), (aab[-1], "Game.aab")):
            destination = out / label
            shutil.copy2(source, destination)
            results.append(destination)
        return results

    # Non-Gradle Godot Android export can create an APK, but cannot promise an AAB.
    apk_path = out / "Game.apk"
    run_command([binary, "--headless", "--path", str(project), "--export-release", preset, str(apk_path)], project)
    if not apk_path.is_file():
        raise BuildError("Exportação Android não gerou APK. AAB exige o modo Gradle no preset Android.")
    raise BuildError("APK gerado, mas AAB não foi produzido. Ative 'Gradle Build' no preset Android para gerar os dois formatos.")


def write_deterministic_zip(root: Path, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    try:
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_STORED, allowZip64=False) as archive:
            for path in sorted(p for p in root.rglob("*") if p.is_file()):
                relative = path.relative_to(root).as_posix()
                info = zipfile.ZipInfo(relative, date_time=(1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_STORED
                info.external_attr = (0o100644 & 0xFFFF) << 16
                archive.writestr(info, path.read_bytes())
        with zipfile.ZipFile(temporary, "r") as archive:
            bad = archive.testzip()
            if bad:
                raise BuildError(f"Verificação do ZCARD falhou no arquivo: {bad}")
            names = set(archive.namelist())
            if "META/zgame.json" not in names:
                raise BuildError("ZCARD inválido: META/zgame.json ausente.")
        temporary.replace(output)
    finally:
        if temporary.exists():
            temporary.unlink()


def build(args: argparse.Namespace) -> dict:
    source = Path(args.project).expanduser().resolve()
    output = Path(args.output).expanduser().resolve()
    if not (source / "project.godot").is_file():
        raise BuildError(f"Projeto Godot inválido (project.godot ausente): {source}")
    if is_within(output, source):
        raise BuildError("O arquivo .zcard deve ficar fora da pasta do projeto para preservar os fontes.")
    targets = parse_targets(args.targets)
    binary = find_godot(args.godot)
    version = godot_version(binary)
    presets = load_presets(source)
    selected = {target: args.__dict__.get(f"preset_{target}") or DEFAULT_PRESETS[target] for target in targets}
    for target, preset in selected.items():
        if preset not in presets:
            raise BuildError(f"Preset '{preset}' não encontrado em export_presets.cfg (destino {target}).")

    title = args.title or source.name
    with tempfile.TemporaryDirectory(prefix="zbuild-godot-") as temporary:
        work = Path(temporary)
        project_copy = work / "project"
        copy_project(source, project_copy)
        stage = work / "zcard"
        (stage / "META").mkdir(parents=True)
        artifacts: dict[str, list[str]] = {}
        for target in targets:
            destination = stage / "CODE" / "GODOT" / target.upper()
            if target == "android":
                files = export_android(binary, project_copy, selected[target], destination)
            else:
                files = export_desktop(binary, project_copy, selected[target], target, destination)
            artifacts[target] = [p.relative_to(stage).as_posix() for p in files]

        primary = next((p for p in artifacts.get("windows", []) if p.lower().endswith(".exe")), None)
        if not primary:
            primary = next((p for p in artifacts.get("linux", []) if p.lower().endswith(".x86_64")), None)
        if not primary:
            primary = next((p for p in artifacts.get("android", []) if p.lower().endswith(".apk")), artifacts[targets[0]][0])

        manifest = {
            "format": "zgame-native-0.1",
            "title": title,
            "platform": targets[0],
            "engine": {"name": "Godot", "version": version},
            "executable": primary,
            "executables": {key: value for key, value in artifacts.items()},
            "entrypoint": 0,
            "boot_scene": args.boot_scene or "",
            "memory_required": 0,
            "package": {"format": "ZCARD-ZIP", "version": "0.1", "builder": f"ZBuild-Godot/{VERSION}"},
        }
        (stage / "META" / "zgame.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        file_hashes = {}
        for path in sorted(p for p in stage.rglob("*") if p.is_file()):
            file_hashes[path.relative_to(stage).as_posix()] = sha256_file(path)
        (stage / "META" / "SHA256SUMS.json").write_text(json.dumps(file_hashes, indent=2) + "\n", encoding="utf-8")
        write_deterministic_zip(stage, output)

    return {
        "status": "success", "builder": VERSION, "godot": version,
        "project": str(source), "output": str(output), "size_bytes": output.stat().st_size,
        "sha256": sha256_file(output), "targets": targets, "artifacts": artifacts,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="ZBuild Godot — exporta jogos Godot 4.x e empacota em .zcard")
    parser.add_argument("--project", required=True, help="Pasta do projeto contendo project.godot")
    parser.add_argument("--output", required=True, help="Arquivo .zcard de saída (fora do projeto)")
    parser.add_argument("--targets", default="dual", help="windows, linux, dual, android ou all")
    parser.add_argument("--godot", help="Caminho do executável Godot 4.x (ou GODOT_BIN)")
    parser.add_argument("--title", help="Título do jogo (padrão: nome da pasta)")
    parser.add_argument("--boot-scene", default="", help="Cena inicial, apenas como metadado")
    parser.add_argument("--preset-windows", dest="preset_windows")
    parser.add_argument("--preset-linux", dest="preset_linux")
    parser.add_argument("--preset-android", dest="preset_android")
    parser.add_argument("--version", action="version", version=f"ZBuild Godot {VERSION}")
    args = parser.parse_args()
    try:
        report = build(args)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        report_path = Path(args.output).expanduser().resolve().with_suffix(".build.json")
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        return 0
    except (BuildError, OSError, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        print(f"[ZBuild] ERRO: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
