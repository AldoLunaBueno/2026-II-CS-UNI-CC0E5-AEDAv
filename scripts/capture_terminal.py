#!/usr/bin/env python3
"""Capture screenshots of a terminal application running inside tmux.

Drives an interactive CLI/TUI command (default: ./main) in an isolated tmux
session, executes a sequence of keystrokes/waits, captures the ANSI pane
output, and renders each view to a PNG image in a subfolder of tmp/.

Usage examples:
  # Capture initial screen only
  python3 scripts/capture_terminal.py --out-dir tmp/captures --step "initial:sleep=0.5"

  # Navigate menus and capture multiple views
  python3 scripts/capture_terminal.py --out-dir tmp/captures \
    --step "menu:sleep=0.5" \
    --step "option1:keys=1,Enter:sleep=0.5" \
    --step "back:keys=0,Enter:sleep=0.3"
"""

from __future__ import annotations

import argparse
import html
import os
import re
import subprocess
import sys
import tempfile
import time
import uuid
from pathlib import Path

ANSI_16 = {
    30: "#1e1e2e", 31: "#f38ba8", 32: "#a6e3a1", 33: "#f9e2af",
    34: "#89b4fa", 35: "#cba6f7", 36: "#94e2d5", 37: "#cdd6f4",
    90: "#6c7086", 91: "#f38ba8", 92: "#a6e3a1", 93: "#f9e2af",
    94: "#89b4fa", 95: "#cba6f7", 96: "#94e2d5", 97: "#ffffff",
}

ANSI_BG_16 = {k + 10: v for k, v in ANSI_16.items()}

SGR_RE = re.compile(r"\x1b\[([0-9;]*)m")
CSI_OTHER_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
OSC_RE = re.compile(r"\x1b\][^\x07\x1b]*(?:\x07|\x1b\\)")


def xterm_256_to_hex(n: int) -> str:
    """Convert an xterm-256 color index (0-255) to a hex RGB string."""
    if n < 8:
        return ANSI_16[30 + n]
    if n < 16:
        return ANSI_16[90 + (n - 8)]
    if n < 232:
        idx = n - 16
        r = (idx // 36) * 51
        g = ((idx // 6) % 6) * 51
        b = (idx % 6) * 51
        return f"#{r:02x}{g:02x}{b:02x}"
    gray = 8 + (n - 232) * 10
    return f"#{gray:02x}{gray:02x}{gray:02x}"


def ansi_to_html(ansi_text: str) -> str:
    """Convert ANSI SGR-formatted terminal text into styled HTML spans."""
    cleaned = OSC_RE.sub("", ansi_text)
    lines_out: list[str] = []

    for raw_line in cleaned.splitlines():
        raw_line = SGR_RE.sub(lambda m: f"\x00SGR:{m.group(1)}\x00", raw_line)
        raw_line = CSI_OTHER_RE.sub("", raw_line)

        fg: str | None = None
        bg: str | None = None
        bold = False
        dim = False
        italic = False
        underline = False
        reverse = False

        parts = raw_line.split("\x00")
        line_html: list[str] = []

        for part in parts:
            if part.startswith("SGR:"):
                codes_str = part[4:]
                codes = [int(c) if c else 0 for c in codes_str.split(";")] if codes_str else [0]
                i = 0
                while i < len(codes):
                    c = codes[i]
                    if c == 0:
                        fg = bg = None
                        bold = dim = italic = underline = reverse = False
                    elif c == 1:
                        bold = True
                    elif c == 2:
                        dim = True
                    elif c == 3:
                        italic = True
                    elif c == 4:
                        underline = True
                    elif c == 7:
                        reverse = True
                    elif c == 22:
                        bold = dim = False
                    elif c == 23:
                        italic = False
                    elif c == 24:
                        underline = False
                    elif c == 27:
                        reverse = False
                    elif c in ANSI_16:
                        fg = ANSI_16[c]
                    elif c == 39:
                        fg = None
                    elif c in ANSI_BG_16:
                        bg = ANSI_BG_16[c]
                    elif c == 49:
                        bg = None
                    elif c in (38, 48) and i + 1 < len(codes):
                        is_fg = c == 38
                        mode = codes[i + 1]
                        color_val: str | None = None
                        if mode == 5 and i + 2 < len(codes):
                            color_val = xterm_256_to_hex(codes[i + 2])
                            i += 2
                        elif mode == 2 and i + 4 < len(codes):
                            r, g, b = codes[i + 2], codes[i + 3], codes[i + 4]
                            color_val = f"#{r:02x}{g:02x}{b:02x}"
                            i += 4
                        if color_val is not None:
                            if is_fg:
                                fg = color_val
                            else:
                                bg = color_val
                    i += 1
                continue

            if not part:
                continue

            eff_fg = bg or "#11111b" if reverse else fg
            eff_bg = fg or "#cdd6f4" if reverse else bg

            styles: list[str] = []
            if eff_fg:
                styles.append(f"color:{eff_fg}")
            if eff_bg:
                styles.append(f"background-color:{eff_bg}")
            if bold:
                styles.append("font-weight:bold")
            if dim:
                styles.append("opacity:0.7")
            if italic:
                styles.append("font-style:italic")
            if underline:
                styles.append("text-decoration:underline")

            escaped = html.escape(part)
            if styles:
                line_html.append(f'<span style="{";".join(styles)}">{escaped}</span>')
            else:
                line_html.append(escaped)

        lines_out.append("".join(line_html) if line_html else " ")

    return "\n".join(lines_out)


def render_png(ansi_text: str, out_path: Path, title: str, cols: int) -> None:
    """Render ANSI terminal output to a PNG file via wkhtmltoimage."""
    body_html = ansi_to_html(ansi_text)
    full_html = f"""<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  * {{ box-sizing: border-box; margin: 0; padding: 0; }}
  body {{
    background: #11111b;
    color: #cdd6f4;
    font-family: "DejaVu Sans Mono", "Ubuntu Mono", "Noto Sans Mono", monospace;
    font-size: 14px;
    line-height: 1.35;
    padding: 16px 20px;
    display: inline-block;
    min-width: {cols}ch;
  }}
  .titlebar {{
    color: #a6adc8;
    font-size: 12px;
    padding-bottom: 10px;
    margin-bottom: 10px;
    border-bottom: 1px solid #313244;
  }}
  pre {{
    font-family: inherit;
    font-size: inherit;
    line-height: inherit;
    white-space: pre;
  }}
</style>
</head>
<body>
  <div class="titlebar">{html.escape(title)}</div>
  <pre>{body_html}</pre>
</body>
</html>
"""
    with tempfile.NamedTemporaryFile("w", suffix=".html", encoding="utf-8", delete=False) as tf:
        tf.write(full_html)
        html_path = Path(tf.name)

    try:
        subprocess.run(
            ["wkhtmltoimage", "--quiet", "--enable-local-file-access", str(html_path), str(out_path)],
            check=True,
        )
    finally:
        html_path.unlink(missing_ok=True)


def parse_step(raw: str) -> tuple[str, list[str], float]:
    """Parse a step spec 'name[:keys=k1,k2][:sleep=0.5]' safely preserving ':' inside keys."""
    delay = 0.4
    m_sleep = re.search(r":sleep=([0-9.]+)$", raw)
    if m_sleep:
        delay = float(m_sleep.group(1))
        raw = raw[: m_sleep.start()]

    keys: list[str] = []
    idx_keys = raw.find(":keys=")
    if idx_keys != -1:
        name_part = raw[:idx_keys]
        keys_part = raw[idx_keys + 6 :]
        keys = [k for k in keys_part.split(",") if k != ""]
    else:
        name_part = raw

    name = re.sub(r"[^A-Za-z0-9_.-]+", "_", name_part.strip()) or "view"
    return name, keys, delay


def run_tmux(args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["tmux", *args], check=True, text=True, capture_output=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Capture terminal views of a CLI/TUI app via tmux.")
    parser.add_argument("--cmd", default="./main", help="Command to run in tmux (default: ./main)")
    parser.add_argument("--out-dir", default="tmp/captures", help="Output directory under tmp/ (default: tmp/captures)")
    parser.add_argument("--cols", type=int, default=120, help="Terminal width in columns (default: 120)")
    parser.add_argument("--rows", type=int, default=35, help="Terminal height in rows (default: 35)")
    parser.add_argument(
        "--step",
        action="append",
        required=True,
        help="Capture step: 'name[:keys=k1,k2][:sleep=0.5]'. Repeatable.",
    )
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    if not out_dir.parts or out_dir.parts[0] != "tmp":
        print(f"Error: --out-dir must be inside tmp/ (got '{args.out_dir}')", file=sys.stderr)
        return 2
    out_dir.mkdir(parents=True, exist_ok=True)

    session = f"cap_{uuid.uuid4().hex[:8]}"
    try:
        run_tmux([
            "new-session", "-d", "-s", session,
            "-x", str(args.cols), "-y", str(args.rows),
            args.cmd,
        ])

        for idx, raw_step in enumerate(args.step, start=1):
            name, keys, delay = parse_step(raw_step)
            for key in keys:
                run_tmux(["send-keys", "-t", session, key])
                time.sleep(0.08)
            time.sleep(delay)

            cap = run_tmux(["capture-pane", "-e", "-p", "-t", session])
            out_png = out_dir / f"{idx:02d}_{name}.png"
            render_png(cap.stdout, out_png, f"{args.cmd} — {name}", args.cols)
            print(str(out_png))
    finally:
        subprocess.run(["tmux", "kill-session", "-t", session], check=False, capture_output=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
