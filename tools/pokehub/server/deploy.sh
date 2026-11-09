#!/usr/bin/env bash
# Puts PokeHub's server (worker.js) online, on PokeHub's own Cloudflare account, as the Worker
# "pokehub-api", with its D1 database "pokehub" (made, and its tables, the first time).
#
#   tools/pokehub/server/deploy.sh
#
# Reads, never from wrangler's login (the Kardap's account is another):
#   ~/.secrets/pokehub-cloudflare.env   CLOUDFLARE_API_TOKEN, CLOUDFLARE_ACCOUNT_ID (the token
#                                       also needs Workers Scripts: Edit and D1: Edit)
#   ~/.secrets/pokehub-google-client.txt   the Google sign-in client: its id, then its secret
# Says at the end the server's address, for building the app with SSR_ACCOUNT_URL.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ENV_FILE="${POKEHUB_ENV:-$HOME/.secrets/pokehub-cloudflare.env}"
GOOGLE_FILE="${POKEHUB_GOOGLE:-$HOME/.secrets/pokehub-google-client.txt}"
CLOUDFLARE_API_TOKEN="$(sed -n 's/^CLOUDFLARE_API_TOKEN=//p' "$ENV_FILE" | head -1)"
CLOUDFLARE_ACCOUNT_ID="$(sed -n 's/^CLOUDFLARE_ACCOUNT_ID=//p' "$ENV_FILE" | head -1)"
[ -n "$CLOUDFLARE_API_TOKEN" ] && [ -n "$CLOUDFLARE_ACCOUNT_ID" ] || { echo "CLOUDFLARE_API_TOKEN / CLOUDFLARE_ACCOUNT_ID missing in $ENV_FILE" >&2; exit 1; }
GOOGLE_ID="$(sed -n 1p "$GOOGLE_FILE" | tr -d '[:space:]')"
GOOGLE_SECRET="$(sed -n 2p "$GOOGLE_FILE" | tr -d '[:space:]')"
# (Without the Google client yet, it is put online all the same, to learn its address: the
# Google client needs that address first. Signing in then waits for the next deploy.)
HAVE_GOOGLE=1
case "$GOOGLE_ID" in *.apps.googleusercontent.com) ;; *) HAVE_GOOGLE=0 ;; esac
[ -n "$GOOGLE_SECRET" ] && [ "$GOOGLE_SECRET" != COLE_O_CLIENT_SECRET_AQUI ] || HAVE_GOOGLE=0
if [ "$HAVE_GOOGLE" = 0 ]; then GOOGLE_ID=""; echo "No Google client in $GOOGLE_FILE yet: deploying without sign-in." >&2; fi
export CLOUDFLARE_API_TOKEN CLOUDFLARE_ACCOUNT_ID
W=(npx --yes wrangler@4.45.0)
WORK="$(mktemp -d)"
trap 'rm -rf -- "$WORK"' EXIT
cp "$HERE/worker.js" "$WORK/worker.js"

# The database: found by its name, or made.
DB_ID="$("${W[@]}" d1 list --json 2>/dev/null | python3 -c 'import json,sys; print(next((d["uuid"] for d in json.load(sys.stdin) if d.get("name")=="pokehub"), ""))')"
if [ -z "$DB_ID" ]; then
    echo "Making the database pokehub..."
    "${W[@]}" d1 create pokehub >/dev/null
    DB_ID="$("${W[@]}" d1 list --json | python3 -c 'import json,sys; print(next(d["uuid"] for d in json.load(sys.stdin) if d.get("name")=="pokehub"))')"
fi
cat > "$WORK/wrangler.toml" <<EOF
name = "pokehub-api"
main = "worker.js"
compatibility_date = "2025-10-01"
workers_dev = true

[vars]
GOOGLE_CLIENT_ID = "$GOOGLE_ID"

[[d1_databases]]
binding = "DB"
database_name = "pokehub"
database_id = "$DB_ID"
EOF
echo "Tables (made if missing)..."
(cd "$WORK" && "${W[@]}" d1 execute pokehub --remote --file "$HERE/schema.sql" >/dev/null)
echo "Deploying..."
(cd "$WORK" && "${W[@]}" deploy 2>&1 | tee "$WORK/deploy.log" | grep -E "https://|Uploaded|Deployed|error|Error" || true)
# (The secret after the Worker exists; given on stdin, never on the command line.)
[ "$HAVE_GOOGLE" = 0 ] || (cd "$WORK" && printf '%s' "$GOOGLE_SECRET" | "${W[@]}" secret put GOOGLE_CLIENT_SECRET >/dev/null)
URL="$(grep -o 'https://pokehub-api\.[a-z0-9-]*\.workers\.dev' "$WORK/deploy.log" | head -1)"
[ -n "$URL" ] || { echo "Deployed, but its address was not found in wrangler's answer: see the dashboard." >&2; exit 1; }
curl -fsS "$URL/" >/dev/null && echo "Server: $URL (build the app with SSR_ACCOUNT_URL=$URL)"
echo "Google sign-in redirect address (in the Google client): $URL/auth/callback"
