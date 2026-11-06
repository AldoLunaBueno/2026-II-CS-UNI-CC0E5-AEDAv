#!/usr/bin/env bash
set -euo pipefail

INPUT_FILE="${1:-README.md}"
OUTPUT_FILE="${2:-tmp/pr_comment.md}"
REMOTE="${REMOTE:-origin}"
BRANCH="${BRANCH:-$(git rev-parse --abbrev-ref HEAD)}"

mkdir -p "$(dirname "$OUTPUT_FILE")"

# Extrae "owner/repo" desde la URL del remoto (soporta HTTPS y SSH)
REMOTE_URL="$(git remote get-url "$REMOTE")"
REPO_PATH="$(echo "$REMOTE_URL" | sed -E 's#(https?://github\.com/|git@github\.com:)([^/]+/[^/.]+)(\.git)?$#\2#')"

RAW_BASE="https://raw.githubusercontent.com/${REPO_PATH}/${BRANCH}"
BLOB_BASE="https://github.com/${REPO_PATH}/blob/${BRANCH}"

python3 - <<EOF
import re

with open('$INPUT_FILE', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Reemplazar rutas relativas por URLs absolutas de GitHub
content = content.replace('(docs/', '(${RAW_BASE}/docs/')
content = content.replace('(containers/', '(${BLOB_BASE}/containers/')
content = content.replace('(utilities/', '(${BLOB_BASE}/utilities/')

# 2. Extraer los elementos del TOC: - [Título](#slug)
toc_pattern = re.compile(r'^\s*-\s*\[(.*?)\]\(#(.*?)\)', re.MULTILINE)
toc_items = toc_pattern.findall(content)

# 3. Inyectar anclas HTML <a id="slug"></a> antes de cada encabezado indexado
# En comentarios de GitHub, los encabezados Markdown no reciben IDs automáticos;
# al colocar <a id="slug"></a>, el sanitizador de GitHub le asigna id="user-content-slug".
for title, slug in toc_items:
    escaped_title = re.escape(title.strip())
    heading_pattern = re.compile(rf'^(#+\s+{escaped_title}\s*)$', re.MULTILINE)
    match = heading_pattern.search(content)
    if match:
        anchor = f'<a id="{slug}"></a>\n'
        content = content[:match.start()] + anchor + content[match.start():]

# 4. Modificar los enlaces del TOC para que apunten a #user-content-slug
for title, slug in toc_items:
    content = content.replace(f'](#{slug})', f'](#user-content-{slug})')

with open('$OUTPUT_FILE', 'w', encoding='utf-8') as f:
    f.write(content)
EOF

echo "✔ Markdown para PR generado en: $OUTPUT_FILE"
echo "  Repositorio : $REPO_PATH (rama: $BRANCH)"
echo "  Para publicar con GitHub CLI: gh pr comment <NUM_PR> --body-file $OUTPUT_FILE"

