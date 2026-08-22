#!/usr/bin/env python3
"""Ponte local do ZConsole OS.

Escuta apenas em loopback e expõe comandos não sensíveis para a UI local:
- GET  /api/status
- POST /api/brightness
- POST /api/volume
- POST /api/power-profile
- POST /api/oscr/{action}

Qualquer comando opcional falha de forma segura e nunca bloqueia a sessão gráfica.
"""
from __future__ import annotations

import glob
import json
import os
import re
import subprocess
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field

APP_ROOT = Path("/usr/share/zconsole/gamestore") if Path("/usr/share/zconsole/gamestore").exists() else Path("/home/ubuntu/zconsole-os-fresh/system_files/usr/share/zconsole/gamestore")
app = FastAPI(title="ZConsole Local API", version="2.0.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://127.0.0.1:48521",
        "http://localhost:48521",
        "http://127.0.0.1:3000",
        "http://localhost:3000",
    ],
    allow_methods=["GET", "POST", "OPTIONS"],
    allow_headers=["Content-Type"],
)


class LevelRequest(BaseModel):
    value: int = Field(ge=0, le=100)


class PowerProfileRequest(BaseModel):
    profile: str = Field(pattern="^(Silencioso|Equilibrado|Desempenho)$")


def run_optional(command: list[str], timeout: float = 2.0) -> tuple[bool, str]:
    """Run a known command without shell expansion; errors are intentionally non-fatal."""
    try:
        result = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        return result.returncode == 0, (result.stdout or result.stderr).strip()
    except (FileNotFoundError, subprocess.SubprocessError, OSError):
        return False, ""


def read_text(path: str) -> str | None:
    try:
        return Path(path).read_text(encoding="utf-8").strip()
    except (OSError, UnicodeDecodeError):
        return None


def read_temperature() -> str | None:
    raw = read_text("/sys/class/thermal/thermal_zone0/temp")
    if raw and raw.isdigit():
        return f"{round(int(raw) / 1000)}°C"
    return None


def read_memory() -> str | None:
    raw = read_text("/proc/meminfo")
    if not raw:
        return None
    values: dict[str, int] = {}
    for line in raw.splitlines():
        match = re.match(r"^(MemTotal|MemAvailable):\s+(\d+)", line)
        if match:
            values[match.group(1)] = int(match.group(2))
    total, available = values.get("MemTotal"), values.get("MemAvailable")
    if not total or available is None:
        return None
    return f"{round((1 - available / total) * 100)}%"


def read_battery() -> str | None:
    for path in glob.glob("/sys/class/power_supply/BAT*/capacity"):
        value = read_text(path)
        if value and value.isdigit():
            return f"{value}%"
    return None


def oscr_state() -> dict[str, str]:
    devices = glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*")
    if devices:
        return {"status": "ready_to_read", "cartridge": "Leitor detectado"}
    return {"status": "waiting", "cartridge": "Nenhum leitor/cartucho detectado"}


def network_status() -> str:
    """Report connectivity only when an interface is up and has a default route."""
    for path in glob.glob("/sys/class/net/*/operstate"):
        interface = Path(path).parent.name
        if interface == "lo":
            continue
        state = read_text(path)
        if state in {"up", "unknown"}:
            ok, routes = run_optional(["ip", "route", "show", "default"])
            if ok and routes:
                return "Conectado"
    return "Indisponível"


def current_audio() -> int | None:
    ok, output = run_optional(["wpctl", "get-volume", "@DEFAULT_AUDIO_SINK@"])
    match = re.search(r"\b0?\.([0-9]+)", output) if ok else None
    if match:
        try:
            return min(100, round(float(f"0.{match.group(1)}") * 100))
        except ValueError:
            return None
    return None


def current_power_profile() -> str:
    ok, output = run_optional(["powerprofilesctl", "get"])
    profile_names = {"power-saver": "Silencioso", "balanced": "Equilibrado", "performance": "Desempenho"}
    if ok and output:
        return profile_names.get(output.strip(), output.strip())
    return os.environ.get("ZCONSOLE_POWER_PROFILE", "Equilibrado")


def read_brightness() -> int | None:
    """Read the active backlight level without assuming a specific laptop vendor."""
    ok, output = run_optional(["brightnessctl", "get"])
    ok_max, maximum = run_optional(["brightnessctl", "max"])
    if ok and ok_max:
        try:
            current, max_value = int(output), int(maximum)
            if max_value > 0:
                return max(0, min(100, round(current / max_value * 100)))
        except ValueError:
            pass

    for current_path in glob.glob("/sys/class/backlight/*/brightness"):
        max_path = str(Path(current_path).with_name("max_brightness"))
        current_raw, max_raw = read_text(current_path), read_text(max_path)
        try:
            if current_raw and max_raw and int(max_raw) > 0:
                return max(0, min(100, round(int(current_raw) / int(max_raw) * 100)))
        except ValueError:
            continue
    return None


@app.get("/api/status")
def status() -> dict[str, Any]:
    current_brightness = read_brightness()
    current_volume = current_audio()
    return {
        "ok": True,
        "source": "local-linux",
        "cloudSync": {"status": "synchronized", "lastSync": "gerenciado pelo zconsole-cloud-sync"},
        "oscr": oscr_state(),
        "powerProfile": current_power_profile(),
        "volume": current_volume,
        "brightness": current_brightness,
        "brightnessAvailable": current_brightness is not None,
        "network": network_status(),
        "telemetry": {
            "cpuTemp": read_temperature() or "Indisponível",
            "ramUsage": read_memory() or "Indisponível",
            "battery": read_battery() or "AC conectado",
        },
    }


@app.post("/api/brightness")
def brightness(payload: LevelRequest) -> dict[str, Any]:
    applied, output = run_optional(["brightnessctl", "set", f"{payload.value}%"])
    return {"ok": applied, "value": payload.value, "applied": applied, "detail": output or "comando indisponível; valor mantido na interface"}


@app.post("/api/volume")
def volume(payload: LevelRequest) -> dict[str, Any]:
    applied, output = run_optional(["wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", f"{payload.value}%"])
    return {"ok": applied, "value": payload.value, "applied": applied, "detail": output or "comando indisponível; valor mantido na interface"}


@app.post("/api/power-profile")
def power_profile(payload: PowerProfileRequest) -> dict[str, Any]:
    profile_names = {"Silencioso": "power-saver", "Equilibrado": "balanced", "Desempenho": "performance"}
    os.environ["ZCONSOLE_POWER_PROFILE"] = payload.profile
    applied, detail = run_optional(["powerprofilesctl", "set", profile_names[payload.profile]])
    return {"ok": applied, "profile": payload.profile, "applied": applied, "detail": detail or "power-profiles-daemon indisponível; perfil mantido na sessão"}


@app.post("/api/oscr/{action}")
def oscr_action(action: str) -> dict[str, Any]:
    if action not in {"status", "eject", "read", "write-save"}:
        raise HTTPException(status_code=404, detail="Ação OSCR não reconhecida")
    return {"ok": True, "action": action, "status": oscr_state(), "message": "Ação preparada sem tocar em dados do cartucho"}


if APP_ROOT.exists():
    app.mount("/", StaticFiles(directory=APP_ROOT, html=True), name="gamestore")


if __name__ == "__main__":
    import uvicorn

    uvicorn.run(app, host="127.0.0.1", port=48521, log_level="warning")
