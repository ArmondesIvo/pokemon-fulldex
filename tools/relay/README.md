# Fulldex trade relay

The meeting point for online trades (frontend/hub_link.cpp): a Cloudflare Worker, free tier.

Put it online (once, from this folder, with a free Cloudflare account):

    npx wrangler login
    npx wrangler deploy

It prints its address, e.g. https://fulldex-relay.<you>.workers.dev. Builds get it with
-DFULLDEX_RELAY_URL=<that address>; a test build also reads fulldex-relay.txt beside its settings.

Try it on this machine first: `npx wrangler dev` (http://localhost:8787).
