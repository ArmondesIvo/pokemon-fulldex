// On-screen controls for phones and tablets: a D-pad, A and B, L and R, START and SELECT, and
// buttons for the pause menu and fast forward, drawn over the game (frontend/ui.h's Canvas) and
// pressed with any number of fingers. Shown while the last input was a touch.
#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <string>

#include <SDL.h>

#include "ui.h"
#include "touchlayout.h"

namespace gbaw
{

namespace Touch
{

// The first ten are the GBA's keys, in mGBA's order (main.cpp's GbaKey).
enum Button { A, B, SELECT, START, RIGHT, LEFT, UP, DOWN, R, L, MENU, FAST, COUNT };

struct Layout
{
    int padX = 0, padY = 0, padR = 0; // the D-pad's middle and size
    int aX = 0, aY = 0, bX = 0, bY = 0, abR = 0, bR = 0;
    SDL_Rect l{}, r{}, start{}, select{}, menu{}, fast{};
};

// Where everything goes on a canvas of w x h (about 360 lines high): the D-pad bottom left, A
// and B bottom right, L and R at the top corners, START and SELECT at the bottom in the middle.
inline Layout Make(int w, int h)
{
    Layout t;
    t.padR = 50;
    t.padX = 22 + t.padR;
    t.padY = h - 26 - t.padR;
    t.abR = t.bR = 25;
    t.aX = w - 22 - t.abR;
    t.aY = h - 26 - t.padR - 14;
    t.bX = t.aX - 2 * t.abR - 10;
    t.bY = t.aY + 30;
    t.l = { 6, 34, 88, 30 };
    t.r = { w - 94, 34, 88, 30 };
    t.select = { w / 2 - 70, h - 24, 60, 18 };
    t.start = { w / 2 + 10, h - 24, 60, 18 };
    t.menu = { w / 2 - 52, 4, 48, 18 };
    t.fast = { w / 2 + 4, 4, 48, 18 };
    return t;
}

inline std::vector<TouchLayout::Control> Geometry(const Layout& t)
{
    std::vector<TouchLayout::Control> c = {
        { "DPAD", "D-PAD", (float)t.padX, (float)t.padY, (float)t.padR, (float)t.padR, false },
        { "A", "A", (float)t.aX, (float)t.aY, (float)t.abR, (float)t.abR, true },
        { "B", "B", (float)t.bX, (float)t.bY, (float)t.bR, (float)t.bR, true }
    };
    auto rect = [&](const char* key, const SDL_Rect& r) { c.push_back({key, key, r.x + r.w / 2.0f, r.y + r.h / 2.0f, r.w / 2.0f, r.h / 2.0f, false}); };
    rect("L", t.l); rect("R", t.r); rect("SELECT", t.select); rect("START", t.start); rect("MENU", t.menu); rect(">>", t.fast);
    return c;
}
inline std::vector<TouchLayout::Control> Defaults(int w, int h)
{
    const int scale = std::max(1, std::min(h / 300, w / 600));
    auto controls = Geometry(Make(w / scale, h / scale));
    for (auto& c : controls) { c.x *= scale; c.y *= scale; c.rx *= scale; c.ry *= scale; }
    return controls;
}
inline void Apply(Layout& t, const TouchLayout::Profile& profile, int w, int h)
{
    auto controls = profile.Apply(Geometry(t), w, h);
    t.padX = (int)controls[0].x; t.padY = (int)controls[0].y; t.padR = (int)controls[0].rx;
    t.aX = (int)controls[1].x; t.aY = (int)controls[1].y; t.abR = (int)controls[1].rx;
    t.bX = (int)controls[2].x; t.bY = (int)controls[2].y; t.bR = (int)controls[2].rx;
    SDL_Rect* rects[] = { &t.l, &t.r, &t.select, &t.start, &t.menu, &t.fast };
    for (int i = 0; i < 6; ++i)
    {
        const auto& c = controls[i + 3];
        *rects[i] = { (int)(c.x - c.rx), (int)(c.y - c.ry), (int)(c.rx * 2), (int)(c.ry * 2) };
    }
}

// The buttons a finger at (x, y) on the canvas presses: a bit for each Button.
inline uint32_t Hit(const Layout& t, float x, float y)
{
    uint32_t m = 0;
    const float dx = x - t.padX, dy = y - t.padY;
    const float d = std::sqrt(dx * dx + dy * dy);
    if (d < t.padR * 1.4f && d > t.padR * 0.15f)
    {
        // Eight directions: each of the four covers 135 degrees, so the diagonals press two.
        const float a = std::atan2(dy, dx), s = 3.14159265f / 8;
        if (std::fabs(a) < 3 * s) m |= 1u << RIGHT;
        if (std::fabs(a) > 5 * s) m |= 1u << LEFT;
        if (a > s && a < 7 * s) m |= 1u << DOWN;
        if (a < -s && a > -7 * s) m |= 1u << UP;
    }
    auto inCircle = [&](int cx, int cy, float r) { const float ex = x - cx, ey = y - cy; return ex * ex + ey * ey <= r * r; };
    if (inCircle(t.aX, t.aY, t.abR * 1.3f)) m |= 1u << A;
    if (inCircle(t.bX, t.bY, t.bR * 1.3f)) m |= 1u << B;
    auto inRect = [&](const SDL_Rect& r) { return x >= r.x - 6 && x < r.x + r.w + 6 && y >= r.y - 6 && y < r.y + r.h + 6; };
    if (inRect(t.l)) m |= 1u << L;
    if (inRect(t.r)) m |= 1u << R;
    if (inRect(t.start)) m |= 1u << START;
    if (inRect(t.select)) m |= 1u << SELECT;
    if (inRect(t.menu)) m |= 1u << MENU;
    if (inRect(t.fast)) m |= 1u << FAST;
    return m;
}

inline void Circle(Canvas& c, int cx, int cy, int r, uint32_t argb)
{
    for (int y = -r; y <= r; y++)
    {
        const int half = (int)std::sqrt((float)(r * r - y * y));
        c.Fill(cx - half, cy + y, 2 * half + 1, 1, argb);
    }
}

// Drawn in the style of My Boy's controls: thin grey outlines over the game, with no fill, so
// the picture stays visible; a pressed control gets a white outline and a faint white fill.
// Each shape is drawn from its distance to the outline, which keeps the curves even.
namespace Style
{
const uint32_t Line = 0xFFA8A8A8, LineOn = 0xFFFFFFFF, FillOn = 0x40FFFFFF;
const float Thick = 1.2f;

// Distance from (px, py) to a rectangle with round corners (negative inside).
inline float RoundRectDist(float px, float py, float cx, float cy, float hw, float hh, float rr)
{
    rr = std::min(rr, std::min(hw, hh));
    const float qx = std::fabs(px - cx) - (hw - rr), qy = std::fabs(py - cy) - (hh - rr);
    const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - rr;
}
inline void RoundRect(Canvas& c, float cx, float cy, float hw, float hh, float rr, bool on)
{
    const int x0 = (int)std::floor(cx - hw - 2), x1 = (int)std::ceil(cx + hw + 2);
    const int y0 = (int)std::floor(cy - hh - 2), y1 = (int)std::ceil(cy + hh + 2);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
        {
            const float d = RoundRectDist(x + 0.5f, y + 0.5f, cx, cy, hw, hh, rr);
            if (d <= 0 && d > -Thick) c.Fill(x, y, 1, 1, on ? LineOn : Line);
            else if (on && d <= 0) c.Fill(x, y, 1, 1, FillOn);
        }
}
inline void Ring(Canvas& c, float cx, float cy, float r, bool on) { RoundRect(c, cx, cy, r, r, r, on); }

// A thick line from (ax, ay) to (bx, by).
inline void Line2(Canvas& c, float ax, float ay, float bx, float by, uint32_t col, float t = Thick)
{
    const int x0 = (int)std::floor(std::min(ax, bx) - t), x1 = (int)std::ceil(std::max(ax, bx) + t);
    const int y0 = (int)std::floor(std::min(ay, by) - t), y1 = (int)std::ceil(std::max(ay, by) + t);
    const float dx = bx - ax, dy = by - ay, len2 = std::max(dx * dx + dy * dy, 1e-6f);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
        {
            const float px = x + 0.5f - ax, py = y + 0.5f - ay;
            const float k = std::clamp((px * dx + py * dy) / len2, 0.0f, 1.0f);
            const float ex = px - k * dx, ey = py - k * dy;
            if (ex * ex + ey * ey <= t * t / 4) c.Fill(x, y, 1, 1, col);
        }
}

// One key of the D-pad: a square with round corners and an arrow like a house's outline,
// pointing out from the middle (dir: 0 up, 1 right, 2 down, 3 left).
inline void DpadKey(Canvas& c, float cx, float cy, float half, int dir, bool on)
{
    RoundRect(c, cx, cy, half, half, half * 0.22f, on);
    const float a = half * 0.62f;
    // The arrow pointing up, then turned.
    const float pts[5][2] = { { 0, -a * 1.05f }, { a, -a * 0.05f }, { a, a }, { -a, a }, { -a, -a * 0.05f } };
    float p[5][2];
    for (int i = 0; i < 5; i++)
    {
        float x = pts[i][0], y = pts[i][1];
        for (int k = 0; k < dir; k++) { const float t = x; x = -y; y = t; }
        p[i][0] = cx + x; p[i][1] = cy + y;
    }
    for (int i = 0; i < 5; i++)
        Line2(c, p[i][0], p[i][1], p[(i + 1) % 5][0], p[(i + 1) % 5][1], on ? LineOn : Line);
}
}

inline void Draw(Canvas& c, const Layout& t, uint32_t held, bool fastOn)
{
    using namespace Style;
    auto pressed = [&](Button b) { return ((held >> b) & 1) != 0; };

    // The D-pad: four separate keys around an empty middle.
    const float key = t.padR * 0.31f, off = t.padR * 0.69f;
    DpadKey(c, t.padX, t.padY - off, key, 0, pressed(UP));
    DpadKey(c, t.padX + off, t.padY, key, 1, pressed(RIGHT));
    DpadKey(c, t.padX, t.padY + off, key, 2, pressed(DOWN));
    DpadKey(c, t.padX - off, t.padY, key, 3, pressed(LEFT));

    // A and B: rings with their letter.
    Ring(c, t.aX, t.aY, t.abR, pressed(A));
    Ring(c, t.bX, t.bY, t.bR, pressed(B));
    c.TextCentred(t.aX, t.aY - 7, "A", 2, pressed(A) ? LineOn : Line);
    c.TextCentred(t.bX, t.bY - 7, "B", 2, pressed(B) ? LineOn : Line);

    // L and R: shoulder buttons, round on the outer side, with the letter there.
    auto shoulder = [&](const SDL_Rect& r, const char* label, bool right, bool on) {
        const float cx = r.x + r.w / 2.0f, cy = r.y + r.h / 2.0f, hh = r.h / 2.0f;
        RoundRect(c, cx, cy, r.w / 2.0f, hh, hh * 0.9f, on);
        const int lx = right ? r.x + r.w - (int)hh - 3 : r.x + (int)hh - 2;
        c.TextCentred(lx, (int)cy - 7, label, 2, on ? LineOn : Line);
    };
    shoulder(t.l, "L", false, pressed(L));
    shoulder(t.r, "R", true, pressed(R));

    // START and SELECT: plain pills, as on the console.
    auto pill = [&](const SDL_Rect& r, bool on) {
        const float hh = std::min(r.h / 2.0f, 7.0f);
        RoundRect(c, r.x + r.w / 2.0f, r.y + r.h / 2.0f, r.w / 2.0f, hh, hh, on);
    };
    pill(t.select, pressed(SELECT));
    pill(t.start, pressed(START));

    // The pause menu and fast forward: square icons in the middle of their spaces. Fast
    // forward stays lit while it is on.
    auto icon = [&](const SDL_Rect& r, bool on) {
        const float s = std::min(r.w, r.h) / 2.0f;
        const float cx = r.x + r.w / 2.0f, cy = r.y + r.h / 2.0f;
        RoundRect(c, cx, cy, s, s, s * 0.25f, on);
        return std::make_pair(cx, s);
    };
    {
        const bool on = pressed(MENU);
        auto [cx, s] = icon(t.menu, on);
        const float cy = t.menu.y + t.menu.h / 2.0f, w = s * 0.55f;
        for (int i = -1; i <= 1; i++) Line2(c, cx - w, cy + i * s * 0.4f, cx + w, cy + i * s * 0.4f, on ? LineOn : Line);
    }
    {
        const bool on = fastOn || pressed(FAST);
        auto [cx, s] = icon(t.fast, on);
        const float cy = t.fast.y + t.fast.h / 2.0f, h = s * 0.45f;
        for (int i = 0; i < 2; i++)
        {
            const float x = cx - s * 0.35f + i * s * 0.4f;
            Line2(c, x, cy - h, x + h * 0.8f, cy, on ? LineOn : Line);
            Line2(c, x + h * 0.8f, cy, x, cy + h, on ? LineOn : Line);
        }
    }
}

}

} // namespace gbaw
