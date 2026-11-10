#!/usr/bin/env bash
# PokeHub for iPhone, on GitHub (the fork ArmondesIvo/pokemon-fulldex): sends this branch as
# "pokehub", turns the fork's Actions on, makes "pokehub" its default branch (a workflow is only
# started from the default branch's list), puts the build inputs in a draft (unpublished)
# release, and starts the build (.github/workflows/pokehub-ios.yml). The .ipa is then the run's
# artifact.
#
#   tools/pokehub/ios_github.sh <pokehub-ios-inputs.tar.gz>
set -euo pipefail
REPO=ArmondesIvo/pokemon-fulldex
INPUTS="${1:?the inputs: pokehub-ios-inputs.tar.gz}"
cd "$(dirname "$0")/../.."
echo "== Sending the code (branch pokehub)"
git -c credential.helper="!gh auth git-credential" push --force https://github.com/$REPO.git HEAD:refs/heads/pokehub
echo "== Actions on, pokehub the default branch"
gh api -X PUT "repos/$REPO/actions/permissions" -F enabled=true -f allowed_actions=all >/dev/null
gh repo edit "$REPO" --default-branch pokehub
echo "== The inputs, in a draft release"
gh release delete pokehub-ios-inputs -R "$REPO" --yes >/dev/null 2>&1 || true
gh release create pokehub-ios-inputs -R "$REPO" --draft --target pokehub --title "PokeHub iOS inputs (private)" --notes "Build inputs only: tables of addresses, pictures, icon, box sprites. No ROM." "$INPUTS"
echo "== Starting the build"
sleep 5
gh workflow run pokehub-ios.yml -R "$REPO" --ref pokehub
echo "Started. Follow it at https://github.com/$REPO/actions"
