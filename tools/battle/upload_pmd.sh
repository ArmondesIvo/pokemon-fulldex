#!/usr/bin/env bash
# Puts the arena's moving pictures (tools/battle/fetch_pmd.py's NNNN.pmd) in PokeHub's bucket, under
# pmd/, where the app fetches them (pokehub_battle.h, SSR_PMD_URL). Only files not there already
# (by size) are sent, 8 at a time, by Cloudflare's API with PokeHub's own token
# (~/.secrets/pokehub-cloudflare.env, as publish_update.sh).
#
#   tools/battle/upload_pmd.sh <folder of .pmd files>
set -euo pipefail
DIR="${1:?folder of .pmd files}"
ENV_FILE="${POKEHUB_ENV:-$HOME/.secrets/pokehub-cloudflare.env}"
CLOUDFLARE_API_TOKEN="$(sed -n 's/^CLOUDFLARE_API_TOKEN=//p' "$ENV_FILE" | head -1)"
CLOUDFLARE_ACCOUNT_ID="$(sed -n 's/^CLOUDFLARE_ACCOUNT_ID=//p' "$ENV_FILE" | head -1)"
POKEHUB_BUCKET="$(sed -n 's/^POKEHUB_BUCKET=//p' "$ENV_FILE" | head -1)"
POKEHUB_PUBLIC_URL="$(sed -n 's/^POKEHUB_PUBLIC_URL=//p' "$ENV_FILE" | head -1)"
POKEHUB_BUCKET="${POKEHUB_BUCKET:-pokehub}"
POKEHUB_PUBLIC_URL="${POKEHUB_PUBLIC_URL%/}"
export CLOUDFLARE_API_TOKEN CLOUDFLARE_ACCOUNT_ID POKEHUB_BUCKET POKEHUB_PUBLIC_URL
one() {
    f="$1"; name="$(basename "$f")"; size="$(stat -c %s "$f")"
    have="$(curl -sI "$POKEHUB_PUBLIC_URL/pmd/$name" | tr -d '\r' | sed -n 's/^[Cc]ontent-[Ll]ength: //p')"
    [ "$have" = "$size" ] && return 0
    code="$(curl -s -o /dev/null -w '%{http_code}' -X PUT \
        -H "Authorization: Bearer $CLOUDFLARE_API_TOKEN" -H "Content-Type: application/octet-stream" \
        --data-binary "@$f" \
        "https://api.cloudflare.com/client/v4/accounts/$CLOUDFLARE_ACCOUNT_ID/r2/buckets/$POKEHUB_BUCKET/objects/pmd/$name")"
    [ "$code" = 200 ] || echo "FAILED $name ($code)"
}
export -f one
ls "$DIR"/*.pmd | xargs -P 8 -I{} bash -c 'one "$@"' _ {}
echo "done: $(ls "$DIR"/*.pmd | wc -l) files"
