// PokeHub's server (a Cloudflare Worker, on PokeHub's own Cloudflare account): signing in with
// Google, and later the cloud saves. The app (Account.java) opens /auth/start in the browser; this
// sends the player to Google, checks Google's answer, keeps the player (D1: users) and a session
// (D1: sessions, only a hash of its token), and sends the browser back to the app with the token.
//
//   GET  /auth/start?state=<the app's random state>   -> Google's sign-in page
//   GET  /auth/callback?code=...&state=...             <- Google, -> a page that opens the app
//   POST /auth/signout   (Authorization: Bearer <token>)  ends that session
//   GET  /me             (Authorization: Bearer <token>)  {"email","name"}
//
// Settings: GOOGLE_CLIENT_ID (a variable) and GOOGLE_CLIENT_SECRET (a secret), a Google OAuth
// client of the "Web application" kind whose redirect address is <this worker>/auth/callback.

const SESSION_DAYS = 365;
const APP_PACKAGE = "io.github.mamesj.pokemonds";

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    try {
      if (url.pathname === "/auth/start" && request.method === "GET") return start(url, env);
      if (url.pathname === "/auth/callback" && request.method === "GET") return await callback(request, url, env);
      if (url.pathname === "/auth/signout" && request.method === "POST") return await signout(request, env);
      if (url.pathname === "/me" && request.method === "GET") return await me(request, env);
      if (url.pathname === "/") return text("PokeHub");
      return text("Not found", 404);
    } catch (e) {
      console.error(e && e.stack || e);
      return text("Something went wrong on PokeHub's server.", 500);
    }
  },
};

function text(body, status = 200, headers = {}) {
  return new Response(body, { status, headers: { "content-type": "text/plain; charset=utf-8", ...headers } });
}
function json(obj, status = 200) {
  return new Response(JSON.stringify(obj), { status, headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store" } });
}
function randomHex(bytes) {
  const b = crypto.getRandomValues(new Uint8Array(bytes));
  return [...b].map((x) => x.toString(16).padStart(2, "0")).join("");
}
async function sha256Hex(s) {
  const d = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(s));
  return [...new Uint8Array(d)].map((x) => x.toString(16).padStart(2, "0")).join("");
}
function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}

// (The browser's own check that the answer comes to the same browser that asked: a nonce in a
// short-lived cookie, and in the state Google hands back.)
function start(url, env) {
  const appState = url.searchParams.get("state") || "";
  if (!/^[0-9a-f]{32,96}$/.test(appState)) return text("Open the sign-in from the PokeHub app.", 400);
  const nonce = randomHex(16);
  const google = new URL("https://accounts.google.com/o/oauth2/v2/auth");
  google.searchParams.set("client_id", env.GOOGLE_CLIENT_ID);
  google.searchParams.set("redirect_uri", new URL("/auth/callback", url).toString());
  google.searchParams.set("response_type", "code");
  google.searchParams.set("scope", "openid email profile");
  google.searchParams.set("state", appState + "." + nonce);
  google.searchParams.set("prompt", "select_account");
  return new Response(null, {
    status: 302,
    headers: {
      location: google.toString(),
      "set-cookie": `pokehub_nonce=${nonce}; Max-Age=600; Path=/auth; HttpOnly; Secure; SameSite=Lax`,
      "cache-control": "no-store",
    },
  });
}

async function callback(request, url, env) {
  if (url.searchParams.get("error")) return page("Sign-in cancelled", "You can go back to PokeHub.", null);
  const code = url.searchParams.get("code") || "";
  const [appState, nonce] = (url.searchParams.get("state") || "").split(".");
  const cookie = (request.headers.get("cookie") || "").match(/(?:^|;\s*)pokehub_nonce=([0-9a-f]+)/);
  if (!code || !appState || !nonce || !cookie || cookie[1] !== nonce) return page("Sign-in did not work", "Start it again from PokeHub.", null);

  // (Google's answer, asked of Google itself over HTTPS: its ID token needs no other check than
  // being for this client, from Google, for a verified address.)
  const r = await fetch("https://oauth2.googleapis.com/token", {
    method: "POST",
    headers: { "content-type": "application/x-www-form-urlencoded" },
    body: new URLSearchParams({
      code,
      client_id: env.GOOGLE_CLIENT_ID,
      client_secret: env.GOOGLE_CLIENT_SECRET,
      redirect_uri: new URL("/auth/callback", url).toString(),
      grant_type: "authorization_code",
    }),
  });
  if (!r.ok) { console.error("token", r.status, await r.text()); return page("Sign-in did not work", "Google did not confirm it. Try again from PokeHub.", null); }
  const tok = await r.json();
  const part = (tok.id_token || "").split(".")[1] || "";
  // (base64url, padded, as UTF-8: a name with accents stays as it is.)
  const b64 = part.replace(/-/g, "+").replace(/_/g, "/").padEnd(Math.ceil(part.length / 4) * 4, "=");
  const claims = JSON.parse(new TextDecoder().decode(Uint8Array.from(atob(b64), (ch) => ch.charCodeAt(0))));
  if (claims.aud !== env.GOOGLE_CLIENT_ID || !["accounts.google.com", "https://accounts.google.com"].includes(claims.iss) || !claims.email_verified || !claims.sub)
    return page("Sign-in did not work", "That Google account could not be confirmed.", null);

  const now = Math.floor(Date.now() / 1000);
  await env.DB.prepare(
    "INSERT INTO users (id, email, name, created, seen) VALUES (?1, ?2, ?3, ?4, ?4) " +
    "ON CONFLICT(id) DO UPDATE SET email = ?2, name = ?3, seen = ?4"
  ).bind(claims.sub, claims.email, claims.name || "", now).run();
  const token = randomHex(32);
  await env.DB.prepare("INSERT INTO sessions (hash, user_id, created, expires) VALUES (?1, ?2, ?3, ?4)")
    .bind(await sha256Hex(token), claims.sub, now, now + SESSION_DAYS * 86400).run();

  const back = new URLSearchParams({ token, state: appState, email: claims.email, name: claims.name || "" });
  const intent = `intent://signin?${back}#Intent;scheme=pokehub;package=${APP_PACKAGE};end`;
  return page("Signed in", `Signed in as ${claims.email}. Going back to PokeHub...`, intent);
}

// The page the browser shows at the end: it opens the app (Chrome wants a tap for that at times:
// the button is there for it).
function page(title, message, appLink) {
  const link = appLink ? escapeHtml(appLink) : "";
  const html = `<!doctype html><html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PokeHub</title><style>
:root{--bg:#faf6f0;--ink:#3a1e08;--primary:#b6634e}
body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;background:var(--bg);color:var(--ink);font-family:system-ui,sans-serif}
main{max-width:420px;padding:32px 24px;text-align:center}
h1{font-size:24px;margin:0 0 12px}p{font-size:16px;line-height:1.5;margin:0 0 24px}
a{display:inline-block;background:var(--primary);color:#faf6f0;text-decoration:none;padding:14px 28px;border-radius:12px;font-weight:600}
</style></head><body><main><h1>${escapeHtml(title)}</h1><p>${escapeHtml(message)}</p>
${link ? `<a href="${link}">Voltar ao PokeHub</a><script>location.href=${JSON.stringify(appLink)};</script>` : ""}
</main></body></html>`;
  return new Response(html, { headers: { "content-type": "text/html; charset=utf-8", "cache-control": "no-store", "referrer-policy": "no-referrer" } });
}

async function session(request, env) {
  const m = (request.headers.get("authorization") || "").match(/^Bearer ([0-9a-f]{64})$/);
  if (!m) return null;
  const hash = await sha256Hex(m[1]);
  const row = await env.DB.prepare(
    "SELECT s.hash, u.id, u.email, u.name FROM sessions s JOIN users u ON u.id = s.user_id WHERE s.hash = ?1 AND s.expires > ?2"
  ).bind(hash, Math.floor(Date.now() / 1000)).first();
  return row;
}

async function signout(request, env) {
  const s = await session(request, env);
  if (s) await env.DB.prepare("DELETE FROM sessions WHERE hash = ?1").bind(s.hash).run();
  return json({ ok: true });
}

async function me(request, env) {
  const s = await session(request, env);
  if (!s) return json({ error: "not signed in" }, 401);
  return json({ email: s.email, name: s.name });
}
