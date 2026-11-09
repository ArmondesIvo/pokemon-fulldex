-- PokeHub's server: its D1 database (tools/pokehub/server/deploy.sh makes it once).
CREATE TABLE IF NOT EXISTS users (
  id      TEXT PRIMARY KEY,   -- Google's account id (the ID token's "sub")
  email   TEXT NOT NULL,
  name    TEXT NOT NULL DEFAULT '',
  created INTEGER NOT NULL,
  seen    INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS sessions (
  hash    TEXT PRIMARY KEY,   -- SHA-256 of the token the app keeps; the token itself is never kept here
  user_id TEXT NOT NULL REFERENCES users(id),
  created INTEGER NOT NULL,
  expires INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS sessions_user ON sessions(user_id);
-- Online trades (worker.js, /trades): A offers N of theirs (1 to 6), B wants one of them and puts N
-- of theirs on the table, A takes one of those. Each side's Pokémon are kept here (their PKHeX
-- form, base64) while it is open, out of their saves; "place" says where each came from, for its
-- owner's app to put it back. Lines of "gen<TAB>data<TAB>place".
CREATE TABLE IF NOT EXISTS trades (
  id           TEXT PRIMARY KEY,
  a_user       TEXT NOT NULL REFERENCES users(id),
  b_user       TEXT NOT NULL REFERENCES users(id),
  state        TEXT NOT NULL,          -- offered, countered, done, cancelled
  count        INTEGER NOT NULL,
  a_mons       TEXT NOT NULL,
  b_mons       TEXT NOT NULL DEFAULT '',
  b_pick       INTEGER NOT NULL DEFAULT -1,  -- the one of A's that B wants
  a_pick       INTEGER NOT NULL DEFAULT -1,  -- the one of B's that A takes
  a_settled    INTEGER NOT NULL DEFAULT 0,   -- each side's app has put its Pokémon where they go
  b_settled    INTEGER NOT NULL DEFAULT 0,
  cancelled_by TEXT NOT NULL DEFAULT '',
  created      INTEGER NOT NULL,
  updated      INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS trades_a ON trades(a_user);
CREATE INDEX IF NOT EXISTS trades_b ON trades(b_user);
