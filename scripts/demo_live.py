#!/usr/bin/env python3
"""Demo interactiva en primer plano de Texcel usando tmux a velocidad humana.

Lanza `./main --texcel` dentro de una sesión de tmux, se conecta en primer
plano (`tmux attach-session`) y reproduce un guion paso a paso simulando
escritura humana para mostrar todas las capacidades de Texcel en vivo.

Ejemplos de uso:
  python3 scripts/demo_live.py
  python3 scripts/demo_live.py --speed 0.75   # Más lento para explicar cada paso
  python3 scripts/demo_live.py --auto-exit    # Cierra automáticamente al terminar
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import threading
import time
import uuid
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
SHEETS_DIR = ROOT_DIR / "output" / "sheets"
CONFIG_PATH = SHEETS_DIR / ".config"
DEMO_SHEET_NAME = "_demo_live"
DEMO_SHEET_PATH = SHEETS_DIR / f"{DEMO_SHEET_NAME}.txl"


def run_tmux(args: list[str], check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["tmux", *args],
        text=True,
        capture_output=True,
        check=check,
    )


class TmuxDriver:
    """Controlador de teclado a velocidad humana sobre un pane de tmux."""

    def __init__(self, target: str, speed: float = 1.0, stop_event: threading.Event | None = None):
        self.target = target
        self.scale = 1.0 / max(0.1, speed)
        self.stop_event = stop_event or threading.Event()

    def wait(self, seconds: float) -> None:
        if self.stop_event.is_set():
            return
        self.stop_event.wait(seconds * self.scale)

    def send_key(self, key: str, pause_after: float = 0.25) -> None:
        if self.stop_event.is_set():
            return
        run_tmux(["send-keys", "-t", self.target, key], check=False)
        self.wait(pause_after)

    def send_keys(self, *keys: str, delay: float = 0.16) -> None:
        for k in keys:
            self.send_key(k, pause_after=delay)

    def type_text(self, text: str, char_delay: float = 0.065, pause_after: float = 0.35) -> None:
        """Escribe carácter por carácter a ritmo humano."""
        for ch in text:
            if self.stop_event.is_set():
                return
            run_tmux(["send-keys", "-t", self.target, "-l", ch], check=False)
            self.wait(char_delay)
        self.wait(pause_after)

    def goto_cell(self, cell_id: str, pause_after: float = 0.5) -> None:
        self.send_key("C-g", pause_after=0.35)
        self.type_text(cell_id, char_delay=0.07, pause_after=0.25)
        self.send_key("Enter", pause_after=pause_after)


def run_demo_script(driver: TmuxDriver, auto_exit: bool) -> None:
    """Guion completo de demostración de funcionalidades de Texcel."""
    # Esperar a que Texcel termine el render inicial
    driver.wait(1.2)

    # ------------------------------------------------------------------
    # 1. Crear hoja limpia de demo desde el Gestor de Hojas (Ctrl+O)
    # ------------------------------------------------------------------
    driver.send_key("C-o", pause_after=0.9)       # Abre ModalKind::Sheets
    driver.send_key("Enter", pause_after=0.5)     # Selecciona "+ Nueva hoja"
    driver.type_text(DEMO_SHEET_NAME, char_delay=0.06, pause_after=0.3)
    driver.send_key("Enter", pause_after=0.9)

    # ------------------------------------------------------------------
    # 2. Ingreso directo en celdas: encabezados y datos
    # ------------------------------------------------------------------
    for col_val in ("Producto", "Precio", "Cant", "Total"):
        driver.type_text(col_val, char_delay=0.055, pause_after=0.2)
        driver.send_key("Enter", pause_after=0.2)
        driver.send_key("Right", pause_after=0.18)

    # Fila 2: Laptop | 1200 | 2
    driver.goto_cell("A2", pause_after=0.4)
    driver.type_text("Laptop", pause_after=0.15)
    driver.send_key("Enter", pause_after=0.2)
    driver.send_key("Right", pause_after=0.15)
    driver.type_text("1200", pause_after=0.15)
    driver.send_key("Enter", pause_after=0.2)
    driver.send_key("Right", pause_after=0.15)
    driver.type_text("2", pause_after=0.15)
    driver.send_key("Enter", pause_after=0.2)

    # Fila 3: Mouse | 25 | 4
    driver.goto_cell("A3", pause_after=0.35)
    driver.type_text("Mouse", pause_after=0.15)
    driver.send_key("Enter", pause_after=0.2)
    driver.send_key("Right", pause_after=0.15)
    driver.type_text("25", pause_after=0.15)
    driver.send_key("Enter", pause_after=0.2)
    driver.send_key("Right", pause_after=0.15)
    driver.type_text("4", pause_after=0.15)
    driver.send_key("Enter", pause_after=0.2)

    # Fila 4: Teclado | 80 | 3
    driver.goto_cell("A4", pause_after=0.35)
    driver.type_text("Teclado", pause_after=0.15)
    driver.send_key("Enter", pause_after=0.2)
    driver.send_key("Right", pause_after=0.15)
    driver.type_text("80", pause_after=0.15)
    driver.send_key("Enter", pause_after=0.2)
    driver.send_key("Right", pause_after=0.15)
    driver.type_text("3", pause_after=0.15)
    driver.send_key("Enter", pause_after=0.4)

    # ------------------------------------------------------------------
    # 3. Fórmula en D2 + Previsualización en vivo + Copiar/Pegar relativo
    # ------------------------------------------------------------------
    driver.goto_cell("D2", pause_after=0.4)
    driver.type_text("= B2 * C2", char_delay=0.09, pause_after=1.1)  # Pausa para ver AST en vivo
    driver.send_key("Enter", pause_after=0.8)

    # Copiar D2 y pegar en D3 y D4 (referencias relativas B3*C3, B4*C4)
    driver.send_key("C-c", pause_after=0.5)
    driver.send_key("Down", pause_after=0.25)
    driver.send_key("C-v", pause_after=0.7)
    driver.send_key("Down", pause_after=0.25)
    driver.send_key("C-v", pause_after=0.9)

    # ------------------------------------------------------------------
    # 4. Calltip + Modo Point (señalar rango D2:D4 con flechas)
    # ------------------------------------------------------------------
    driver.goto_cell("A5", pause_after=0.3)
    driver.type_text("TOTAL", pause_after=0.2)
    driver.send_key("Enter", pause_after=0.3)

    driver.goto_cell("D5", pause_after=0.4)
    driver.type_text("= SUM(", char_delay=0.08, pause_after=1.0)     # Muestra calltip de SUM
    driver.send_key("Tab", pause_after=0.6)                         # Activa Modo Point
    driver.send_keys("Up", "Up", "Up", delay=0.25)                  # Apunta a D2
    driver.send_keys("S-Down", "S-Down", delay=0.35)                # Extiende rango D2:D4
    driver.wait(0.9)
    driver.type_text(")", char_delay=0.08, pause_after=0.9)         # Cierra paréntesis
    driver.send_key("Enter", pause_after=1.1)

    # ------------------------------------------------------------------
    # 5. Selección de rango + Quick Stats + Auto-ajuste de columnas
    # ------------------------------------------------------------------
    driver.goto_cell("D2", pause_after=0.35)
    driver.send_keys("S-Down", "S-Down", delay=0.3)                 # Quick Stats de D2:D4
    driver.wait(1.3)

    # Seleccionar A1:D5 y autoajustar columnas (Ctrl+X =)
    driver.goto_cell("A1", pause_after=0.3)
    driver.send_keys("S-Right", "S-Right", "S-Right", delay=0.2)
    driver.send_key("C-x", pause_after=0.5)
    driver.type_text("=", pause_after=0.9)

    # ------------------------------------------------------------------
    # 6. Manejo de errores (#DIV/0!), Undo/Redo y propagación reactiva
    # ------------------------------------------------------------------
    driver.goto_cell("E2", pause_after=0.4)
    driver.type_text("= B2 / 0", char_delay=0.08, pause_after=1.1)  # Previsualiza #DIV/0!
    driver.send_key("Enter", pause_after=1.0)
    driver.send_key("C-z", pause_after=0.9)                         # Undo
    driver.send_key("C-y", pause_after=0.9)                         # Redo
    driver.type_text("= IFERROR(B2 / 0, 0)", char_delay=0.07, pause_after=1.0)
    driver.send_key("Enter", pause_after=0.9)

    # Cambiar B2 de 1200 -> 1500 para ver recálculo en cascada (D2 y D5)
    driver.goto_cell("B2", pause_after=0.4)
    driver.type_text("1500", char_delay=0.08, pause_after=0.4)
    driver.send_key("Enter", pause_after=1.3)

    # ------------------------------------------------------------------
    # 7. Búsqueda rápida (Ctrl+B), Catálogo fx (Ctrl+F) y Ayuda (Ctrl+X ?)
    # ------------------------------------------------------------------
    driver.send_key("C-b", pause_after=0.5)
    driver.type_text("Mouse", char_delay=0.08, pause_after=0.9)
    driver.send_key("Enter", pause_after=0.6)
    driver.send_key("Escape", pause_after=0.5)

    driver.send_key("C-f", pause_after=0.8)                         # Catálogo de funciones
    driver.type_text("COUNT", char_delay=0.08, pause_after=0.9)
    driver.send_keys("Down", "Down", delay=0.35)
    driver.wait(0.8)
    driver.send_key("Escape", pause_after=0.5)

    driver.send_key("C-x", pause_after=0.45)
    driver.type_text("?", pause_after=1.4)                          # Modal de Ayuda
    driver.send_key("Escape", pause_after=0.6)

    # ------------------------------------------------------------------
    # 8. Abrir una hoja compleja real (calificaciones_aeda) e inspeccionar AST
    # ------------------------------------------------------------------
    calif_exists = (SHEETS_DIR / "calificaciones_aeda.txl").exists()
    if calif_exists:
        driver.send_key("C-o", pause_after=0.8)
        # Buscar calificaciones_aeda en la lista con flechas si está presente
        txl_names = sorted(p.stem for p in SHEETS_DIR.glob("*.txl"))
        if "calificaciones_aeda" in txl_names:
            idx = txl_names.index("calificaciones_aeda")
            # En el modal: 0 = "+ Nueva hoja", 1..N = archivos
            for _ in range(idx + 1):
                driver.send_key("Down", pause_after=0.14)
            driver.wait(0.5)
            driver.send_key("Enter", pause_after=1.5)
            driver.goto_cell("F5", pause_after=1.8)                 # Ver fórmula + AST + dependencias

    if auto_exit:
        driver.wait(1.5)
        driver.send_key("C-d", pause_after=0.3)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Ejecuta una demo en vivo de Texcel dentro de tmux en primer plano."
    )
    parser.add_argument(
        "--speed",
        type=float,
        default=1.0,
        help="Multiplicador de velocidad (ej. 0.7 = más lento, 1.5 = más rápido). Default: 1.0",
    )
    parser.add_argument(
        "--auto-exit",
        action="store_true",
        help="Cierra Texcel automáticamente al finalizar el guion (si no, queda abierto para explorar).",
    )
    args = parser.parse_args()

    if shutil.which("tmux") is None:
        print("Error: tmux no está instalado.", file=sys.stderr)
        return 1

    main_bin = ROOT_DIR / "main"
    if not main_bin.exists():
        print("Error: no existe ./main. Compila primero con `make`.", file=sys.stderr)
        return 1

    if not sys.stdin.isatty() or not sys.stdout.isatty():
        print("Error: ejecuta este script desde una terminal interactiva (TTY).", file=sys.stderr)
        return 1

    # Respaldar .config para no alterar la configuración del usuario tras la demo
    config_backup = CONFIG_PATH.read_bytes() if CONFIG_PATH.exists() else None
    demo_existed = DEMO_SHEET_PATH.exists()
    demo_backup = DEMO_SHEET_PATH.read_bytes() if demo_existed else None

    term_size = shutil.get_terminal_size((120, 36))
    session = f"texcel_demo_{uuid.uuid4().hex[:8]}"
    stop_event = threading.Event()

    print(f"Iniciando demo en vivo de Texcel (speed={args.speed}x)...")
    print("Tip: cuando termine el guion puedes seguir explorando o salir con Ctrl+D.")
    time.sleep(1.0)

    try:
        run_tmux([
            "new-session", "-d", "-s", session,
            "-x", str(term_size.columns),
            "-y", str(term_size.lines),
            "-c", str(ROOT_DIR),
            "./main --texcel",
        ])
        run_tmux(["set-option", "-t", session, "status", "off"], check=False)

        driver = TmuxDriver(target=f"{session}:0.0", speed=args.speed, stop_event=stop_event)
        worker = threading.Thread(
            target=run_demo_script,
            args=(driver, args.auto_exit),
            daemon=True,
        )
        worker.start()

        # Adjuntar la terminal actual en primer plano a la sesión de tmux
        subprocess.run(["tmux", "attach-session", "-t", session], check=False)
        stop_event.set()
        worker.join(timeout=1.0)
    finally:
        stop_event.set()
        run_tmux(["kill-session", "-t", session], check=False)

        # Restaurar .config y limpiar hoja temporal de demo
        if config_backup is not None:
            CONFIG_PATH.write_bytes(config_backup)
        elif CONFIG_PATH.exists():
            CONFIG_PATH.unlink()

        if demo_existed and demo_backup is not None:
            DEMO_SHEET_PATH.write_bytes(demo_backup)
        elif DEMO_SHEET_PATH.exists():
            DEMO_SHEET_PATH.unlink()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
