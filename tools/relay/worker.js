// FULLDEX TRADE RELAY (Cloudflare Worker): two devices that typed the same short code pass the
// launcher's trade messages (hub_link.cpp) to each other. Each device asks every second or so:
//
//   POST /v1/<CODE>/<device id>    body: its messages since last time (may be empty)
//   reply: one byte, how many devices are in the room (1 or 2), then the other's new messages
//
// A room holds two devices; a third is turned away (409). Nothing is kept: a room is forgotten
// 15 minutes after anyone last asked. The messages are just the launcher's (a Pokémon's bytes and a
// few words); the relay does not read them.
const MAX_BODY = 64 * 1024, MAX_ROOM = 512 * 1024, FORGET_MS = 15 * 60 * 1000;

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (url.pathname === "/") return new Response("Fulldex trade relay\n");
    const m = url.pathname.match(/^\/v1\/([A-Z0-9]{4,12})\/([a-z0-9]{8,32})$/);
    if (!m || request.method !== "POST") return new Response("not found\n", { status: 404 });
    return env.ROOMS.get(env.ROOMS.idFromName(m[1])).fetch(request);
  },
};

function join(a, b) {
  const out = new Uint8Array(a.length + b.length);
  out.set(a, 0);
  out.set(b, a.length);
  return out;
}

export class Room {
  constructor(state) {
    this.state = state;
  }
  async fetch(request) {
    const me = new URL(request.url).pathname.split("/")[3];
    const s = this.state.storage;
    const body = new Uint8Array(await request.arrayBuffer());
    if (body.length > MAX_BODY) return new Response("too big\n", { status: 413 });
    let members = (await s.get("members")) || [];
    if (!members.includes(me)) {
      if (members.length >= 2) return new Response("room full\n", { status: 409 });
      members.push(me);
      await s.put("members", members);
    }
    // What each device has said (out:<id>), and how much of the other's each has read (read:<id>).
    let mine = (await s.get("out:" + me)) || new Uint8Array(0);
    if (body.length) {
      if (mine.length + body.length > MAX_ROOM) return new Response("room full\n", { status: 413 });
      mine = join(mine, body);
      await s.put("out:" + me, mine);
    }
    const other = members.find((x) => x !== me);
    let news = new Uint8Array(0);
    if (other) {
      const theirs = (await s.get("out:" + other)) || new Uint8Array(0);
      const read = (await s.get("read:" + me)) || 0;
      news = theirs.slice(read);
      await s.put("read:" + me, theirs.length);
    }
    await s.setAlarm(Date.now() + FORGET_MS);
    return new Response(join(new Uint8Array([members.length]), news), {
      headers: { "Content-Type": "application/octet-stream", "Cache-Control": "no-store" },
    });
  }
  async alarm() {
    await this.state.storage.deleteAll();
  }
}
