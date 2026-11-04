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

sed -e "s|(docs/|(${RAW_BASE}/docs/|g" \
    -e "s|(containers/|(${BLOB_BASE}/containers/|g" \
    -e "s|(utilities/|(${BLOB_BASE}/utilities/|g" \
    "$INPUT_FILE" > "$OUTPUT_FILE"

echo "✔ Markdown para PR generado en: $OUTPUT_FILE"
echo "  Repositorio : $REPO_PATH (rama: $BRANCH)"
echo "  Para publicar con GitHub CLI: gh pr comment <NUM_PR> --body-file $OUTPUT_FILE"

