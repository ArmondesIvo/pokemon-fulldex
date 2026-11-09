// PokeHub's server (a Cloudflare Worker, on PokeHub's own Cloudflare account): signing in with
// Google, and later the cloud saves. The app (Account.java) opens /auth/start in the browser; this
// sends the player to Google, checks Google's answer, keeps the player (D1: users) and a session
// (D1: sessions, only a hash of its token), and sends the browser back to the app with the token.
//
//   GET  /auth/start?state=<the app's random state>   -> Google's sign-in page
//   GET  /auth/callback?code=...&state=...             <- Google, -> a page that opens the app
//   POST /auth/signout   (Authorization: Bearer <token>)  ends that session
//   GET  /me             (Authorization: Bearer <token>)  {"email","name"}
//   GET|PUT|DELETE /saves/<name>        a save of the signed-in player's (the app's cloud saves,
//   GET|PUT|DELETE /saves/<name>.json   frontend/cloud_saves.cpp: the WebDAV way, by GET and PUT)
//                  kept in the private R2 bucket SAVES, under users/<their Google id>/<name>.
//   GET  /users                          the other players: "id<TAB>name<TAB>email" lines
//   GET  /trades                         the player's open trades (and finished ones not yet put away)
//   POST /trades                         a new one: "to<TAB>id", then N "M<TAB>gen<TAB>data<TAB>place"
//   POST /trades/<id>/counter            B: "pick<TAB>i", then N "M..." lines of theirs
//   POST /trades/<id>/finish             A: "pick<TAB>i": done
//   POST /trades/<id>/cancel             either, while open
//   POST /trades/<id>/settled            this side's app has put its Pokémon where they go
//   (Text, a line a thing, tab-separated: the app reads it without a JSON library.)
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
      if (url.pathname.startsWith("/saves/")) return await saves(request, url, env);
      if (url.pathname === "/users" && request.method === "GET") return await users(request, env);
      if (url.pathname === "/trades" || url.pathname.startsWith("/trades/")) return await trades(request, url, env);
      if (url.pathname === "/") return info("PokeHub", HOME);
      if (url.pathname === "/privacy") return info("PokeHub: Privacidade", PRIVACY);
      return text("Not found", 404);
    } catch (e) {
      console.error(e && e.stack || e);
      return text("Something went wrong on PokeHub's server.", 500);
    }
  },
};

// The home page and the privacy policy Google's sign-in asks for (its Branding page).
const HOME = `<h1>PokeHub</h1>
<p>PokeHub é um app particular, para um pequeno grupo de amigos, que reúne jogos de Pokémon que cada um já tem, um PC de Pokémon e uma Pokédex.</p>
<p>A conta do PokeHub (entrar com o Google) serve para guardar os saves de cada pessoa e ver as próprias coisas em qualquer celular.</p>
<p><a href="/privacy">Política de privacidade</a></p>`;
const PRIVACY = `<h1>Política de privacidade</h1>
<p>Última atualização: 9 de novembro de 2026.</p>
<h2>O que o PokeHub guarda</h2>
<p>Quando você entra com o Google, o PokeHub recebe do Google apenas o seu <b>nome</b>, o seu <b>e-mail</b> e um <b>número que identifica a sua conta Google</b>. Ele guarda isso para saber que a conta é sua, e guarda a data em que você entrou.</p>
<p>Depois do login, o app fica com uma chave de sessão no seu celular. No servidor fica só uma impressão digital (hash) dela, nunca a chave.</p>
<p>Se você usar os saves na nuvem, os arquivos de save dos seus jogos ficam guardados no servidor do PokeHub, ligados à sua conta.</p>
<h2>O que o PokeHub não faz</h2>
<p>O PokeHub não lê seus e-mails, contatos, arquivos do Google Drive nem nada além do nome e do e-mail. Não vende, não compartilha e não usa seus dados para anúncios.</p>
<h2>Onde ficam</h2>
<p>Os dados ficam na Cloudflare (Workers, D1 e R2), na conta do PokeHub.</p>
<h2>Apagar</h2>
<p>Você pode sair da conta no app a qualquer momento. Para apagar sua conta e seus saves do servidor, peça a quem te passou o PokeHub, que apaga tudo.</p>
<h2>Contato</h2>
<p>armondeseuro@gmail.com</p>`;
function info(title, body) {
  const html = `<!doctype html><html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>${title}</title><style>
:root{--bg:#faf6f0;--ink:#3a1e08;--hint:#7b7467;--primary:#b6634e}
body{margin:0;background:var(--bg);color:var(--ink);font-family:system-ui,sans-serif;line-height:1.6}
main{max-width:680px;margin:0 auto;padding:32px 16px}h1{color:var(--primary)}a{color:var(--primary)}
</style></head><body><main>${body}</main></body></html>`;
  return new Response(html, { headers: { "content-type": "text/html; charset=utf-8" } });
}

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

// The cloud saves: only the signed-in player's own, by name (a save file, and its description).
const SAVE_MAX = 8 << 20; // (the largest save of a DS game is 512 KB; room for any, and no more)
async function saves(request, url, env) {
  const s = await session(request, env);
  if (!s) return json({ error: "not signed in" }, 401);
  let name;
  try { name = decodeURIComponent(url.pathname.slice("/saves/".length)); } catch { return json({ error: "bad name" }, 400); }
  // (A plain file name: never a path, so one player's key can never reach another's.)
  if (!/^[A-Za-z0-9][A-Za-z0-9 ._()+-]{0,150}$/.test(name) || name.includes("..")) return json({ error: "bad name" }, 400);
  const key = `users/${s.id}/${name}`;
  if (request.method === "GET") {
    const obj = await env.SAVES.get(key);
    if (!obj) return json({ error: "no such save" }, 404);
    return new Response(obj.body, { headers: { "content-type": "application/octet-stream", "cache-control": "no-store" } });
  }
  if (request.method === "PUT") {
    const len = Number(request.headers.get("content-length") || "0");
    if (len > SAVE_MAX) return json({ error: "too large" }, 413);
    const data = await request.arrayBuffer();
    if (data.byteLength > SAVE_MAX) return json({ error: "too large" }, 413);
    await env.SAVES.put(key, data);
    return json({ ok: true }, 201);
  }
  if (request.method === "DELETE") {
    await env.SAVES.delete(key);
    return json({ ok: true });
  }
  return json({ error: "method" }, 405);
}

// ---- Online trades -----------------------------------------------------------------------
function lines(body) { return new Response(body, { headers: { "content-type": "text/plain; charset=utf-8", "cache-control": "no-store" } }); }
const clean = (s) => String(s || "").replace(/[\t\r\n]/g, " ");
async function users(request, env) {
  const s = await session(request, env);
  if (!s) return text("not signed in", 401);
  const r = await env.DB.prepare("SELECT id, name, email FROM users WHERE id != ?1 ORDER BY name, email").bind(s.id).all();
  return lines(r.results.map((u) => `${u.id}\t${clean(u.name)}\t${clean(u.email)}`).join("\n") + "\n");
}
// A side's Pokémon as sent: N lines of "M<TAB>gen<TAB>base64<TAB>place", each checked.
function parseMons(rows, want) {
  const mons = [];
  for (const l of rows) {
    const f = l.split("\t");
    if (f[0] !== "M") continue;
    const gen = Number(f[1]), data = f[2] || "", place = f[3] || "";
    if (!(gen >= 1 && gen <= 7) || !/^[A-Za-z0-9+/=]{20,600}$/.test(data) || place.length > 800 || /[\t\r\n]/.test(place)) return null;
    mons.push(`${gen}\t${data}\t${place}`);
  }
  if (mons.length < 1 || mons.length > 6 || (want && mons.length !== want)) return null;
  return mons;
}
async function trades(request, url, env) {
  const s = await session(request, env);
  if (!s) return text("not signed in", 401);
  const now = Math.floor(Date.now() / 1000);
  if (url.pathname === "/trades" && request.method === "GET") {
    const r = await env.DB.prepare(
      "SELECT t.*, ua.name AS a_name, ua.email AS a_email, ub.name AS b_name, ub.email AS b_email FROM trades t " +
      "JOIN users ua ON ua.id = t.a_user JOIN users ub ON ub.id = t.b_user " +
      "WHERE (t.a_user = ?1 AND t.a_settled = 0) OR (t.b_user = ?1 AND t.b_settled = 0) ORDER BY t.updated DESC LIMIT 50"
    ).bind(s.id).all();
    let out = "";
    for (const t of r.results) {
      const mine = t.a_user === s.id ? "a" : "b";
      const other = mine === "a" ? { name: t.b_name, email: t.b_email } : { name: t.a_name, email: t.a_email };
      out += `T\t${t.id}\t${mine}\t${t.state}\t${t.count}\t${clean(other.name)}\t${clean(other.email)}\t${t.b_pick}\t${t.a_pick}\t${t.updated}\t${t.cancelled_by === s.id ? 1 : 0}\n`;
      // (A's are seen by both; B's by A once B has put them on the table. A place only to its owner.)
      const side = (letter, text, owner) => (text ? text.split("\n") : []).forEach((m, i) => {
        const [gen, data, place] = m.split("\t");
        out += `${letter}\t${i}\t${gen}\t${data}\t${owner === s.id ? place || "" : ""}\n`;
      });
      side("A", t.a_mons, t.a_user);
      if (t.state !== "offered" || mine === "b") side("B", t.b_mons, t.b_user);
    }
    return lines(out);
  }
  const rows = (await request.text()).split("\n");
  if (url.pathname === "/trades" && request.method === "POST") {
    const to = (rows[0] || "").split("\t");
    if (to[0] !== "to" || !to[1] || to[1] === s.id) return text("bad partner", 400);
    if (!(await env.DB.prepare("SELECT id FROM users WHERE id = ?1").bind(to[1]).first())) return text("no such player", 404);
    const mons = parseMons(rows, 0);
    if (!mons) return text("bad Pokémon", 400);
    const id = randomHex(12);
    await env.DB.prepare("INSERT INTO trades (id, a_user, b_user, state, count, a_mons, created, updated) VALUES (?1, ?2, ?3, 'offered', ?4, ?5, ?6, ?6)")
      .bind(id, s.id, to[1], mons.length, mons.join("\n"), now).run();
    return lines(id + "\n");
  }
  const m = url.pathname.match(/^\/trades\/([0-9a-f]{24})\/(counter|finish|cancel|settled)$/);
  if (!m || request.method !== "POST") return text("Not found", 404);
  const t = await env.DB.prepare("SELECT * FROM trades WHERE id = ?1").bind(m[1]).first();
  if (!t || (t.a_user !== s.id && t.b_user !== s.id)) return text("no such trade", 404);
  const isA = t.a_user === s.id;
  const pick = Number(((rows[0] || "").split("\t"))[1]);
  if (m[2] === "counter") {
    if (isA || t.state !== "offered") return text("not now", 409);
    if (!(pick >= 0 && pick < t.count)) return text("bad pick", 400);
    const mons = parseMons(rows, t.count);
    if (!mons) return text(`put ${t.count} on the table`, 400);
    await env.DB.prepare("UPDATE trades SET state = 'countered', b_mons = ?1, b_pick = ?2, updated = ?3 WHERE id = ?4 AND state = 'offered'")
      .bind(mons.join("\n"), pick, now, t.id).run();
  } else if (m[2] === "finish") {
    if (!isA || t.state !== "countered") return text("not now", 409);
    if (!(pick >= 0 && pick < t.count)) return text("bad pick", 400);
    await env.DB.prepare("UPDATE trades SET state = 'done', a_pick = ?1, updated = ?2 WHERE id = ?3 AND state = 'countered'").bind(pick, now, t.id).run();
  } else if (m[2] === "cancel") {
    if (t.state !== "offered" && t.state !== "countered") return text("not now", 409);
    await env.DB.prepare("UPDATE trades SET state = 'cancelled', cancelled_by = ?1, updated = ?2 WHERE id = ?3 AND state IN ('offered', 'countered')").bind(s.id, now, t.id).run();
  } else {
    // settled: this side's app has its Pokémon back, or the one it got. Both: the trade is gone.
    if (t.state !== "done" && t.state !== "cancelled") return text("not now", 409);
    await env.DB.prepare(`UPDATE trades SET ${isA ? "a_settled" : "b_settled"} = 1 WHERE id = ?1`).bind(t.id).run();
    await env.DB.prepare("DELETE FROM trades WHERE id = ?1 AND a_settled = 1 AND b_settled = 1").bind(t.id).run();
  }
  return lines("ok\n");
}
