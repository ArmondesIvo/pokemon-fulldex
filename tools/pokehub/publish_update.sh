#!/usr/bin/env bash
# Puts the APK just built online as PokeHub's newest version: the APK, then latest.json naming it
# (when it was built, its SHA-256 and what changed), in a Cloudflare R2 bucket of PokeHub's own
# account. The app (Updates.java) reads latest.json at each start and offers the new version.
#
#   tools/pokehub/publish_update.sh "What changed, for the update's question"
#
# PokeHub's account, never another: its settings are read from a file of their own
# (~/.secrets/pokehub-cloudflare.env, or POKEHUB_ENV), not from wrangler's login:
#   CLOUDFLARE_API_TOKEN=...      a token with Workers R2 Storage: Edit, on that account only
#   CLOUDFLARE_ACCOUNT_ID=...
#   POKEHUB_BUCKET=pokehub
#   POKEHUB_PUBLIC_URL=https://pub-....r2.dev   the bucket's public address
# The APK must have been built with SSR_UPDATE_URL="$POKEHUB_PUBLIC_URL/latest.json", or the app
# that installs it would never look for the next one: checked below.
set -euo pipefail
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
ENV_FILE="${POKEHUB_ENV:-$HOME/.secrets/pokehub-cloudflare.env}"
APK="${POKEHUB_APK:-$HERE/build-android-dist/pokemon-ds-wide-android.apk}"
BUILT_FILE="$HERE/build-android-dist/fulldex-build-id.txt"
INFO="$HERE/build-android-dist/skeleton/BuildInfo.java"
NOTES="${1:-}"

[ -f "$ENV_FILE" ] || { echo "No $ENV_FILE: PokeHub's Cloudflare settings (see the top of this script)." >&2; exit 1; }
# (Only these four names are taken from it, nothing else of the shell's.)
CLOUDFLARE_API_TOKEN="$(sed -n 's/^CLOUDFLARE_API_TOKEN=//p' "$ENV_FILE" | head -1)"
CLOUDFLARE_ACCOUNT_ID="$(sed -n 's/^CLOUDFLARE_ACCOUNT_ID=//p' "$ENV_FILE" | head -1)"
POKEHUB_BUCKET="$(sed -n 's/^POKEHUB_BUCKET=//p' "$ENV_FILE" | head -1)"
POKEHUB_PUBLIC_URL="$(sed -n 's/^POKEHUB_PUBLIC_URL=//p' "$ENV_FILE" | head -1)"
POKEHUB_BUCKET="${POKEHUB_BUCKET:-pokehub}"
POKEHUB_PUBLIC_URL="${POKEHUB_PUBLIC_URL%/}"
for v in CLOUDFLARE_API_TOKEN CLOUDFLARE_ACCOUNT_ID POKEHUB_PUBLIC_URL; do
    [ -n "${!v}" ] || { echo "$v is not set in $ENV_FILE" >&2; exit 1; }
done
case "$POKEHUB_PUBLIC_URL" in https://*) ;; *) echo "POKEHUB_PUBLIC_URL must start with https://" >&2; exit 1 ;; esac

[ -f "$APK" ] || { echo "No APK at $APK: build it first (tools/android/build_all.sh)." >&2; exit 1; }
[ -f "$BUILT_FILE" ] || { echo "No $BUILT_FILE: build the APK first." >&2; exit 1; }
BUILT="$(tr -dc 0-9 < "$BUILT_FILE")"
[ -n "$BUILT" ] || { echo "$BUILT_FILE has no build time" >&2; exit 1; }
# (The build-id file and the APK must be the same build: the APK no older than the id.)
[ "$(stat -c %Y "$APK")" -ge "$BUILT" ] || { echo "The APK is older than $BUILT_FILE: build again." >&2; exit 1; }
WANT="$POKEHUB_PUBLIC_URL/latest.json"
grep -qF "UPDATE_URL = \"$WANT\"" "$INFO" || {
    echo "This APK was not built with SSR_UPDATE_URL=$WANT: build again with it, or it would never update." >&2; exit 1; }

SHA="$(sha256sum "$APK" | cut -d' ' -f1)"
NAME="pokehub-$BUILT.apk"
VERSION="$(date -d "@$BUILT" '+%d/%m/%Y %H:%M')"
WORK="$(mktemp -d)"
trap 'rm -rf -- "$WORK"' EXIT
# (What changed in the versions before, from the file online now: an app several versions behind
# shows all their news when it is asked to update, once, to the newest.)
curl -fsS "$WANT?t=$(date +%s)" -o "$WORK/before.json" 2>/dev/null || echo '{}' > "$WORK/before.json"
python3 - "$WORK/latest.json" "$WORK/before.json" "$BUILT" "$VERSION" "$NAME" "$SHA" "$NOTES" <<'PY'
import json, sys
out, before, built, version, apk, sha, notes = sys.argv[1:]
try:
    old = json.load(open(before, encoding="utf-8"))
except ValueError:
    old = {}
history = old.get("history") or ([{"built": old["built"], "version": old.get("version", ""), "notes": old.get("notes", "")}] if "built" in old else [])
history = [{"built": int(built), "version": version, "notes": notes}] + [h for h in history if h.get("built") != int(built)]
json.dump({"built": int(built), "version": version, "apk": apk, "sha256": sha, "notes": notes, "history": history[:20]},
          open(out, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
PY

export CLOUDFLARE_API_TOKEN CLOUDFLARE_ACCOUNT_ID
WRANGLER=(npx --yes wrangler@4.45.0)
echo "Sending $NAME ($(du -h "$APK" | cut -f1)) to $POKEHUB_BUCKET..."
"${WRANGLER[@]}" r2 object put "$POKEHUB_BUCKET/$NAME" --file "$APK" --remote \
    --content-type application/vnd.android.package-archive
# (The APK first, then the file naming it: the app never sees a version whose APK is not there yet.)
"${WRANGLER[@]}" r2 object put "$POKEHUB_BUCKET/latest.json" --file "$WORK/latest.json" --remote \
    --content-type application/json --cache-control no-cache
echo "Checking it from outside..."
curl -fsS "$WANT?t=$(date +%s)" | python3 -c 'import json,sys; d=json.load(sys.stdin); print("latest.json:", d["version"], d["apk"])'
curl -fsSI "$POKEHUB_PUBLIC_URL/$NAME" | grep -i '^content-length' || { echo "The APK is not reachable at $POKEHUB_PUBLIC_URL/$NAME" >&2; exit 1; }
echo "Published: PokeHub $VERSION. The apps ask to update at their next start."
