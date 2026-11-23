#!/usr/bin/env python3
"""
Pruebas End-to-End (E2E) headless sobre un pseudoterminal (PTY) real para Texcel.
Ejecuta `./main --texcel` en segundo plano sin tocar la terminal del usuario ni
modificar `output/sheets/` del repositorio, enviando secuencias reales de teclado
y ratón SGR y verificando tanto la salida ANSI como la hoja persistida.
"""

import fcntl
import os
import pty
import re
import select
import struct
import subprocess
import sys
import tempfile
import termios
import time

# Constantes de layout de texcel_tui::TuiState (src/texcel/helpers/state.h)
LEFT_X = 0
TOP_Y = 1
ROW_HEADER_W = 8
CELL_WIDTH = 9

ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")


def strip_ansi(data: str) -> str:
    return ANSI_RE.sub("", data)


def cell_sgr_coords(col: int, row: int) -> tuple[int, int]:
    """Devuelve coordenadas (x1, y1) base-1 dentro de la celda (col, row)."""
    grid_x0 = LEFT_X + 2 + ROW_HEADER_W
    mx = grid_x0 + col * (CELL_WIDTH + 1) + 2
    my = TOP_Y + 3 + row
    return mx + 1, my + 1


def sgr_mouse(btn: int, col: int, row: int, release: bool = False) -> bytes:
    x1, y1 = cell_sgr_coords(col, row)
    suffix = "m" if release else "M"
    return f"\x1b[<{btn};{x1};{y1}{suffix}".encode("ascii")


class PtyTexcelSession:
    def __init__(self, binary_path: str, workdir: str, cols: int = 130, rows: int = 35):
        self.master_fd, slave_fd = pty.openpty()
        fcntl.ioctl(
            self.master_fd,
            termios.TIOCSWINSZ,
            struct.pack("HHHH", rows, cols, 0, 0),
        )
        env = os.environ.copy()
        env["TERM"] = "xterm-256color"
        self.proc = subprocess.Popen(
            [binary_path, "--texcel"],
            stdin=slave_fd,
            stdout=slave_fd,
            stderr=slave_fd,
            cwd=workdir,
            env=env,
            close_fds=True,
        )
        os.close(slave_fd)
        self.raw_output = bytearray()

    def drain(self, wait_s: float = 0.08) -> str:
        deadline = time.monotonic() + wait_s
        chunk = bytearray()
        while True:
            rem = deadline - time.monotonic()
            if rem <= 0:
                break
            r, _, _ = select.select([self.master_fd], [], [], rem)
            if not r:
                break
            try:
                data = os.read(self.master_fd, 65536)
            except OSError:
                break
            if not data:
                break
            chunk.extend(data)
            self.raw_output.extend(data)
            # Una vez que llega algo, acortar la espera para coalescer el frame
            deadline = min(deadline, time.monotonic() + 0.03)
        return chunk.decode("utf-8", errors="replace")

    def wait_for_text(self, needle: str, timeout_s: float = 3.0) -> str:
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            self.drain(0.05)
            plain = strip_ansi(self.raw_output.decode("utf-8", errors="replace"))
            if needle in plain:
                return plain
        raise AssertionError(
            f"Timeout esperando '{needle}' en la salida PTY.\nÚltima salida:\n"
            + strip_ansi(self.raw_output.decode("utf-8", errors="replace"))[-800:]
        )

    def send(self, payload: bytes, pause_s: float = 0.04) -> None:
        os.write(self.master_fd, payload)
        self.drain(pause_s)

    def close(self, timeout_s: float = 3.0) -> int:
        try:
            rc = self.proc.wait(timeout=timeout_s)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
            raise AssertionError("Texcel no terminó a tiempo tras enviar la secuencia de salida.")
        finally:
            try:
                os.close(self.master_fd)
            except OSError:
                pass
        return rc


def main() -> int:
    binary = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "./main")
    if not os.path.exists(binary):
        raise FileNotFoundError(f"Binario no encontrado: {binary}")

    with tempfile.TemporaryDirectory(prefix="texcel_e2e_") as tmpdir:
        sess = PtyTexcelSession(binary, tmpdir)
        try:
            # 1. Esperar render inicial de la TUI interactiva
            sess.wait_for_text("Texcel", timeout_s=3.0)

            # 2. Poblar A1 = 10 y B1 = 25 desde Modo 2 (Tab + Enter)
            sess.send(b"\t10\n")
            sess.send(sgr_mouse(0, 1, 0) + sgr_mouse(0, 1, 0, release=True))  # Clic en B1
            sess.send(b"\t25\n")

            # 3. Modo 2 en C1: escribir '=()+5' -> mover cursor dentro de '()' -> clic SGR en A1
            sess.send(sgr_mouse(0, 2, 0) + sgr_mouse(0, 2, 0, release=True))  # Clic en C1
            sess.send(b"\t=()+5")
            sess.send(b"\x1b[D\x1b[D\x1b[D")  # Flecha izquierda x3: cursor entre '(' y ')'
            sess.send(sgr_mouse(0, 0, 0) + sgr_mouse(0, 0, 0, release=True))  # Clic en A1 -> =(A1)+5 -> 15
            sess.send(b"\n")

            # 4. Modo 2 en D1: insertar rango A1:B1 arrastrando con ratón dentro de '=SUM()'
            sess.send(sgr_mouse(0, 3, 0) + sgr_mouse(0, 3, 0, release=True))  # Clic en D1
            sess.send(b"\t=SUM()")
            sess.send(b"\x1b[D")
            sess.send(sgr_mouse(0, 0, 0))                   # Mouse down en A1
            sess.send(sgr_mouse(32, 1, 0))                  # Drag hacia B1
            sess.send(sgr_mouse(0, 1, 0, release=True))     # Mouse up en B1 -> =SUM(A1:B1) -> 35
            sess.send(b"\n")

            # 5. Modo 2 en A2 con valor pendiente '99' -> clic en B2 guarda '99' en A2
            sess.send(sgr_mouse(0, 0, 1) + sgr_mouse(0, 0, 1, release=True))  # Clic en A2
            sess.send(b"\t99")
            sess.send(sgr_mouse(0, 1, 1) + sgr_mouse(0, 1, 1, release=True))  # Clic en B2 (auto-commit)

            # 6. Guardar hoja con Ctrl+S y salir limpio con 'q' en Modo 1
            sess.send(b"\x13")  # Ctrl+S
            sess.send(b"q")     # Salir desde GridNavigation
            rc = sess.close(timeout_s=3.0)
            assert rc == 0, f"Código de salida inesperado: {rc}"
        except Exception:
            if sess.proc.poll() is None:
                sess.proc.kill()
                sess.proc.wait()
            raise

        # Verificar salida renderizada ANSI (limpia) y archivo persistido .txl
        plain_out = strip_ansi(sess.raw_output.decode("utf-8", errors="replace"))
        assert "15" in plain_out, "No se observó el resultado 15 de =(A1)+5 en pantalla."
        assert "35" in plain_out, "No se observó el resultado 35 de =SUM(A1:B1) en pantalla."
        assert "99" in plain_out, "No se observó 99 tras hacer clic fuera de A2."

        saved_path = os.path.join(tmpdir, "output", "sheets", "default.txl")
        assert os.path.exists(saved_path), f"No se generó {saved_path} con Ctrl+S."
        with open(saved_path, "r", encoding="utf-8") as fh:
            saved_txl = fh.read()

        assert "A1" in saved_txl and "10" in saved_txl, f"Falta A1=10 en .txl:\n{saved_txl}"
        assert "B1" in saved_txl and "25" in saved_txl, f"Falta B1=25 en .txl:\n{saved_txl}"
        assert "(A1)+5" in saved_txl, f"Falta fórmula =(A1)+5 en .txl:\n{saved_txl}"
        assert "SUM(A1:B1)" in saved_txl, f"Falta fórmula SUM(A1:B1) en .txl:\n{saved_txl}"
        assert "A2" in saved_txl and "99" in saved_txl, f"Falta A2=99 (commit-on-click) en .txl:\n{saved_txl}"

    print("[PASS] test_tui_pty")
    return 0


if __name__ == "__main__":
    sys.exit(main())
