---
trigger: model_decision
description: >-
  Tomar capturas automáticas de la terminal en una subcarpeta de tmp/ cuando se
  trabaja en el aspecto visual de la aplicación CLI/TUI del proyecto.
---

# Capturas visuales de terminal en `tmp/`

Cuando el usuario o la tarea involucre trabajar en el **aspecto visual** del proyecto que corre en terminal (menús, tablas, colores ANSI, alineación, bordes, banners, prompts o layout de `./main`), debes tomar capturas automáticas de las vistas relevantes de la terminal para verificar el resultado.

## Reglas obligatorias

1. **Ubicación de las capturas**:
   - Guarda todas las imágenes `.png` dentro de una subcarpeta de `tmp/`, por ejemplo `tmp/captures/` o `tmp/<nombre-tarea>/` (nunca en la raíz del proyecto ni fuera de `tmp/`).
2. **Herramienta de captura**:
   - Usa el script [capture_terminal.py](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/scripts/capture_terminal.py) (basado en `tmux` + `wkhtmltoimage`), pasando únicamente los `--step` relevantes para las vistas afectadas por la tarea.
   - Si el código C/C++ cambió, compila primero (`make`) antes de capturar.
3. **Inspección visual**:
   - Después de generar las capturas, revisa las imágenes `.png` relevantes con `read_file` para comprobar alineación, colores, legibilidad y ausencia de desbordamientos, e indica al usuario las rutas de las capturas generadas.
