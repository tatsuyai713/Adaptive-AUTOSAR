#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DOXYGEN_CONF="${REPO_ROOT}/docs/doxygen.conf"
DOCS_DIR="${REPO_ROOT}/docs"
HTML_DIR="${DOCS_DIR}/api"

if ! command -v doxygen >/dev/null 2>&1; then
  echo "[ERROR] doxygen is not installed. Install it first." >&2
  exit 1
fi

mkdir -p "${DOCS_DIR}"
rm -rf "${HTML_DIR}"
rm -f "${DOCS_DIR}/doxygen-warnings.log" "${DOCS_DIR}/index.html"

cd "${REPO_ROOT}"
doxygen "${DOXYGEN_CONF}"

if [[ ! -f "${HTML_DIR}/index.html" ]]; then
  echo "[ERROR] Doxygen output was not generated: ${HTML_DIR}/index.html" >&2
  exit 1
fi

cat > "${DOCS_DIR}/index.html" << 'HTML'
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta http-equiv="refresh" content="0; url=./api/index.html">
  <title>Adaptive AUTOSAR API Docs</title>
</head>
<body>
  <p>Redirecting to <a href="./api/index.html">API documentation</a>...</p>
</body>
</html>
HTML

echo "[OK] Doxygen documentation generated under: ${DOCS_DIR}"
echo "[INFO] Open: ${DOCS_DIR}/index.html"
