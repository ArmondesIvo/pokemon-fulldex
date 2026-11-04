// Frosted glass for the pause menu (ui.h, Menu::Draw): the paused game blurred once as the
// background, translucent rounded panels and tiles over it, and simple vector icons.
// Drawn in software into the Canvas, so it looks the same on PC, phones and the Switch.
// Included by ui.h after Canvas.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Glass
{
// src (with its alpha, times cover/255) over dst; the result is opaque.
inline uint32_t Over(uint32_t dst, uint32_t src, int cover = 255)
{
    const int a = (int)(src >> 24) * cover / 255;
    if (a <= 0) return dst | 0xFF000000u;
    const int ia = 255 - a;
    const int r = ((int)(src >> 16 & 255) * a + (int)(dst >> 16 & 255) * ia) / 255;
    const int g = ((int)(src >> 8 & 255) * a + (int)(dst >> 8 & 255) * ia) / 255;
    const int b = ((int)(src & 255) * a + (int)(dst & 255) * ia) / 255;
    return 0xFF000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
}

// Grows the canvas's drawn rectangle (Canvas::Dirty) to take in a box.
inline void Mark(Canvas& c, int x0, int y0, int x1, int y1)
{
    x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(c.w, x1); y1 = std::min(c.h, y1);
    if (x1 <= x0 || y1 <= y0) return;
    c.x0d = std::min(c.x0d, x0); c.y0d = std::min(c.y0d, y0);
    c.x1d = std::max(c.x1d, x1); c.y1d = std::max(c.y1d, y1);
}

// The game behind the menu: src (w x h, ARGB) made small, blurred, darkened a little and a little
// greyer (so white glass and text read over any scene), then scaled back up into out (opaque).
// Done once when the menu opens: the game is paused, so it never needs doing again.
inline void MakeBackdrop(std::vector<uint32_t>& out, const uint32_t* src, int w, int h)
{
    const int f = std::max(4, std::min(w, h) / 90);
    const int sw = std::max(1, w / f), sh = std::max(1, h / f);
    std::vector<float> s((size_t)sw * sh * 3), t(s.size());
    for (int y = 0; y < sh; y++)
        for (int x = 0; x < sw; x++)
        {
            float r = 0, g = 0, b = 0;
            int n = 0;
            for (int yy = y * f; yy < std::min(h, y * f + f); yy += 2)
                for (int xx = x * f; xx < std::min(w, x * f + f); xx += 2)
                {
                    const uint32_t p = src[(size_t)yy * w + xx];
                    r += (float)(p >> 16 & 255); g += (float)(p >> 8 & 255); b += (float)(p & 255); n++;
                }
            float* d = &s[((size_t)y * sw + x) * 3];
            d[0] = r / n; d[1] = g / n; d[2] = b / n;
        }
    const int rad = 2; // three box blurs each way: close to a gaussian
    for (int pass = 0; pass < 3; pass++)
    {
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++)
                for (int k = 0; k < 3; k++)
                {
                    float sum = 0;
                    for (int dx = -rad; dx <= rad; dx++) sum += s[((size_t)y * sw + std::clamp(x + dx, 0, sw - 1)) * 3 + k];
                    t[((size_t)y * sw + x) * 3 + k] = sum / (2 * rad + 1);
                }
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++)
                for (int k = 0; k < 3; k++)
                {
                    float sum = 0;
                    for (int dy = -rad; dy <= rad; dy++) sum += t[((size_t)std::clamp(y + dy, 0, sh - 1) * sw + x) * 3 + k];
                    s[((size_t)y * sw + x) * 3 + k] = sum / (2 * rad + 1);
                }
    }
    for (size_t i = 0; i < s.size(); i += 3)
    {
        const float l = 0.3f * s[i] + 0.59f * s[i + 1] + 0.11f * s[i + 2];
        for (int k = 0; k < 3; k++) s[i + k] = (s[i + k] * 0.75f + l * 0.25f) * 0.58f;
    }
    out.assign((size_t)w * h, 0xFF000000u);
    for (int y = 0; y < h; y++)
    {
        const float fy = std::clamp((y + 0.5f) / f - 0.5f, 0.0f, (float)(sh - 1));
        const int y0 = (int)fy, y1 = std::min(sh - 1, y0 + 1);
        const float ty = fy - (float)y0;
        for (int x = 0; x < w; x++)
        {
            const float fx = std::clamp((x + 0.5f) / f - 0.5f, 0.0f, (float)(sw - 1));
            const int x0 = (int)fx, x1 = std::min(sw - 1, x0 + 1);
            const float tx = fx - (float)x0;
            uint32_t p = 0xFF000000u;
            for (int k = 0; k < 3; k++)
            {
                const float a = s[((size_t)y0 * sw + x0) * 3 + k], b = s[((size_t)y0 * sw + x1) * 3 + k];
                const float c2 = s[((size_t)y1 * sw + x0) * 3 + k], d = s[((size_t)y1 * sw + x1) * 3 + k];
                const float v = (a * (1 - tx) + b * tx) * (1 - ty) + (c2 * (1 - tx) + d * tx) * ty;
                p |= (uint32_t)std::clamp((int)v, 0, 255) << (16 - 8 * k);
            }
            out[(size_t)y * w + x] = p;
        }
    }
}

// The menu's background: the blurred game when it is ready (true), otherwise the game
// darkened as before (false: the first frame, or a renderer that cannot be read back).
inline bool Backdrop(Canvas& c)
{
    Mark(c, 0, 0, c.w, c.h);
    if (c.backdrop.size() == (size_t)c.w * c.h && !c.px.empty())
    {
        std::copy(c.backdrop.begin(), c.backdrop.end(), c.px.begin());
        return true;
    }
    std::fill(c.px.begin(), c.px.end(), 0xC0101418u);
    return false;
}

// Text and anything else drawn with partial alpha made solid, over the opaque blurred game.
inline void Opaque(Canvas& c)
{
    for (int y = c.y0d; y < c.y1d; y++)
        for (int x = c.x0d; x < c.x1d; x++) c.px[(size_t)y * c.w + x] |= 0xFF000000u;
}

// A rounded rectangle of glass: fill (with its alpha) and an optional border bw wide, smooth edged.
inline void Round(Canvas& c, int x, int y, int w, int h, float r, uint32_t fill, uint32_t border = 0, float bw = 1.0f)
{
    if (w <= 0 || h <= 0) return;
    Mark(c, x, y, x + w, y + h);
    const float hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy;
    r = std::min(r, std::min(hx, hy));
    const bool hasBorder = (border >> 24) != 0;
    const float edgeZone = (hasBorder ? bw : 0.0f) + 1.0f;
    for (int py = std::max(0, y); py < std::min(c.h, y + h); py++)
    {
        uint32_t* row = &c.px[(size_t)py * c.w];
        const float qy = std::fabs(py + 0.5f - cy) - (hy - r);
        for (int px = std::max(0, x); px < std::min(c.w, x + w); px++)
        {
            const float qx = std::fabs(px + 0.5f - cx) - (hx - r);
            uint32_t& p = row[px];
            if (qx < -r - edgeZone || qy < -r - edgeZone) // well inside, away from the edge: plain fill
            {
                if (qx < 0 && qy < 0 && std::max(qx, qy) - r < -edgeZone) { p = Over(p, fill); continue; }
            }
            const float d = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
            const float inside = std::clamp(0.5f - d, 0.0f, 1.0f);
            if (inside <= 0) continue;
            p = Over(p, fill, (int)(inside * 255));
            if (hasBorder)
            {
                const float e = std::clamp(bw * 0.5f + 0.5f - std::fabs(d + bw * 0.5f), 0.0f, 1.0f);
                if (e > 0) p = Over(p, border, (int)(e * 255));
            }
        }
    }
}

// Signed distances for the icons (in the icon's own units: -1 to 1 across it).
inline float Circle(float u, float v, float cx, float cy, float r) { return std::hypot(u - cx, v - cy) - r; }
inline float Box(float u, float v, float cx, float cy, float hx, float hy, float r)
{
    const float qx = std::fabs(u - cx) - hx + r, qy = std::fabs(v - cy) - hy + r;
    return std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
}
inline float Segment(float u, float v, float ax, float ay, float bx, float by, float r)
{
    const float px = u - ax, py = v - ay, dx = bx - ax, dy = by - ay;
    const float t = std::clamp((px * dx + py * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
    return std::hypot(px - dx * t, py - dy * t) - r;
}
// A triangle pointing right, its left edge at x0, tip at x1, half height hh, centred on cy.
inline float Play(float u, float v, float x0, float x1, float cy, float hh)
{
    const float len = std::hypot(x1 - x0, hh);
    const float top = ((v - cy) * (x1 - x0) + (u - x1) * hh) / len;   // the upper slanted edge
    const float bot = (-(v - cy) * (x1 - x0) + (u - x1) * hh) / len;  // the lower slanted edge
    return std::max(std::max(x0 - u, top), bot);
}

enum Icon { I_PLAY, I_SAVE, I_LOAD, I_CHEATS, I_FAST, I_TROPHY, I_GEAR, I_POWER, I_COUNT };

// One of the icons, centred at (cx, cy), size pixels from its centre to its edge.
inline void DrawIcon(Canvas& c, int icon, float cx, float cy, float size, uint32_t argb)
{
    const float t = 0.12f; // line thickness
    auto sdf = [&](float u, float v) -> float {
        switch (icon)
        {
        case I_PLAY: return Play(u, v, -0.42f, 0.62f, 0.0f, 0.62f) - 0.03f;
        case I_SAVE:
        {
            const float body = std::fabs(Box(u, v, 0, 0, 0.72f, 0.72f, 0.14f)) - t / 2;
            const float label = Box(u, v, 0, 0.36f, 0.4f, 0.2f, 0.04f);
            const float shutter = Box(u, v, -0.05f, -0.5f, 0.32f, 0.14f, 0.03f);
            return std::min(body, std::min(label, shutter));
        }
        case I_LOAD:
        {
            const float folder = std::min(Box(u, v, 0, 0.12f, 0.78f, 0.52f, 0.1f), Box(u, v, -0.4f, -0.42f, 0.36f, 0.14f, 0.08f));
            return std::fabs(folder) - t / 2;
        }
        case I_CHEATS:
        {
            const float page = std::fabs(Box(u, v, 0, 0, 0.56f, 0.74f, 0.12f)) - t / 2;
            const float l1 = Segment(u, v, -0.26f, -0.32f, 0.26f, -0.32f, t / 2);
            const float l2 = Segment(u, v, -0.26f, 0.0f, 0.26f, 0.0f, t / 2);
            const float l3 = Segment(u, v, -0.26f, 0.32f, 0.1f, 0.32f, t / 2);
            return std::min(page, std::min(l1, std::min(l2, l3)));
        }
        case I_FAST: return std::min(Play(u, v, -0.78f, 0.04f, 0, 0.52f), Play(u, v, -0.04f, 0.78f, 0, 0.52f)) - 0.02f;
        case I_TROPHY:
        {
            const float rim = Box(u, v, 0, -0.5f, 0.48f, 0.16f, 0.04f);
            const float bowl = std::max(Circle(u, v, 0, -0.36f, 0.48f), -(v + 0.36f));
            const float handles = std::min(std::fabs(Circle(u, v, -0.5f, -0.3f, 0.2f)) - 0.05f, std::fabs(Circle(u, v, 0.5f, -0.3f, 0.2f)) - 0.05f);
            const float stem = Segment(u, v, 0, 0.1f, 0, 0.5f, 0.08f);
            const float base = Box(u, v, 0, 0.62f, 0.36f, 0.1f, 0.04f);
            return std::min(std::min(rim, bowl), std::min(handles, std::min(stem, base)));
        }
        case I_GEAR:
        {
            float gear = Circle(u, v, 0, 0, 0.56f);
            for (int k = 0; k < 8; k++)
            {
                const float a = k * 0.785398f, ca = std::cos(a), sa = std::sin(a);
                const float ru = u * ca + v * sa, rv = -u * sa + v * ca;
                gear = std::min(gear, Box(ru, rv, 0.66f, 0, 0.16f, 0.14f, 0.03f));
            }
            return std::max(gear, -Circle(u, v, 0, 0, 0.24f));
        }
        case I_POWER:
        {
            const float ring = std::fabs(Circle(u, v, 0, 0.08f, 0.58f)) - t / 2;
            const float gap = Box(u, v, 0, -0.5f, 0.26f, 0.32f, 0.0f);
            const float line = Segment(u, v, 0, -0.78f, 0, -0.06f, t / 2);
            return std::min(std::max(ring, -gap), line);
        }
        }
        return 1.0f;
    };
    const int x0 = (int)(cx - size) - 2, y0 = (int)(cy - size) - 2, x1 = (int)(cx + size) + 3, y1 = (int)(cy + size) + 3;
    Mark(c, x0, y0, x1, y1);
    for (int py = std::max(0, y0); py < std::min(c.h, y1); py++)
        for (int px = std::max(0, x0); px < std::min(c.w, x1); px++)
        {
            const float d = sdf((px + 0.5f - cx) / size, (py + 0.5f - cy) / size) * size;
            const float cover = std::clamp(0.5f - d, 0.0f, 1.0f);
            if (cover > 0) { uint32_t& p = c.px[(size_t)py * c.w + px]; p = Over(p, argb, (int)(cover * 255)); }
        }
}
} // namespace Glass
