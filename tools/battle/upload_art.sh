#!/usr/bin/env bash
# Puts the arena's pictures in PokeHub's bucket, where the app fetches them (pokehub_battle.h):
# the moving pictures (tools/battle/fetch_pmd.py's NNNN.pmd) under pmd/, and the trainers'
# (Showdown's, ~/projetos/jogos/pokemon-fulldex-ref/trainers/*.png) under trainers/. Only files not there already
# (by size) are sent, 8 at a time, by Cloudflare's API with PokeHub's own token
# (~/.secrets/pokehub-cloudflare.env, as publish_update.sh).
#
#   tools/battle/upload_pmd.sh <folder> [prefix: pmd] [extension: pmd]
set -euo pipefail
DIR="${1:?folder of files}"
PREFIX="${2:-pmd}"
EXT="${3:-pmd}"
export PREFIX
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
    have="$(curl -sI "$POKEHUB_PUBLIC_URL/$PREFIX/$name" | tr -d '\r' | sed -n 's/^[Cc]ontent-[Ll]ength: //p')"
    [ "$have" = "$size" ] && return 0
    code="$(curl -s -o /dev/null -w '%{http_code}' -X PUT \
        -H "Authorization: Bearer $CLOUDFLARE_API_TOKEN" -H "Content-Type: application/octet-stream" \
        --data-binary "@$f" \
        "https://api.cloudflare.com/client/v4/accounts/$CLOUDFLARE_ACCOUNT_ID/r2/buckets/$POKEHUB_BUCKET/objects/$PREFIX/$name")"
    [ "$code" = 200 ] || echo "FAILED $name ($code)"
}
export -f one
ls "$DIR"/*."$EXT" | xargs -P 8 -I{} bash -c 'one "$@"' _ {}
echo "done: $(ls "$DIR"/*."$EXT" | wc -l) files"
