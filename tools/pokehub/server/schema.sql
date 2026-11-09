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
