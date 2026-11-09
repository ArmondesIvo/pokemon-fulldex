#pragma once
// ONLINE TRADES (PokeHub's, through its server: tools/pokehub/server/worker.js, /trades).
//  1. A chooses 1 to 6 of theirs (My PC's choosing, Pc().pick) and a player: they go on the table.
//  2. B sees them (the bell on the top bar), wants one, and puts 1 to 6 of theirs on the table.
//  3. A takes one of B's: done. Either may call it off before.
// What is on the table is out of its owner's saves meanwhile (kept on the server), so nothing is
// ever in two places. At the end, each side's app (on its next look) puts the one it got in the
// Bank and its own others back where they were (or in the Bank, when that place is taken), notes
// each one done in a file of its own (so a second look never puts one back twice), then tells the
// server. A done trade is shown with a little scene, on the side that finished it at once, on the
// other at its next start.
// Drawn in PKVault's look, as My PC: HubMode 8, from the bell.
// (Included inside namespace Launcher, by launcher.h.)
#ifdef SSR_POKEMON_HUB
// (The trades' page: My PC's colours, PokeHub's green.)
namespace TradePal = PcPal;
inline void TradePanel(Canvas& c, int x, int y, int w, int h, float r, float u) { Glass::Round(c, x, y, w, h, r, TradePal::Card, TradePal::Line, 1.5f * u); }
inline void TradeButton(Canvas& c, int x, int y, int w, int h, float r, bool on, float u)
{
    if (on) Glass::Round(c, x, y, w, h, r, TradePal::Primary, TradePal::PrimaryDark, 1.2f * u);
    else Glass::Round(c, x, y, w, h, r, TradePal::Btn, TradePal::Line, 1.0f * u);
}
struct TradeMon { PokemonHub::Packed p; std::string place; bool ok = false; };
struct OnlineTrade
{
    std::string id, role, state, partner, partnerEmail; // role "a" (started it) or "b"
    int count = 0, bPick = -1, aPick = -1;
    long long updated = 0;
    bool cancelledByMe = false;
    std::vector<TradeMon> a, b;
    const std::vector<TradeMon>& Mine() const { return role == "a" ? a : b; }
    const std::vector<TradeMon>& Theirs() const { return role == "a" ? b : a; }
};
struct TradesState
{
    std::mutex m;
    std::vector<OnlineTrade> list;
    std::vector<std::pair<std::string, std::string>> users; // id, name
    std::atomic<bool> busy{ false };
    std::string msg;
    std::string selId;   // the trade shown
    int card = -1;       // a card tapped (theirs 0..5, mine 100..105): its details
    std::thread worker;
    // (The jobs, one after another: a send while the list is fetched waits its turn, never dropped.)
    std::deque<std::function<void()>> jobs;
    bool running = false, fetchQueued = false;
    long long lastFetch = 0;
    // (Choosing for one: the player to send to, or the trade answered and the one of theirs wanted.)
    std::string toId, toName, counterId;
    int want = -1;
    // (The scene of a done trade.)
    struct Anim { bool on = false; int give = 0, get = 0; bool giveShiny = false, getShiny = false; std::string giveName, getName, partner; std::chrono::steady_clock::time_point at; } anim;
    std::vector<PcHit> hits;
    float u = 1;
};
inline TradesState& Trades() { static TradesState t; return t; }
enum { TR_NEW = 1, TR_ITEM, TR_CARD, TR_WANT, TR_DECLINE, TR_FINISH, TR_CANCEL };

inline std::string TradeB64(const std::string& d)
{
    static const char* k = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    size_t i = 0;
    for (; i + 2 < d.size(); i += 3)
    {
        const uint32_t v = (uint8_t)d[i] << 16 | (uint8_t)d[i + 1] << 8 | (uint8_t)d[i + 2];
        o += k[v >> 18]; o += k[(v >> 12) & 63]; o += k[(v >> 6) & 63]; o += k[v & 63];
    }
    if (i + 1 == d.size()) { const uint32_t v = (uint8_t)d[i] << 16; o += k[v >> 18]; o += k[(v >> 12) & 63]; o += "=="; }
    else if (i + 2 == d.size()) { const uint32_t v = (uint8_t)d[i] << 16 | (uint8_t)d[i + 1] << 8; o += k[v >> 18]; o += k[(v >> 12) & 63]; o += k[(v >> 6) & 63]; o += '='; }
    return o;
}
inline std::string TradeUnB64(const std::string& s)
{
    auto val = [](char c) { return c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52 : c == '+' ? 62 : c == '/' ? 63 : -1; };
    std::string o;
    uint32_t acc = 0;
    int bits = 0;
    for (char c : s)
    {
        const int v = val(c);
        if (v < 0) continue;
        acc = acc << 6 | (uint32_t)v; bits += 6;
        if (bits >= 8) { bits -= 8; o += (char)((acc >> bits) & 0xFF); }
    }
    return o;
}
inline std::vector<std::string> TradeSplit(const std::string& s, char by)
{
    std::vector<std::string> out;
    size_t at = 0;
    for (;;) { const size_t e = s.find(by, at); out.push_back(s.substr(at, e == std::string::npos ? std::string::npos : e - at)); if (e == std::string::npos) break; at = e + 1; }
    return out;
}
// One call to the server, as the signed-in player. False: no answer (or not signed in).
inline bool TradeCall(const std::string& method, const std::string& path, const std::string& body, int& status, std::string& out)
{
    const std::string sess = CloudSaves::PokeHubSession() ? CloudSaves::PokeHubSession()() : std::string();
    const size_t t = sess.find('\t');
    if (t == std::string::npos) { out = "Sign in first (the figure at the top)."; return false; }
    Achievements::HttpCall c;
    c.method = method;
    c.url = sess.substr(0, t) + path;
    c.headers.push_back("Authorization: Bearer " + sess.substr(t + 1));
    c.headers.push_back("Content-Type: text/plain; charset=utf-8");
    c.body = body;
    c.timeout = 20;
    c.userAgent = "PokeHub";
    std::string err;
    if (!Achievements::HttpRequest(c, status, out, err)) { out = "No connection to PokeHub's server."; return false; }
    return true;
}

// (Each Pokémon put away, by "trade:side:index": never twice, whatever a look is cut short by.)
inline std::string TradesDoneFile() { return HubDir() + "/pokehub-trades-done.txt"; }
inline bool TradeItemDone(const std::string& key)
{
    std::ifstream f(TradesDoneFile());
    std::string l;
    while (std::getline(f, l)) if (l == key) return true;
    return false;
}
inline void TradeItemMark(const std::string& key)
{
    std::ofstream f(TradesDoneFile(), std::ios::app);
    f << key << "\n";
}
// A save by its path (a game's, or the Bank's folder), opened.
inline std::unique_ptr<PokemonHub::Save> TradeOpen(const std::string& path, std::string& err)
{
    for (const MoverGame& g : MoverGames()) if (g.path == path) return OpenMoverGame(g, err);
    err = "That game is not in your list any more.";
    return nullptr;
}
// One of the player's own put back where it was ("path|box|slot"), or into the Bank when that place
// is taken or gone; or the one they got, into the Bank.
inline bool TradeStore(const TradeMon& tm, bool toBank, std::string& err)
{
    std::string path = BankDir();
    int box = PokemonHub::ANY_BOX, slot = -1;
    if (!toBank)
    {
        const std::vector<std::string> f = TradeSplit(TradeUnB64(tm.place), '|');
        if (f.size() == 3) { path = f[0]; box = atoi(f[1].c_str()); slot = atoi(f[2].c_str()); }
    }
    std::unique_ptr<PokemonHub::Save> s = TradeOpen(path, err);
    if (s && box != PokemonHub::ANY_BOX)
    {
        // (Its place taken since: the Bank instead.)
        bool taken = false;
        for (const PokemonHub::Mon& m : s->Box(box)) taken = taken || m.slot == slot;
        if (taken || box < 0 || box >= s->Boxes()) { s.reset(); path = BankDir(); box = PokemonHub::ANY_BOX; slot = -1; }
    }
    if (!s) { s = TradeOpen(BankDir(), err); box = PokemonHub::ANY_BOX; slot = -1; }
    if (!s) return false;
    if (!PokemonHub::Receive(*s, tm.p, false, 0, err, box, slot)) return false;
    return PokemonHub::Commit(*s, err);
}

// A finished trade put away on this side: what it got in the Bank, its own others back. True when
// all of it is (then the server is told).
inline bool TradeSettle(const OnlineTrade& t)
{
    const std::vector<TradeMon>& mine = t.Mine();
    const std::vector<TradeMon>& theirs = t.Theirs();
    const bool done = t.state == "done";
    const int myGiven = t.role == "a" ? t.bPick : t.aPick, theirGiven = t.role == "a" ? t.aPick : t.bPick;
    std::string err;
    bool ok = true;
    // (Its own back first, to their places, then the one it got: that never takes a place one of
    // its own was going back to.)
    for (int i = 0; i < (int)mine.size(); i++)
    {
        if (done && i == myGiven) continue;
        const std::string key = t.id + ":back:" + std::to_string(i);
        if (TradeItemDone(key)) continue;
        if (TradeStore(mine[(size_t)i], false, err)) TradeItemMark(key); else ok = false;
    }
    if (done && theirGiven >= 0 && theirGiven < (int)theirs.size())
    {
        const std::string key = t.id + ":get";
        if (!TradeItemDone(key)) { if (TradeStore(theirs[(size_t)theirGiven], true, err)) TradeItemMark(key); else ok = false; }
    }
    if (!ok) { std::lock_guard<std::mutex> l(Trades().m); Trades().msg = "Could not put a Pokémon away: " + err; return false; }
    int status = 0;
    std::string out;
    TradeCall("POST", "/trades/" + t.id + "/settled", "", status, out);
    ImportedSaves()++; // (My PC reads its saves again)
    if (done && myGiven >= 0 && myGiven < (int)mine.size() && theirGiven >= 0 && theirGiven < (int)theirs.size())
    {
        TradesState& T = Trades();
        std::lock_guard<std::mutex> l(T.m);
        T.anim.on = true;
        T.anim.give = mine[(size_t)myGiven].p.mon.species; T.anim.giveShiny = mine[(size_t)myGiven].p.mon.shiny;
        T.anim.get = theirs[(size_t)theirGiven].p.mon.species; T.anim.getShiny = theirs[(size_t)theirGiven].p.mon.shiny;
        T.anim.giveName = PcMonName(mine[(size_t)myGiven].p.mon); T.anim.getName = PcMonName(theirs[(size_t)theirGiven].p.mon);
        T.anim.partner = t.partner.empty() ? t.partnerEmail : t.partner;
        T.anim.at = std::chrono::steady_clock::now();
    }
    else if (!done)
    {
        std::lock_guard<std::mutex> l(Trades().m);
        Trades().msg = t.cancelledByMe ? "Trade called off. Your Pokémon are back." : (t.partner.empty() ? t.partnerEmail : t.partner) + " called the trade off. Your Pokémon are back.";
    }
    return true;
}

// The trades as the server has them, and the finished ones put away. (On a thread of its own.)
inline void TradesFetchJob()
{
    TradesState& T = Trades();
    int status = 0;
    std::string body;
    if (!TradeCall("GET", "/trades", "", status, body) || status != 200)
    {
        std::lock_guard<std::mutex> l(T.m);
        if (status == 401) T.msg = "Sign in again (the figure at the top).";
        T.busy = false;
        Hub().version++;
        return;
    }
    std::vector<OnlineTrade> list;
    for (const std::string& line : TradeSplit(body, '\n'))
    {
        const std::vector<std::string> f = TradeSplit(line, '\t');
        if (f[0] == "T" && f.size() >= 11)
        {
            OnlineTrade t;
            t.id = f[1]; t.role = f[2]; t.state = f[3]; t.count = atoi(f[4].c_str()); t.partner = f[5]; t.partnerEmail = f[6];
            t.bPick = atoi(f[7].c_str()); t.aPick = atoi(f[8].c_str()); t.updated = atoll(f[9].c_str()); t.cancelledByMe = f[10] == "1";
            list.push_back(t);
        }
        else if ((f[0] == "A" || f[0] == "B") && f.size() >= 5 && !list.empty())
        {
            TradeMon tm;
            const std::string raw = TradeUnB64(f[3]);
            tm.ok = PokemonHub::PackedFrom(atoi(f[2].c_str()), std::vector<uint8_t>(raw.begin(), raw.end()), tm.p);
            tm.place = f[4];
            (f[0] == "A" ? list.back().a : list.back().b).push_back(tm);
        }
    }
    // (Finished ones put away, unless My PC has changes not saved: they would be read over.)
    std::vector<OnlineTrade> open;
    for (const OnlineTrade& t : list)
    {
        if ((t.state == "done" || t.state == "cancelled") && Pc().pending == 0) { TradeSettle(t); continue; }
        open.push_back(t);
    }
    std::lock_guard<std::mutex> l(T.m);
    T.list = open;
    T.busy = false;
    T.lastFetch = (long long)time(nullptr);
    Hub().version++;
}
// A job for the server, queued: run after those before it, on the trades' own thread. (Before,
// a job asked for while another ran was dropped: a trade sent as the list was being fetched never
// went, the page showing "Sending..." and nothing after.)
inline void TradesRun(std::function<void()> job, bool fetch = false)
{
    TradesState& T = Trades();
    std::lock_guard<std::mutex> l(T.m);
    if (fetch && T.fetchQueued) return; // (a look already waiting: one is enough)
    if (fetch) T.fetchQueued = true;
    T.jobs.push_back([job, fetch] { if (fetch) { std::lock_guard<std::mutex> g(Trades().m); Trades().fetchQueued = false; } job(); });
    T.busy = true;
    if (T.running) return;
    T.running = true;
    if (T.worker.joinable()) T.worker.join();
    T.worker = std::thread([] {
        TradesState& S = Trades();
        for (;;)
        {
            std::function<void()> next;
            {
                std::lock_guard<std::mutex> g(S.m);
                if (S.jobs.empty()) { S.running = false; S.busy = false; Hub().version++; return; }
                next = std::move(S.jobs.front());
                S.jobs.pop_front();
                S.busy = true;
            }
            next();
        }
    });
}
inline void TradesFetch() { if (Account().signedIn) TradesRun([] { TradesFetchJob(); }, true); }
// What waits on this player: the bell's dot.
inline int TradesWaiting()
{
    TradesState& T = Trades();
    std::lock_guard<std::mutex> l(T.m);
    int n = 0;
    for (const OnlineTrade& t : T.list) n += (t.role == "b" && t.state == "offered") || (t.role == "a" && t.state == "countered");
    return n;
}

// Taken out of the player's saves and put on the table (a new trade, or an answer): out first,
// then sent; if the server does not take it, put back at once.
inline void TradesSendJob(std::vector<std::tuple<std::string, int, int>> chosen, std::string to, std::string counterId, int want)
{
    TradesState& T = Trades();
    auto say = [&](const std::string& m) { std::lock_guard<std::mutex> l(T.m); T.msg = m; T.busy = false; Hub().version++; };
    std::vector<TradeMon> out;
    std::string err;
    for (const auto& c : chosen)
    {
        TradeMon tm;
        std::unique_ptr<PokemonHub::Save> s = TradeOpen(std::get<0>(c), err);
        if (!s || !PokemonHub::Pack(*s, std::get<1>(c), std::get<2>(c), tm.p, err)) { say("Could not take it: " + err); return; }
        tm.place = TradeB64(std::get<0>(c) + "|" + std::to_string(std::get<1>(c)) + "|" + std::to_string(std::get<2>(c)));
        out.push_back(tm);
    }
    // (Out of the saves, a save at a time, each backed up by Commit.)
    std::vector<size_t> taken;
    auto putBack = [&]() { std::string e; for (size_t i : taken) TradeStore(out[i], false, e); ImportedSaves()++; };
    for (size_t i = 0; i < chosen.size(); i++)
    {
        std::unique_ptr<PokemonHub::Save> s = TradeOpen(std::get<0>(chosen[i]), err);
        if (!s || !PokemonHub::Remove(*s, std::get<1>(chosen[i]), std::get<2>(chosen[i]), err) || !PokemonHub::Commit(*s, err)) { putBack(); say("Could not take it out of its save: " + err); return; }
        taken.push_back(i);
    }
    std::string body = counterId.empty() ? "to\t" + to + "\n" : "pick\t" + std::to_string(want) + "\n";
    for (const TradeMon& tm : out) body += "M\t" + std::to_string(tm.p.generation) + "\t" + TradeB64(std::string(tm.p.data.begin(), tm.p.data.end())) + "\t" + tm.place + "\n";
    int status = 0;
    std::string reply;
    const bool sent = TradeCall("POST", counterId.empty() ? "/trades" : "/trades/" + counterId + "/counter", body, status, reply) && status >= 200 && status < 300;
    if (!sent) { putBack(); say("The trade was not sent (" + (status ? std::to_string(status) + " " + reply : reply) + "). Your Pokémon are back."); return; }
    ImportedSaves()++;
    { std::lock_guard<std::mutex> l(T.m); T.msg = counterId.empty() ? "Sent. You will see here when they answer." : "Sent. Now they choose one of yours."; }
    T.busy = false;
    TradesFetchJob();
}
inline void TradesActJob(std::string id, std::string what, int pick)
{
    int status = 0;
    std::string reply;
    const bool ok = TradeCall("POST", "/trades/" + id + "/" + what, what == "finish" ? "pick\t" + std::to_string(pick) + "\n" : std::string(), status, reply) && status >= 200 && status < 300;
    if (!ok) { std::lock_guard<std::mutex> l(Trades().m); Trades().msg = "PokeHub's server did not take it: " + reply; }
    Trades().busy = false;
    TradesFetchJob();
}

// The bell: the trades' page.
inline void OpenTrades()
{
    GlassFocus& F = Focus();
    OpenSide(SIDE_HUB);
    HubMode() = 8;
    F.prow = 0; F.pact = -1; F.rScroll = 0;
    Trades().card = -1;
    TradesFetch();
    Hub().version++;
}
// New Trade: the players to choose from (the dialog, then My PC to choose what to offer).
inline void TradesUsersJob()
{
    int status = 0;
    std::string body;
    std::vector<std::pair<std::string, std::string>> users;
    if (TradeCall("GET", "/users", "", status, body) && status == 200)
        for (const std::string& line : TradeSplit(body, '\n'))
        {
            const std::vector<std::string> f = TradeSplit(line, '\t');
            if (f.size() >= 3 && !f[0].empty()) users.push_back({ f[0], f[1].empty() ? f[2] : f[1] });
        }
    std::lock_guard<std::mutex> l(Trades().m);
    Trades().users = users;
    Trades().busy = false;
    if (users.empty()) Trades().msg = status == 200 ? "No one else uses PokeHub yet." : "Could not reach PokeHub's server.";
    Hub().version++;
}
inline void OpenTradePartnerAsk()
{
    GlassFocus& F = Focus();
    TradesState& T = Trades();
    std::lock_guard<std::mutex> l(T.m);
    F.dialog = 12;
    F.drow = 0;
    F.dTitle = "New Trade";
    F.dItems.clear(); F.dPaths.clear();
    for (const auto& u : T.users) { F.dItems.push_back(u.second); F.dPaths.push_back("#tradeto:" + u.first); }
    F.dMsg = T.users.empty() ? "No one else uses PokeHub yet." : "Who do you want to trade with?";
    F.dItems.push_back("Cancel"); F.dPaths.push_back("#close");
}
// The player chosen: My PC, to choose 1 to 6 to offer.
inline void TradesStartNew(const std::string& id)
{
    TradesState& T = Trades();
    std::string name;
    { std::lock_guard<std::mutex> l(T.m); for (const auto& u : T.users) if (u.first == id) name = u.second; }
    if (Pc().pending) { std::lock_guard<std::mutex> l(T.m); T.msg = "Save or Undo All in My PC first."; return; }
    T.toId = id; T.toName = name; T.counterId.clear(); T.want = -1;
    OpenMyPc();
    PcState& P = Pc();
    P.pick = PcState::Pick();
    P.pick.on = true; P.pick.need = 0;
    P.pick.title = LauncherText::Tr("Choose up to 6 to offer") + " \xC2\xB7 " + name;
    P.msg.clear();
}
inline void TradesStartCounter(const OnlineTrade& t, int want)
{
    TradesState& T = Trades();
    if (Pc().pending) { std::lock_guard<std::mutex> l(T.m); T.msg = "Save or Undo All in My PC first."; return; }
    T.toId.clear(); T.counterId = t.id; T.want = want;
    OpenMyPc();
    PcState& P = Pc();
    P.pick = PcState::Pick();
    // (As many as they like, 1 to 6, whatever the other offered: the other then takes one of them.)
    P.pick.on = true; P.pick.need = 0;
    P.pick.title = LauncherText::Tr("Choose up to 6 to offer") + " \xC2\xB7 " + (t.partner.empty() ? t.partnerEmail : t.partner);
    P.msg.clear();
}
inline void TradesPicked(bool send)
{
    PcState& P = Pc();
    TradesState& T = Trades();
    const std::vector<std::tuple<std::string, int, int>> chosen = P.pick.chosen;
    P.pick = PcState::Pick();
    P.selSide = -1;
    OpenTrades();
    if (!send) return;
    const std::string to = T.toId, counter = T.counterId;
    const int want = T.want;
    { std::lock_guard<std::mutex> l(T.m); T.msg = "Sending..."; }
    TradesRun([chosen, to, counter, want] { TradesSendJob(chosen, to, counter, want); });
}

// ---- Drawing: PKVault's look ---------------------------------------------------------------
inline std::string TradeStatus(const OnlineTrade& t)
{
    const std::string who = t.partner.empty() ? t.partnerEmail : t.partner;
    if (t.role == "b" && t.state == "offered") return LauncherText::Tr("Your turn:") + " " + who + " " + LauncherText::Tr("offers") + " " + std::to_string(t.count);
    if (t.role == "a" && t.state == "countered") return LauncherText::Tr("Your turn: choose one of theirs");
    if (t.role == "a" && t.state == "offered") return LauncherText::Tr("Waiting for") + " " + who;
    if (t.role == "b" && t.state == "countered") return LauncherText::Tr("Waiting for") + " " + who + " " + LauncherText::Tr("to choose");
    return LauncherText::Tr("Finishing...");
}
// A Pokémon's card: its picture, level, origin mark; picked or wanted, framed.
inline void TradeCard(Canvas& c, float u, int x, int y, int w, int h, const TradeMon& tm, bool chosen, const std::string& tag)
{
    Glass::Round(c, x, y, w, h, 8 * u, chosen ? TradePal::Body : TradePal::Btn, chosen ? TradePal::Primary : TradePal::Line, chosen ? 3.0f * u : 1.0f * u);
    if (!tm.ok) { GT(c, x + (int)(8 * u), y + h / 2.0f, "?", 14 * u, TradePal::Hint, true); return; }
    PcDrawMon(c, tm.p.mon.species, tm.p.mon.shiny, false, x + (int)(6 * u), y + (int)(6 * u), w - (int)(12 * u), h - (int)(24 * u));
    const std::string lv = "Lv." + std::to_string(tm.p.mon.level);
    GT(c, x + w - (int)(6 * u) - GW(lv, 10 * u, true), y + h - 9 * u, lv, 10 * u, TradePal::Ink, true);
    if (tm.p.mon.originMark) PcDrawOriginMark(c, tm.p.mon.originMark, x + 9 * u, y + 10 * u, 9 * u, TradePal::Hint, TradePal::Body);
    if (tm.p.mon.shiny) Glass::DrawIcon(c, Glass::I_SPARKLE, x + w - 9 * u, y + 9 * u, 5 * u, TradePal::Shiny);
    if (!tag.empty())
    {
        const int tw = GW(tag, 9.5f * u, true) + (int)(10 * u);
        Glass::Round(c, x + (w - tw) / 2, y - (int)(8 * u), tw, (int)(16 * u), 8 * u, TradePal::Primary);
        GT(c, x + (w - tw) / 2 + (int)(5 * u), y, tag, 9.5f * u, TradePal::OnPrimary, true);
    }
}
// A Pokémon's details, as My PC shows them: who, where it came from, its stats and moves.
inline void TradeDetail(Canvas& c, float u, int x, int y, int w, int h, const TradeMon& tm)
{
    TradePanel(c, x, y, w, h, 10 * u, u);
    PokemonHub::MonDetail d;
    if (!tm.ok || !PokemonHub::PackedDetail(tm.p, d)) { GT(c, x + (int)(12 * u), y + 20 * u, "?", 14 * u, TradePal::Hint, true); return; }
    const int pad = (int)(12 * u), ps = std::min(h - 2 * pad, (int)(w * 0.3f));
    PcDrawMon(c, d.mon.species, d.mon.shiny, false, x + pad, y + pad, ps, ps, 0, true);
    int tx = x + pad + ps + (int)(12 * u), ty = y + pad;
    const int tw = x + w - pad - tx;
    const std::string nm = PcMonName(d.mon);
    GTFit(c, tx, ty + 10 * u, nm, 16 * u, tw - (int)(60 * u), TradePal::Ink, true);
    if (!d.gender.empty()) GT(c, tx + std::min(GW(nm, 16 * u, true), tw - (int)(60 * u)) + (int)(8 * u), ty + 10 * u, d.gender, 13 * u, d.gender == "M" ? TradePal::Male : TradePal::Female, true);
    const std::string lv = "Lv." + std::to_string(d.mon.level);
    GT(c, x + w - pad - GW(lv, 13 * u, true), ty + 10 * u, lv, 13 * u, TradePal::Ink, true);
    ty += (int)(22 * u);
    int cx = tx;
    for (const std::string& t : { d.type1, d.type2 })
    {
        if (t.empty()) continue;
        const int cw = GW(t, 9.5f * u, true) + (int)(12 * u);
        Glass::Round(c, cx, ty, cw, (int)(17 * u), 6 * u, PcTypeColour(t));
        GT(c, cx + (int)(6 * u), ty + 8.5f * u, t, 9.5f * u, 0xFFFFFFFFu, true);
        cx += cw + (int)(5 * u);
    }
    if (d.mon.originMark) PcDrawOriginMark(c, d.mon.originMark, cx + 8 * u, ty + 8.5f * u, 13 * u, TradePal::Ink, TradePal::Card);
    ty += (int)(24 * u);
    auto line = [&](const std::string& k, const std::string& v) {
        GT(c, tx, ty + 7 * u, k, 10.5f * u, TradePal::Hint, true);
        GTFit(c, tx + (int)(64 * u), ty + 7 * u, v.empty() ? std::string("-") : v, 11 * u, tw - (int)(64 * u), TradePal::Ink, true);
        ty += (int)(16 * u);
    };
    line("Origin", d.origin.empty() ? (d.mon.generation <= 2 ? "Game Boy (Gen " + std::to_string(d.mon.generation) + ")" : std::string()) : d.origin);
    if (d.mon.generation >= 3) { line("Nature", d.nature); line("Ability", d.ability); }
    line("Item", d.item);
    line("OT", d.ot + "  " + std::to_string(d.tid));
    // (Stats, with IVs and EVs under each, and the moves.)
    static const char* sn[6] = { "HP", "Atk", "Def", "Spe", "SpA", "SpD" };
    const int sw = tw / 6;
    for (int i = 0; i < 6; i++)
    {
        const int sx = tx + i * sw;
        GT(c, sx, ty + 7 * u, sn[i], 9.5f * u, TradePal::Stat[i], true);
        GT(c, sx, ty + 21 * u, std::to_string(d.stats[i]), 11 * u, TradePal::Ink, true);
        GT(c, sx, ty + 34 * u, std::to_string(d.ivs[i]) + "/" + std::to_string(d.evs[i]), 8.5f * u, TradePal::Hint, false);
    }
    ty += (int)(44 * u);
    std::string moves;
    for (const std::string& m : d.moves) if (!m.empty()) moves += (moves.empty() ? "" : " \xC2\xB7 ") + m;
    GTFit(c, tx, ty + 7 * u, moves, 10.5f * u, tw, TradePal::Ink, true);
}
inline void DrawTradesPage(Canvas& c, float u, int L, int R, int top, int bottom)
{
    TradesState& T = Trades();
    T.u = u;
    T.hits.clear();
    GlassFocus& F = Focus();
    F.rVis = 0; F.pdY = c.h; F.prow = 0; F.pact = -1;
    std::lock_guard<std::mutex> lk(T.m);
    Glass::Round(c, L, top, R - L, bottom - top, 12 * u, TradePal::Body, TradePal::Primary, 4 * u);
    { const int in = (int)(10 * u); L += in; R -= in; top += in; bottom -= in; }
    const int gap = (int)(10 * u), lw = std::min((int)(300 * u), (R - L) / 3);
    // The list: New Trade, then each trade.
    TradePanel(c, L, top, lw, bottom - top, 10 * u, u);
    int y = top + (int)(10 * u);
    {
        const int bw = lw - (int)(20 * u), bh = (int)(40 * u);
        TradeButton(c, L + (int)(10 * u), y, bw, bh, 8 * u, true, u);
        const std::string nt = LauncherText::Tr("+ New Trade");
        GT(c, L + (int)(10 * u) + (bw - GW(nt, 14 * u, true)) / 2, y + bh * 0.5f, nt, 14 * u, TradePal::OnPrimary, true);
        T.hits.push_back({ L + (int)(10 * u), y, bw, bh, TR_NEW, 0, 0 });
        y += bh + (int)(10 * u);
    }
    if (T.selId.empty() && !T.list.empty()) T.selId = T.list.front().id;
    const OnlineTrade* sel = nullptr;
    for (const OnlineTrade& t : T.list)
    {
        const int ih = (int)(58 * u);
        if (y + ih > bottom - (int)(10 * u)) break;
        const bool on = t.id == T.selId;
        if (on) sel = &t;
        Glass::Round(c, L + (int)(10 * u), y, lw - (int)(20 * u), ih, 8 * u, on ? TradePal::Body : TradePal::Btn, on ? TradePal::Primary : TradePal::Line, on ? 2.5f * u : 1.0f * u);
        const bool mine = (t.role == "b" && t.state == "offered") || (t.role == "a" && t.state == "countered");
        GTFit(c, L + (int)(22 * u), y + 18 * u, t.partner.empty() ? t.partnerEmail : t.partner, 14 * u, lw - (int)(60 * u), TradePal::Ink, true);
        GTFit(c, L + (int)(22 * u), y + 40 * u, TradeStatus(t), 11 * u, lw - (int)(44 * u), mine ? TradePal::Primary : TradePal::Hint, mine);
        if (mine) Glass::Round(c, L + lw - (int)(30 * u), y + (int)(12 * u), (int)(10 * u), (int)(10 * u), 5 * u, 0xFFE03131u);
        T.hits.push_back({ L + (int)(10 * u), y, lw - (int)(20 * u), ih, TR_ITEM, 0, (int)(&t - &T.list[0]) });
        y += ih + (int)(8 * u);
    }
    if (T.list.empty())
    {
        for (const std::string& l : WrapLines(LauncherText::Tr(T.busy ? "Looking..." : "No trades yet. Start one with New Trade."), 12 * u, lw - (int)(36 * u)))
        { GT(c, L + (int)(18 * u), y + 10 * u, l, 12 * u, TradePal::Hint, false); y += (int)(18 * u); }
    }
    // The trade chosen.
    const int x0 = L + lw + gap, w0 = R - x0;
    TradePanel(c, x0, top, w0, bottom - top, 10 * u, u);
    const int pad = (int)(14 * u);
    if (!T.msg.empty()) GTFit(c, x0 + pad, bottom - pad - 6 * u, T.msg, 11.5f * u, w0 - 2 * pad, TradePal::Hint, false);
    if (!sel)
    {
        GTFit(c, x0 + pad, top + pad + 12 * u, LauncherText::Tr("Trade your Pokémon with other PokeHub players: choose up to 6 to offer, they choose one and offer as many of theirs, you take one."), 13 * u, w0 - 2 * pad, TradePal::Ink, false);
        return;
    }
    const OnlineTrade& t = *sel;
    const std::string who = t.partner.empty() ? t.partnerEmail : t.partner;
    GT(c, x0 + pad, top + pad + 10 * u, LauncherText::Tr("Trade with") + " " + who, 17 * u, TradePal::Ink, true);
    GTFit(c, x0 + pad, top + pad + 32 * u, TradeStatus(t), 12 * u, w0 - 2 * pad, TradePal::Hint, true);
    int cy = top + pad + (int)(52 * u);
    const int cw = std::min((int)(84 * u), (w0 - 2 * pad - 5 * (int)(8 * u)) / 6), ch = (int)(cw * 0.95f);
    auto row = [&](const std::string& label, const std::vector<TradeMon>& mons, int base, int marked, const std::string& tag) {
        GT(c, x0 + pad, cy + 8 * u, label, 12 * u, TradePal::Hint, true);
        cy += (int)(22 * u);
        for (int i = 0; i < (int)mons.size(); i++)
        {
            const int cx = x0 + pad + i * (cw + (int)(8 * u));
            TradeCard(c, u, cx, cy, cw, ch, mons[(size_t)i], T.card == base + i || marked == i, marked == i ? tag : std::string());
            T.hits.push_back({ cx, cy, cw, ch, TR_CARD, 0, base + i });
        }
        cy += ch + (int)(14 * u);
    };
    const std::vector<TradeMon>& theirs = t.Theirs();
    const std::vector<TradeMon>& mine = t.Mine();
    if (!theirs.empty()) row(who + " " + LauncherText::Tr("offers"), theirs, 0, t.role == "a" ? -1 : -1, "");
    if (!mine.empty()) row(LauncherText::Tr("You offer"), mine, 100, t.role == "a" ? t.bPick : t.aPick, LauncherText::Tr(t.role == "a" ? "They want" : "Taken"));
    // The card tapped: its details; then what can be done.
    const int by = bottom - pad - (int)(40 * u) - (T.msg.empty() ? 0 : (int)(20 * u));
    const TradeMon* shown = T.card >= 100 ? (T.card - 100 < (int)mine.size() ? &mine[(size_t)(T.card - 100)] : nullptr) : (T.card >= 0 && T.card < (int)theirs.size() ? &theirs[(size_t)T.card] : nullptr);
    if (shown && by - cy > (int)(120 * u)) TradeDetail(c, u, x0 + pad, cy, w0 - 2 * pad, by - cy - (int)(10 * u), *shown);
    else if (!shown) GTFit(c, x0 + pad, cy + 10 * u, LauncherText::Tr("Tap a Pokémon to see it."), 12 * u, w0 - 2 * pad, TradePal::Hint, false);
    int bx = x0 + w0 - pad;
    auto button = [&](const std::string& label, int kind, bool strong) {
        const std::string tl = LauncherText::Tr(label);
        const int bw = GW(tl, 13 * u, true) + (int)(28 * u), bh = (int)(40 * u);
        bx -= bw;
        TradeButton(c, bx, by, bw, bh, 8 * u, strong, u);
        GT(c, bx + (int)(14 * u), by + bh * 0.5f, tl, 13 * u, strong ? TradePal::OnPrimary : TradePal::Ink, true);
        T.hits.push_back({ bx, by, bw, bh, kind, 0, 0 });
        bx -= (int)(10 * u);
    };
    if (T.busy) return;
    const bool pickedTheirs = T.card >= 0 && T.card < (int)theirs.size();
    if (t.role == "b" && t.state == "offered")
    {
        if (pickedTheirs) button("I Want This One", TR_WANT, true);
        button("Decline", TR_DECLINE, false);
    }
    else if (t.role == "a" && t.state == "countered")
    {
        if (pickedTheirs) button("Trade", TR_FINISH, true);
        button("Cancel Trade", TR_CANCEL, false);
    }
    else if (t.state == "offered" || t.state == "countered") button("Cancel Trade", TR_CANCEL, false);
}
inline void TradesPointer(int phase, int x, int y)
{
    if (phase != 2) return;
    TradesState& T = Trades();
    PcHit hit{};
    bool found = false;
    {
        std::lock_guard<std::mutex> lk(T.m);
        for (size_t i = T.hits.size(); i-- > 0;)
        {
            const PcHit& h = T.hits[i];
            if (x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h) { hit = h; found = true; break; }
        }
    }
    if (!found) return;
    if (hit.kind == TR_NEW)
    {
        if (!Account().signedIn) { std::lock_guard<std::mutex> lk(T.m); T.msg = "Sign in first (the figure at the top)."; Hub().version++; return; }
        // (The players, then the choice: the dialog opens once they are here.)
        TradesRun([] { TradesUsersJob(); Trades().busy = false; });
        if (T.worker.joinable()) T.worker.join();
        OpenTradePartnerAsk();
        Hub().version++;
        return;
    }
    std::string id;
    OnlineTrade copy;
    {
        std::lock_guard<std::mutex> lk(T.m);
        if (hit.kind == TR_ITEM && hit.value < (int)T.list.size()) { T.selId = T.list[(size_t)hit.value].id; T.card = -1; T.msg.clear(); Hub().version++; return; }
        if (hit.kind == TR_CARD) { T.card = hit.value; Hub().version++; return; }
        for (const OnlineTrade& t : T.list) if (t.id == T.selId) copy = t;
        id = copy.id;
    }
    if (id.empty()) return;
    const int card = T.card;
    if (hit.kind == TR_WANT) { TradesStartCounter(copy, card); return; }
    if (hit.kind == TR_FINISH) { { std::lock_guard<std::mutex> lk(T.m); T.msg = "Trading..."; } TradesRun([id, card] { TradesActJob(id, "finish", card); }); Hub().version++; return; }
    if (hit.kind == TR_DECLINE || hit.kind == TR_CANCEL) { { std::lock_guard<std::mutex> lk(T.m); T.msg = "Calling it off..."; } TradesRun([id] { TradesActJob(id, "cancel", -1); }); Hub().version++; return; }
}

// The trade's scene: the Pokémon sent goes off one side, the one got comes in from the other, as
// the games' trade does; over everything, a tap to close.
inline bool TradeAnimOn() { return Trades().anim.on; }
inline void TradeAnimClose() { Trades().anim.on = false; Hub().version++; }
inline void DrawTradeAnim(Canvas& c, float u)
{
    TradesState& T = Trades();
    TradesState::Anim a;
    { std::lock_guard<std::mutex> lk(T.m); a = T.anim; }
    if (!a.on) return;
    const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - a.at).count();
    Hub().version++; // (a new frame each time, while it plays)
    for (auto& p : c.px) p = 0xFF000000u + (((p >> 2) & 0x3F3F3Fu) + ((p >> 3) & 0x1F1F1Fu)) + 0x080402u;
    const int w = std::min((int)(640 * u), c.w - (int)(40 * u)), h = std::min((int)(420 * u), c.h - (int)(40 * u));
    const int x0 = (c.w - w) / 2, y0 = (c.h - h) / 2;
    Glass::Round(c, x0, y0, w, h, 16 * u, TradePal::Body, TradePal::Primary, 4 * u);
    const int sx = x0 + (int)(20 * u), sy = y0 + (int)(20 * u), sw = w - (int)(40 * u), sh = h - (int)(130 * u);
    Glass::Round(c, sx, sy, sw, sh, 12 * u, 0xFF16261Au, TradePal::Line, 1.0f * u);
    const int box = (int)(sh * 0.7f);
    const float cx = sx + sw * 0.5f, cy = sy + sh * 0.5f;
    // (0 to 1.6 s: theirs... ours goes; 1.6 to 2.2: a ball crosses; 2.2 to 3.4: theirs comes; then rests.)
    if (t < 1.6f)
    {
        const float k = std::max(0.0f, (t - 0.6f) / 1.0f), e = k * k;
        const int b = std::max(8, (int)(box * (1 - 0.6f * e)));
        PcDrawMon(c, a.give, a.giveShiny, false, (int)(cx - b / 2.0f - e * sw * 0.55f), (int)(cy - b / 2.0f), b, b, 0, true);
    }
    else if (t < 2.2f)
    {
        const float k = (t - 1.6f) / 0.6f;
        const float bx = sx + sw * (0.1f + 0.8f * k), byy = cy - std::sin(k * 3.14159f) * sh * 0.3f;
        Glass::DrawIcon(c, Glass::I_POKEBALL, bx, byy, 14 * u, 0xFFFFFFFFu);
    }
    else
    {
        const float k = std::min(1.0f, (t - 2.2f) / 1.0f), e = 1 - (1 - k) * (1 - k);
        const int b = std::max(8, (int)(box * (0.4f + 0.6f * e)));
        PcDrawMon(c, a.get, a.getShiny, false, (int)(cx - b / 2.0f + (1 - e) * sw * 0.55f), (int)(cy - b / 2.0f), b, b, 0, true);
        if (k >= 1)
            for (int i = 0; i < 12; i++)
            {
                const float ang = i * 0.5236f + t * 0.8f, r = box * (0.45f + 0.08f * std::sin(t * 4 + i));
                Glass::DrawIcon(c, Glass::I_SPARKLE, cx + std::cos(ang) * r, cy + std::sin(ang) * r * 0.7f, (5 + 2 * std::sin(t * 6 + i)) * u, TradePal::Shiny);
            }
    }
    const std::string l1 = t < 2.2f ? LauncherText::Tr("Sending") + " " + a.giveName + "..." : LauncherText::Tr("Trade complete!");
    const std::string l2 = t < 2.2f ? std::string() : a.giveName + " " + LauncherText::Tr("went to") + " " + a.partner + ". " + LauncherText::Tr("You got") + " " + a.getName + "!";
    const int ty = sy + sh + (int)(30 * u);
    GT(c, x0 + (w - GW(l1, 17 * u, true)) / 2, (float)ty, l1, 17 * u, TradePal::Ink, true);
    if (!l2.empty()) GTFit(c, x0 + std::max((int)(20 * u), (w - GW(l2, 14 * u, false)) / 2), ty + 26 * u, l2, 14 * u, w - (int)(40 * u), TradePal::Ink, false);
    const std::string hint = LauncherText::Tr("Tap to close") + "   \xC2\xB7   " + LauncherText::Tr("It is in your Bank.");
    GT(c, x0 + (w - GW(hint, 11.5f * u, false)) / 2, (float)(y0 + h - (int)(16 * u)), hint, 11.5f * u, TradePal::Hint, false);
}
#else
inline void TradesPicked(bool) {}
inline bool TradeAnimOn() { return false; }
inline void TradeAnimClose() {}
inline void DrawTradeAnim(Canvas&, float) {}
inline void TradesFetch() {}
inline int TradesWaiting() { return 0; }
inline void OpenTrades() {}
#endif
