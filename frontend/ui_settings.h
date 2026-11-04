// Settings pages of the pause menu (Menu::style 1, ui.h): the template every settings page uses.
//   Header: a back button, the page's icon and its title, over a soft line.
//   Left:   the rows, in sections (K_SECTION, with an icon and a line between sections). A row has
//           an icon, its name and on the right one of: an Off / On pair of pills (K_TOGGLE), a
//           < value > picker (K_PICKER), or > for a row that opens a page (K_LINK).
//   Right:  a preview of the chosen row: its name, what it does, a picture for each choice (made
//           from the paused game: Canvas::sharp, dsTop, dsBottom; never bundled art), the choice
//           that is on marked, and a tip.
// On a portrait screen the preview goes under the rows. Sizes follow the panel (k: one pixel of
// the 1400 x 788 design), so it looks the same on a PC window, a phone and the Switch.
// Included at the end of ui.h.
#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace SettingsArt
{
// The settings colours: frosted glass (Glass::Frost) with white over it, and white for whatever is
// chosen (the row's edge, the pill that is on, its picture), with dark text on a white pill.
constexpr uint32_t Panel = 0x0FFFFFFF, PanelEdge = 0x5CFFFFFF, Row = 0x1AFFFFFF, RowEdge = 0x24FFFFFF;
constexpr uint32_t RowChosen = 0x2EFFFFFF, Glow = 0xFFFFFF, ChosenEdge = 0xFFFFFFFF;
constexpr uint32_t Pill = 0xF7FFFFFF, PillEdge = 0xFFFFFFFF, PillInk = 0xFF0B1018, PillOff = 0x1EFFFFFF, PillOffEdge = 0x33FFFFFF;
constexpr uint32_t Desc = 0xFFD3DAE4, Line = 0x2EFFFFFF;

// Text in pixels (px: the font's size) in either font: the clean font at that size, or the pixel
// font at the nearest whole scale. title: written in title case (labels are kept in capitals).
inline std::string Shown(const std::string& s, bool title)
{
    return title ? CleanFont::Title(UiLang::Tr(s)) : UiLang::Tr(s);
}
inline int PixelScale(int px) { return std::max(1, (px + 4) / 9); }
inline int W(const std::string& s, int px, bool bold = false, bool title = true)
{
    if (Menu::UseClean()) return CleanFont::Width(Shown(s, title), px, bold);
    return Canvas::TextWidth(s, PixelScale(px));
}
// (y: the baseline.)
inline void T(Canvas& c, int x, int y, const std::string& s, int px, uint32_t col, bool bold = false, bool title = true)
{
    if (Menu::UseClean()) CleanFont::Draw(c, x, y, Shown(s, title), px, col, bold);
    else c.Text(x, y - 7 * PixelScale(px), s, PixelScale(px), col);
}
inline void TCentred(Canvas& c, int cx, int y, const std::string& s, int px, uint32_t col, bool bold = false)
{
    T(c, cx - W(s, px, bold) / 2, y, s, px, col, bold);
}
// Sentences (descriptions, tips) cut into lines no wider than maxW.
inline std::vector<std::string> Wrap(const std::string& s, int px, int maxW, size_t maxLines)
{
    std::vector<std::string> lines;
    std::string line;
    size_t i = 0;
    const std::string text = UiLang::Tr(s);
    while (i < text.size())
    {
        size_t j = text.find(' ', i);
        if (j == std::string::npos) j = text.size();
        const std::string word = text.substr(i, j - i);
        const std::string tryLine = line.empty() ? word : line + " " + word;
        if (!line.empty() && W(tryLine, px, false, false) > maxW)
        {
            lines.push_back(line);
            line = word;
        }
        else line = tryLine;
        i = j + 1;
    }
    if (!line.empty()) lines.push_back(line);
    if (lines.size() > maxLines)
    {
        lines.resize(maxLines);
        lines.back() += "...";
    }
    return lines;
}

// A soft glow round a box (the chosen row, card or button): glass.h's.
inline void GlowRound(Canvas& c, int x, int y, int w, int h, float r, float k) { Glass::Glow(c, x, y, w, h, r, k); }

// A horizontal line brightest in its middle (under the header).
inline void FadeLine(Canvas& c, int x0, int x1, int y, int thick, uint32_t rgb, int alpha)
{
    if (x1 <= x0) return;
    Glass::Mark(c, x0, y - thick, x1, y + 2 * thick);
    for (int x = std::max(0, x0); x < std::min(c.w, x1); x++)
    {
        const float t = (x - x0) / (float)(x1 - x0);
        const float f = std::pow(std::max(0.0f, 1.0f - std::fabs(t - 0.45f) * 1.9f), 0.7f);
        for (int d = -thick; d < 2 * thick; d++)
        {
            const int yy = y + d;
            if (yy < 0 || yy >= c.h) continue;
            const float fall = d >= 0 && d < thick ? 1.0f : 0.3f; // (a faint glow either side)
            uint32_t& p = c.px[(size_t)yy * c.w + x];
            p = Glass::Over(p, (uint32_t)(alpha * f * fall) << 24 | rgb);
        }
    }
}

inline void Rule(Canvas& c, int x, int y, int w, int h, uint32_t argb) { Glass::Round(c, x, y, w, h, 0, argb); }

// The paused game, as pictures are made of it.
struct Src
{
    const uint32_t* p = nullptr;
    int w = 0, h = 0;
};
inline Src Top(const Canvas& c) { return c.dsTop.size() == 256 * 192 ? Src{ c.dsTop.data(), 256, 192 } : Src{}; }
inline Src Bottom(const Canvas& c) { return c.dsBottom.size() == 256 * 192 ? Src{ c.dsBottom.data(), 256, 192 } : Src{}; }
// The whole picture as the player sees it (or the top screen, if the window could not be read).
inline Src Frame(const Canvas& c)
{
    if (!c.sharp.empty() && c.sharp.size() == (size_t)c.w * c.h) return { c.sharp.data(), c.w, c.h };
    return Top(c);
}

// Part of an image (sx, sy, sw, sh) drawn into a box, smooth or as sharp pixels, its corners
// rounded r. With no image: a plain dark screen with a Poke Ball on it.
inline void Image(Canvas& c, Src s, float sx, float sy, float sw, float sh, int dx, int dy, int dw, int dh, bool smooth, float r)
{
    if (dw <= 0 || dh <= 0) return;
    if (!s.p)
    {
        Glass::Round(c, dx, dy, dw, dh, r, 0xFF2A333F);
        const float size = std::min(dw, dh) * 0.26f;
        Glass::DrawIcon(c, Glass::I_POKEBALL, dx + dw * 0.5f, dy + dh * 0.5f, size, 0xFF8995A3);
        return;
    }
    Glass::Mark(c, dx, dy, dx + dw, dy + dh);
    const float hx = dw * 0.5f, hy = dh * 0.5f, cx = dx + hx, cy = dy + hy;
    r = std::min(r, std::min(hx, hy));
    for (int py = std::max(0, dy); py < std::min(c.h, dy + dh); py++)
    {
        const float v = sy + (py - dy + 0.5f) * sh / dh;
        const float qy = std::fabs(py + 0.5f - cy) - (hy - r);
        for (int px = std::max(0, dx); px < std::min(c.w, dx + dw); px++)
        {
            float cover = 1.0f;
            if (r > 0)
            {
                const float qx = std::fabs(px + 0.5f - cx) - (hx - r);
                if (qx > 0 && qy > 0) cover = std::clamp(r - std::hypot(qx, qy) + 0.5f, 0.0f, 1.0f);
                if (cover <= 0) continue;
            }
            const float u = sx + (px - dx + 0.5f) * sw / dw;
            uint32_t out;
            if (!smooth)
            {
                const int ix = std::clamp((int)u, 0, s.w - 1), iy = std::clamp((int)v, 0, s.h - 1);
                out = s.p[(size_t)iy * s.w + ix] | 0xFF000000u;
            }
            else
            {
                const float fx = std::clamp(u - 0.5f, 0.0f, (float)(s.w - 1)), fy = std::clamp(v - 0.5f, 0.0f, (float)(s.h - 1));
                const int x0 = (int)fx, y0 = (int)fy, x1 = std::min(s.w - 1, x0 + 1), y1 = std::min(s.h - 1, y0 + 1);
                const float tx = fx - x0, ty = fy - y0;
                const uint32_t a = s.p[(size_t)y0 * s.w + x0], b = s.p[(size_t)y0 * s.w + x1];
                const uint32_t c2 = s.p[(size_t)y1 * s.w + x0], d = s.p[(size_t)y1 * s.w + x1];
                out = 0xFF000000u;
                for (int sh2 = 0; sh2 <= 16; sh2 += 8)
                {
                    const float va = (float)(a >> sh2 & 255), vb = (float)(b >> sh2 & 255), vc = (float)(c2 >> sh2 & 255), vd = (float)(d >> sh2 & 255);
                    const float val = (va * (1 - tx) + vb * tx) * (1 - ty) + (vc * (1 - tx) + vd * tx) * ty;
                    out |= (uint32_t)std::clamp((int)(val + 0.5f), 0, 255) << sh2;
                }
            }
            uint32_t& p = c.px[(size_t)py * c.w + px];
            p = cover >= 1.0f ? out : Glass::Over(p, out, (int)(cover * 255));
        }
    }
}
inline void Whole(Canvas& c, Src s, int x, int y, int w, int h, float r) { Image(c, s, 0, 0, (float)s.w, (float)s.h, x, y, w, h, true, r); }

// A box of the shape aspect (w / h) as large as fits in (x, y, w, h), centred.
inline void Fit(float aspect, int x, int y, int w, int h, int& ox, int& oy, int& ow, int& oh)
{
    ow = w;
    oh = (int)(w / aspect);
    if (oh > h) { oh = h; ow = (int)(h * aspect); }
    ox = x + (w - ow) / 2;
    oy = y + (h - oh) / 2;
}
inline void Screen(Canvas& c, Src s, float aspect, int x, int y, int w, int h, float r)
{
    int ox, oy, ow, oh;
    Fit(aspect, x, y, w, h, ox, oy, ow, oh);
    Whole(c, s, ox, oy, ow, oh, r);
}
// The top screen cut to the widescreen shape (as MERGED fills a wide screen with it).
inline void WideTop(Canvas& c, int x, int y, int w, int h, float r)
{
    const Src s = Top(c);
    const float want = (float)w / h, sh = std::min(192.0f, 256.0f / want);
    Image(c, s, 0, (192 - sh) / 2, 256, sh, x, y, w, h, true, r);
}
// The other screen drawn small into a corner of a view, edged so it reads as on top.
inline void Inset(Canvas& c, Src s, int x, int y, int w, int h, float k)
{
    Glass::Round(c, x - (int)k, y - (int)k, w + 2 * (int)k, h + 2 * (int)k, 3 * k, 0xE6FFFFFF);
    Whole(c, s, x, y, w, h, 2 * k);
}

// One choice's picture (Menu::P_*, art: its variant), in (x, y, w, h).
inline void Picture(Canvas& c, int kind, int art, int x, int y, int w, int h, float k)
{
    const float r = 7 * k;
    Glass::Round(c, x, y, w, h, r, 0xFF0E131A);
    const int pad = (int)(4 * k), ix = x + pad, iy = y + pad, iw = w - 2 * pad, ih = h - 2 * pad, gap = (int)(3 * k);
    const float ds = 256.0f / 192.0f;
    switch (kind)
    {
    case Menu::P_LAYOUT: // MERGED, HYBRID, SIDE, VERTICAL, TOP, BOTTOM
        if (art == 0)
        {
            WideTop(c, x, y, w, h, r);
            const int bw = (int)(w * 0.32f), bh = (int)(bw / ds);
            Inset(c, Bottom(c), x + w - bw - pad, y + h - bh - pad, bw, bh, k);
        }
        else if (art == 1)
        {
            Screen(c, Top(c), ds, ix, iy, (int)(iw * 0.7f), ih, 2 * k);
            Screen(c, Bottom(c), ds, ix + (int)(iw * 0.7f) + gap, iy, iw - (int)(iw * 0.7f) - gap, ih, 2 * k);
        }
        else if (art == 2)
        {
            Screen(c, Top(c), ds, ix, iy, iw / 2 - gap / 2, ih, 2 * k);
            Screen(c, Bottom(c), ds, ix + iw / 2 + gap / 2, iy, iw / 2 - gap / 2, ih, 2 * k);
        }
        else if (art == 3)
        {
            Screen(c, Top(c), ds, ix, iy, iw, ih / 2 - gap / 2, 2 * k);
            Screen(c, Bottom(c), ds, ix, iy + ih / 2 + gap / 2, iw, ih / 2 - gap / 2, 2 * k);
        }
        else Screen(c, art == 4 ? Top(c) : Bottom(c), ds, ix, iy, iw, ih, 2 * k);
        break;
    case Menu::P_LARGE: // AUTO, TOP, BOTTOM: the large screen and the other small beside it
    {
        const Src big = art == 2 ? Bottom(c) : Top(c), small = art == 2 ? Top(c) : Bottom(c);
        const int bw = (int)(iw * 0.68f);
        Screen(c, big, ds, ix, iy, bw, ih, 2 * k);
        Screen(c, small, ds, ix + bw + gap, iy, iw - bw - gap, ih, 2 * k);
        if (art == 0) // (AUTO: the two swap round, the touch screen brought forward while it is used)
        {
            const float cx = ix + bw + (iw - bw) * 0.5f, cy = iy + ih * 0.82f, s = std::max(3.0f, 6 * k);
            Glass::Round(c, (int)(cx - 2.2f * s), (int)(cy - 1.4f * s), (int)(4.4f * s), (int)(2.8f * s), 1.4f * s, 0xD0101820);
            Glass::DrawIcon(c, Glass::I_CHEVRON_L, cx - s, cy, s, 0xFFFFFFFF);
            Glass::DrawIcon(c, Glass::I_CHEVRON_R, cx + s, cy, s, 0xFFFFFFFF);
        }
        break;
    }
    case Menu::P_SECOND: // OFF, EXTRA TOUCH, DS SPLIT, DS SWAPPED, COMPANION: the phone's screen over its second display
    {
        const int mh = (int)(ih * 0.54f), sh = ih - mh - gap * 2;
        int mx, my, mw, mh2, sx, sy, sw, sh2;
        Fit(16.0f / 9.0f, ix, iy, iw, mh, mx, my, mw, mh2);
        Fit(ds, ix, iy + mh + gap * 2, iw, sh, sx, sy, sw, sh2);
        if (art == 2) Screen(c, Top(c), ds, mx, my, mw, mh2, 2 * k);
        else if (art == 3) Screen(c, Bottom(c), ds, mx, my, mw, mh2, 2 * k);
        else
        {
            WideTop(c, mx, my, mw, mh2, 2 * k);
            const int bw = (int)(mw * 0.3f), bh = (int)(bw / ds);
            Inset(c, Bottom(c), mx + mw - bw - gap, my + mh2 - bh - gap, bw, bh, k);
        }
        if (art == 1 || art == 2) Whole(c, Bottom(c), sx, sy, sw, sh2, 2 * k);
        else if (art == 3) Whole(c, Top(c), sx, sy, sw, sh2, 2 * k);
        else if (art == 4)
        {
            Glass::Round(c, sx, sy, sw, sh2, 2 * k, 0xFF1A2536);
            for (int i = 0; i < 4; i++)
            {
                const int ry = sy + sh2 * (i + 1) / 5 - (int)(2 * k);
                Glass::Round(c, sx + sw / 8, ry - (int)(2 * k), (int)(5 * k), (int)(5 * k), 2.5f * k, 0xFF8FB6FF);
                Glass::Round(c, sx + sw / 8 + (int)(9 * k), ry - (int)k, sw * (5 - i % 3) / 9, (int)(3 * k), 1.5f * k, 0xFF4E5E74);
            }
        }
        else Glass::Round(c, sx, sy, sw, sh2, 2 * k, 0xFF06080B);
        Glass::Round(c, sx - (int)k, sy - (int)k, sw + 2 * (int)k, sh2 + 2 * (int)k, 3 * k, 0, 0x66FFFFFF, k);
        break;
    }
    case Menu::P_ASPECT: // AUTO, 16:9, 16:10, 3:2, 4:3, in a screen the shape of this one
    {
        static const float ratios[] = { 0, 16.0f / 9, 16.0f / 10, 3.0f / 2, 4.0f / 3 };
        const float a = art <= 0 || art > 4 ? (float)w / h : ratios[art];
        Glass::Round(c, x, y, w, h, r, 0xFF040608);
        int ox, oy, ow, oh;
        Fit(a, x, y, w, h, ox, oy, ow, oh);
        const Src f = Frame(c);
        Image(c, f, 0, 0, (float)f.w, (float)f.h, ox, oy, ow, oh, true, ow == w ? r : 0);
        break;
    }
    case Menu::P_FULLSCREEN: // OFF (a window on the desktop), ON (the whole screen)
        if (art == 1) Whole(c, Frame(c), x, y, w, h, r);
        else
        {
            Glass::Round(c, x, y, w, h, r, 0xFF2C3B50);
            const int wx = x + (int)(w * 0.13f), wy = y + (int)(h * 0.12f), ww = (int)(w * 0.74f), wh = (int)(h * 0.76f), bar = std::max(3, (int)(h * 0.09f));
            Glass::Round(c, wx, wy, ww, bar + (int)(2 * k), 2 * k, 0xFFC9D2DD);
            for (int i = 0; i < 3; i++)
                Glass::Round(c, wx + ww - (i + 1) * bar, wy + bar / 4, bar / 2, bar / 2, bar / 4.0f, 0xFF6A7787);
            Whole(c, Frame(c), wx, wy + bar, ww, wh - bar, 0);
        }
        break;
    case Menu::P_SMOOTH: // OFF (sharp pixels), ON (blended), close up
    case Menu::P_FILTER: // OFF, SCANLINES, LCD GRID, close up
    {
        const Src s = Top(c);
        const float cw = 40, ch = cw * h / w, cx = 128 - cw / 2, cy = 112 - ch / 2;
        Image(c, s, cx, cy, cw, ch, x, y, w, h, kind == Menu::P_SMOOTH && art == 1, r);
        if (kind == Menu::P_FILTER && art > 0 && s.p)
        {
            const float step = w / cw;
            for (float ly = y; ly < y + h; ly += step)
                Rule(c, x + (int)(r * 0.5f), (int)(ly + step * 0.62f), w - (int)r, std::max(1, (int)(step * 0.38f)), art == 1 ? 0x80000000 : 0x60000000);
            if (art == 2)
                for (float lx = x; lx < x + w; lx += step)
                    Rule(c, (int)(lx + step * 0.7f), y + (int)(r * 0.5f), std::max(1, (int)(step * 0.3f)), h - (int)r, 0x60000000);
        }
        break;
    }
    case Menu::P_BATTLE: // CORNER, STRIP, PANEL
    {
        Whole(c, Frame(c), x, y, w, h, r);
        static const uint32_t col[] = { 0xF0E2574C, 0xF04C8BE2, 0xF04CC27A, 0xF0E2BE4C };
        if (art == 0)
        {
            const float s = h * 0.1f, cx = x + w - s * 3.2f, cy = y + h - s * 3.2f;
            const float off[4][2] = { { 0, -1.3f }, { 1.3f, 0 }, { 0, 1.3f }, { -1.3f, 0 } };
            for (int i = 0; i < 4; i++)
                Glass::Round(c, (int)(cx + off[i][0] * s - s / 2 * 1.4f), (int)(cy + off[i][1] * s - s / 2 * 1.4f), (int)(s * 1.4f), (int)(s * 1.4f), s * 0.7f, col[i]);
        }
        else if (art == 1)
        {
            const int bh = (int)(h * 0.2f), by = y + h - bh;
            Glass::Round(c, x, by, w, bh, 0, 0xC0101820);
            for (int i = 0; i < 4; i++)
                Glass::Round(c, x + w * (2 + 23 * i) / 96, by + bh / 5, w * 20 / 96, bh * 3 / 5, bh * 0.25f, col[i]);
        }
        else
        {
            const int pw = (int)(w * 0.3f), px = x + w - pw;
            Glass::Round(c, px, y, pw, h, 0, 0xC0101820);
            for (int i = 0; i < 4; i++)
                Glass::Round(c, px + pw / 8, y + h * (1 + 6 * i) / 25, pw * 3 / 4, h * 5 / 25, h * 0.05f, col[i]);
        }
        break;
    }
    case Menu::P_FPS:
        Whole(c, Frame(c), x, y, w, h, r);
        if (art == 1)
        {
            const int px = std::max(8, (int)(h * 0.14f));
            const int bw = W("60 FPS", px, true, false) + px, bh = px + px / 2;
            Glass::Round(c, x + pad, y + pad, bw, bh, 3 * k, 0xC8000000);
            T(c, x + pad + px / 2, y + pad + bh - px / 3 - 1, "60 FPS", px, 0xFF7CF29A, true, false);
        }
        break;
    case Menu::P_COMPANION:
        Whole(c, Frame(c), x, y, w, h, r);
        if (art == 1)
        {
            const int pw = (int)(w * 0.36f), px = x + w - pw - pad;
            Glass::Round(c, px, y + pad, pw, h - 2 * pad, 4 * k, 0xD8162131, 0x55FFFFFF, k);
            for (int i = 0; i < 4; i++)
            {
                const int ry = y + pad + (h - 2 * pad) * (2 * i + 1) / 8;
                const int d = std::max(3, (int)(h * 0.08f));
                Glass::Round(c, px + pw / 8, ry - d / 2, d, d, d / 2.0f, 0xFF8FB6FF);
                Glass::Round(c, px + pw / 8 + d + (int)(2 * k), ry - d / 4, pw * (4 - i % 2) / 8, std::max(2, d / 2), d / 4.0f, 0xFF52637A);
            }
        }
        break;
    case Menu::P_PARTY:
        Whole(c, Frame(c), x, y, w, h, r);
        if (art == 1)
        {
            const int pw = (int)(w * 0.34f), px = x + w - pw - pad, ph = (int)(h * 0.66f);
            Glass::Round(c, px, y + pad, pw, ph, 3 * k, 0xCC101722);
            static const float hp[] = { 1.0f, 0.8f, 0.62f, 0.95f, 0.2f, 0.7f };
            for (int i = 0; i < 6; i++)
            {
                const int ry = y + pad + (ph * (i + 1)) / 7;
                const int bw = pw - pw / 4;
                Glass::Round(c, px + pw / 8, ry - (int)(2 * k), bw / 2, std::max(1, (int)(1.6f * k)), 0, 0xFFC7CFD8);
                Glass::Round(c, px + pw / 8, ry, bw, std::max(2, (int)(2.4f * k)), 0, 0xFF394653);
                Glass::Round(c, px + pw / 8, ry, (int)(bw * hp[i]), std::max(2, (int)(2.4f * k)), 0, hp[i] < 0.25f ? 0xFFE05050 : 0xFF45C881);
            }
        }
        break;
    default: Whole(c, Frame(c), x, y, w, h, r); break;
    }
}
} // namespace SettingsArt

inline int Menu::FirstRow() const
{
    for (size_t i = 0; i < items.size(); i++)
        if (items[i].kind != K_SECTION && items[i].kind != K_BACK) return (int)i;
    return 0;
}

inline int Menu::UpdateSettings(const MenuInput& in, int& dir)
{
    const int n = (int)items.size();
    if (selected < 0 || selected >= n) selected = FirstRow();
    auto step = [&](int d) {
        int i = selected;
        for (int k = 0; k < n; k++)
        {
            i = (i + d + n) % n;
            if (items[i].kind != K_SECTION) { selected = i; return; }
        }
    };
    if (in.up) step(-1);
    if (in.down) step(1);
    const MenuItem& it = items[selected];
    if (it.disabled || it.kind == K_SECTION) return -1;
    if (in.accept) return selected; // (a toggle turns over, a picker steps on, a link or Back opens)
    if (in.left || in.right)
    {
        const int d = in.left ? -1 : 1;
        // (Off is the left pill and On the right: left turns it off, right on, each only once.)
        if (it.kind == K_TOGGLE) { if ((d < 0) == (it.choice == 0)) return -1; dir = d; return selected; }
        if (it.kind == K_PICKER || (it.kind == K_PLAIN && !it.value.empty() && !it.info)) { dir = d; return selected; }
        if (it.kind == K_LINK && d > 0) return selected;
    }
    return -1;
}

inline int Menu::SettingsItemAt(int x, int y) const
{
    if (drawnW <= 0 || x < drawnX || x >= drawnX + drawnW || y < drawnY || y >= drawnY + drawnH) return -2;
    if (x >= backX && x < backX + backS && y >= backY && y < backY + backS)
        for (size_t i = 0; i < items.size(); i++)
            if (items[i].kind == K_BACK) return (int)i;
    if (x < rowX || x >= rowX + rowW) return -1;
    for (size_t i = 0; i < items.size() && i < rowY.size(); i++)
        if (rowY[i] >= 0 && items[i].kind != K_SECTION && y >= rowY[i] && y < rowY[i] + rowH) return (int)i;
    return -1;
}

inline void Menu::SettingsTap(int item, int x, MenuInput& input)
{
    if (item == -2) { input.back = true; return; }
    if (item < 0) return;
    selected = item;
    const MenuItem& it = items[item];
    // (A pill tapped: that one. A picker: its left half back, its right half on.)
    if (it.kind == K_TOGGLE || it.kind == K_PICKER)
    {
        if (x >= ctlX) { if (x < ctlX + ctlW / 2) input.left = true; else input.right = true; }
        else if (it.kind == K_PICKER) input.right = true;
        else input.accept = true; // (a toggle's name: turned over)
    }
    else input.accept = true;
}

inline void Menu::DrawSettings(Canvas& c, const std::string& title, const std::string& hint) const
{
    using namespace SettingsArt;
    const bool solid = Glass::Backdrop(c);
    const int n = (int)items.size();
    if (selected < 0 || selected >= n) return;
    const bool wide = c.w >= c.h * 1.2f;
    float pw, ph, k;
    if (wide)
    {
        pw = std::min(c.w * 0.76f, c.h * 1.62f);
        ph = std::min(c.h * 0.84f, pw * 0.62f);
        k = ph / 645.0f;
    }
    else
    {
        pw = c.w * 0.94f;
        ph = c.h * 0.9f;
        k = std::min(pw / 560.0f, ph / 1000.0f);
    }
    k = std::max(k, 0.42f);
    auto K = [&](float v) { return (int)std::lround(v * k); };
    const int px0 = (int)((c.w - pw) / 2), py0 = (int)((c.h - ph) / 2), W0 = (int)pw, H0 = (int)ph;
    drawnX = px0; drawnY = py0; drawnW = W0; drawnH = H0; drawnLineH = 0; drawnFirstY = 0;

    // The panel.
    if (solid) Glass::Frost(c, px0, py0, W0, H0, 28 * k);
    else Glass::Round(c, px0, py0, W0, H0, 28 * k, GlassShade); // (no blurred game to frost: the old shade)
    Glass::Round(c, px0, py0, W0, H0, 28 * k, Panel, PanelEdge, std::max(1.0f, 1.4f * k));

    // Header: back button, the page's icon, its title, a line under them.
    backS = K(54); backX = px0 + K(28); backY = py0 + K(18);
    const bool backChosen = items[selected].kind == K_BACK;
    if (backChosen) GlowRound(c, backX, backY, backS, backS, 14 * k, k);
    Glass::Round(c, backX, backY, backS, backS, 14 * k, backChosen ? RowChosen : Row, backChosen ? ChosenEdge : PanelEdge, backChosen ? 2 * k : std::max(1.0f, k));
    Glass::DrawIcon(c, Glass::I_BACK, backX + backS * 0.5f, backY + backS * 0.5f, 13 * k, GlassText);
    const int iconX = backX + backS + K(18), iconS = K(58), iconY = py0 + K(16);
    Glass::Round(c, iconX, iconY, iconS, iconS, 15 * k, Row, PanelEdge, std::max(1.0f, k));
    if (pageIcon >= 0) Glass::DrawIcon(c, pageIcon, iconX + iconS * 0.5f, iconY + iconS * 0.5f, 17 * k, GlassText);
    T(c, iconX + iconS + K(20), iconY + K(41), title, K(34), GlassText, true);
    FadeLine(c, px0 + K(28), px0 + W0 - K(28), py0 + K(92), std::max(1, K(1.5f)), 0xFFFFFF, 0x90);

    // Where the rows and the preview go.
    const int top = py0 + K(108), bottom = py0 + H0 - K(24);
    int listX, listW, listBottom, pvX, pvY, pvW, pvH;
    if (wide)
    {
        listX = px0 + K(30);
        listW = (int)(W0 * 0.605f);
        listBottom = bottom;
        pvX = listX + listW + K(30);
        pvY = top + K(4);
        pvW = px0 + W0 - K(22) - pvX;
        pvH = bottom - pvY;
        Rule(c, listX + listW + K(18), top + K(8), std::max(1, K(1)), bottom - top - K(16), Line);
    }
    else
    {
        listX = px0 + K(24);
        listW = W0 - K(48);
        listBottom = top + (int)((bottom - top) * 0.56f);
        pvX = listX;
        pvY = listBottom + K(16);
        pvW = listW;
        pvH = bottom - pvY;
    }

    // The rows: their places down the list, a section's heading (and a line before every one but
    // the first) taking a little more room than a row.
    // (Closer together, down to nine tenths, before the list has to scroll.)
    const int avail = listBottom - top;
    std::vector<int> at(n, -1);
    int total = 0, pitch = 0, headH = 0, sepH = 0;
    for (float squeeze : { 1.0f, 0.95f, 0.9f })
    {
        rowH = K(40 * squeeze); pitch = K(44 * squeeze); headH = K(46 * squeeze); sepH = K(14 * squeeze);
        total = 0;
        bool firstSection = true;
        for (int i = 0; i < n; i++)
        {
            if (items[i].kind == K_BACK) continue;
            if (items[i].kind == K_SECTION)
            {
                if (!firstSection) total += sepH;
                firstSection = false;
                at[i] = total;
                total += headH;
            }
            else { at[i] = total; total += pitch; }
        }
        if (total <= avail) break;
    }
    rowX = listX; rowW = listW;
    if (total <= avail) scrollPx = 0;
    else if (at[selected] >= 0)
    {
        // (The chosen row kept in view; its section's heading too when that fits.)
        int y0 = at[selected];
        for (int i = selected - 1; i >= 0; i--)
            if (items[i].kind == K_SECTION) { if (at[selected] - at[i] <= avail - pitch) y0 = at[i]; break; }
            else if (items[i].kind != K_BACK) break;
        if (y0 < scrollPx) scrollPx = y0;
        if (at[selected] + rowH > scrollPx + avail) scrollPx = at[selected] + rowH - avail;
        scrollPx = std::clamp(scrollPx, 0, total - avail);
    }
    rowY.assign(n, -1);
    ctlW = K(186);
    ctlX = listX + listW - K(14) - ctlW;
    bool firstSection = true;
    for (int i = 0; i < n; i++)
    {
        const MenuItem& it = items[i];
        if (at[i] < 0) continue;
        const int y = top + at[i] - scrollPx;
        const bool isSection = it.kind == K_SECTION;
        if (isSection && !firstSection && y - sepH / 2 >= top && y - sepH / 2 < listBottom)
            Rule(c, listX, y - sepH / 2 - 1, listW, std::max(1, K(1)), Line);
        if (isSection) firstSection = false;
        if (y < top || y + (isSection ? headH : rowH) > listBottom) continue; // (scrolled out of view)
        rowY[i] = y;
        if (isSection)
        {
            if (it.icon >= 0) Glass::DrawIcon(c, it.icon, listX + K(22), y + headH * 0.52f, 12 * k, GlassText);
            T(c, listX + K(50), y + headH * 2 / 3 + K(1), it.label, K(22), GlassText, true);
            continue;
        }
        const bool sel = i == selected, off = it.disabled;
        if (sel) GlowRound(c, listX, y, listW, rowH, 10 * k, k);
        Glass::Round(c, listX, y, listW, rowH, 10 * k, sel ? RowChosen : Row, sel ? ChosenEdge : RowEdge, sel ? 2.2f * k : std::max(1.0f, k));
        const uint32_t ink = off ? GlassOff : GlassText;
        if (it.icon >= 0) Glass::DrawIcon(c, it.icon, listX + K(36), y + rowH * 0.5f, 11 * k, ink);
        const int base = y + rowH / 2 + K(7); // (the baseline of the row's text)
        T(c, listX + K(76), base, it.label, K(20), ink);
        if (it.kind == K_TOGGLE)
        {
            const int hw = (ctlW - K(6)) / 2, ph2 = std::min(rowH - K(6), K(32)), py = y + (rowH - ph2) / 2;
            for (int s = 0; s < 2; s++)
            {
                const bool on = s == it.choice;
                const int x = ctlX + s * (hw + K(6));
                Glass::Round(c, x, py, hw, ph2, ph2 * 0.5f, on ? (off ? 0x50FFFFFF : Pill) : PillOff, on ? (off ? 0x40FFFFFF : PillEdge) : PillOffEdge, std::max(1.0f, k));
                const std::string& name = s < (int)it.choices.size() ? it.choices[s] : (s ? std::string("ON") : std::string("OFF"));
                TCentred(c, x + hw / 2, py + ph2 / 2 + K(6), name, K(18), off ? GlassOff : on ? PillInk : 0xFFDCE2EA, on);
            }
        }
        else if (it.kind == K_PICKER || (it.kind == K_PLAIN && !it.value.empty()))
        {
            const bool arrows = it.kind == K_PICKER && !off;
            if (arrows)
            {
                Glass::DrawIcon(c, Glass::I_CHEVRON_L, ctlX + K(14), y + rowH * 0.5f, 9 * k, GlassText);
                Glass::DrawIcon(c, Glass::I_CHEVRON_R, ctlX + ctlW - K(14), y + rowH * 0.5f, 9 * k, GlassText);
            }
            int vpx = K(19);
            while (vpx > 8 && W(it.value, vpx, true) > ctlW - K(56)) vpx--;
            if (it.kind == K_PICKER) TCentred(c, ctlX + ctlW / 2, base, it.value, vpx, ink, true);
            else T(c, ctlX + ctlW - W(it.value, vpx), base, it.value, vpx, off ? GlassOff : GlassHint);
        }
        else if (it.kind == K_LINK)
            Glass::DrawIcon(c, Glass::I_CHEVRON_R, listX + listW - K(26), y + rowH * 0.5f, 9 * k, ink);
    }
    if (total > avail) // (a thin bar: where the rows in view are among them all)
    {
        const int bx = listX + listW + K(6), bw = std::max(2, K(3));
        Glass::Round(c, bx, top, bw, avail, bw * 0.5f, 0x18FFFFFF);
        const int th = std::max(K(24), avail * avail / total), ty = top + (avail - th) * scrollPx / std::max(1, total - avail);
        Glass::Round(c, bx, ty, bw, th, bw * 0.5f, 0x70FFFFFF);
    }

    // The preview of the chosen row.
    const MenuItem& it = items[selected];
    Glass::Round(c, pvX, pvY, pvW, pvH, 18 * k, Row, 0x24FFFFFF, std::max(1.0f, k));
    const int pad = K(20), innerW = pvW - 2 * pad;
    const std::string name = it.kind == K_BACK ? std::string("BACK") : it.label;
    const std::string desc = it.kind == K_BACK ? std::string("Back to Settings.") : it.desc;
    T(c, pvX + pad, pvY + K(42), name, K(25), GlassText, true);
    int y = pvY + K(80);
    for (const std::string& line : Wrap(desc, K(18), innerW, wide ? 4 : 3))
    {
        T(c, pvX + pad, y, line, K(18), Desc, false, false);
        y += K(26);
    }
    int regionTop = y + K(10), regionBottom = pvY + pvH - pad;
    if (!it.tip.empty())
    {
        const int tipPx = K(17), tipLine = K(23);
        const auto lines = Wrap(it.tip, tipPx, innerW - K(34), 3);
        const int tipTop = pvY + pvH - pad - (int)lines.size() * tipLine;
        Rule(c, pvX + pad, tipTop - K(18), innerW, std::max(1, K(1)), Line);
        Glass::DrawIcon(c, Glass::I_INFO, pvX + pad + K(12), tipTop + K(6), 11 * k, Desc);
        for (size_t i = 0; i < lines.size(); i++) T(c, pvX + pad + K(34), tipTop + K(12) + (int)i * tipLine, lines[i], tipPx, Desc, false, false);
        regionBottom = tipTop - K(34);
    }
    // The choices: a picture of each, the one on now edged and its name in a violet pill.
    const int choices = (int)it.choices.size();
    if (it.kind != K_BACK && it.kind != K_LINK && it.preview > P_NONE && choices > 0 && regionBottom - regionTop > K(60))
    {
        const int cols = choices <= 3 ? choices : 3, rows = (choices + cols - 1) / cols;
        const int gapX = K(14), gapY = K(12), labelH = K(28), labelGap = K(10);
        const int regionW = innerW, regionH = regionBottom - regionTop;
        const int cellW = (regionW - (cols - 1) * gapX) / cols, cellH = (regionH - (rows - 1) * gapY) / rows;
        const float aspect = it.preview == P_SECOND ? 0.95f : std::clamp((float)c.w / c.h, 1.3f, 2.1f);
        int picW = std::min((int)(cellW * 0.94f), (int)((cellH - labelH - labelGap) * aspect));
        // (Two side by side, as wide as a third of the panel each at most: the design's size.)
        picW = std::min(picW, (int)(innerW * (cols <= 2 ? 0.44f : 0.32f)));
        const int picH = (int)(picW / aspect);
        const bool pictures = picH >= K(26);
        const int blockH = rows * ((pictures ? picH + labelGap : 0) + labelH) + (rows - 1) * gapY;
        const int y0 = regionTop + std::max(0, (regionH - blockH) / 2);
        for (int i = 0; i < choices; i++)
        {
            const int col = i % cols, row = i / cols;
            const int cx = pvX + pad + col * (cellW + gapX) + cellW / 2;
            int cy = y0 + row * ((pictures ? picH + labelGap : 0) + labelH + gapY);
            const bool on = i == it.choice;
            if (pictures)
            {
                const int x = cx - picW / 2;
                if (on && !it.disabled) GlowRound(c, x - K(4), cy - K(4), picW + K(8), picH + K(8), 12 * k, k);
                Glass::Round(c, x - K(4), cy - K(4), picW + K(8), picH + K(8), 12 * k, on ? 0x30FFFFFF : 0x1A000000,
                             on ? ChosenEdge : 0x48FFFFFF, on ? 2.4f * k : std::max(1.0f, 1.2f * k));
                Picture(c, it.preview, i < (int)it.art.size() ? it.art[i] : i, x, cy, picW, picH, k);
                cy += picH + labelGap;
            }
            int lpx = K(16);
            while (lpx > 8 && W(it.choices[i], lpx, on) + K(30) > cellW) lpx--;
            const int lw = W(it.choices[i], lpx, on) + K(30);
            Glass::Round(c, cx - lw / 2, cy, lw, labelH, labelH * 0.5f, on ? Pill : PillOff, on ? PillEdge : PillOffEdge, std::max(1.0f, k));
            TCentred(c, cx, cy + labelH / 2 + lpx * 7 / 20, it.choices[i], lpx, on ? PillInk : 0xFFDCE2EA, on);
        }
    }
    else if (it.kind != K_BACK && !it.value.empty() && regionBottom - regionTop > K(40))
    {
        // (No pictures, as for LANGUAGE: the choice on now, large.)
        const int vpx = K(22), lw = W(it.value, vpx, true) + K(44), lh = K(42);
        const int cx = pvX + pvW / 2, cy = regionTop + (regionBottom - regionTop - lh) / 2;
        Glass::Round(c, cx - lw / 2, cy, lw, lh, lh * 0.5f, Pill, PillEdge, std::max(1.0f, k));
        TCentred(c, cx, cy + lh / 2 + vpx * 7 / 20, it.value, vpx, PillInk, true);
    }

    // The buttons' hint under the panel, if there is room.
    if (!hint.empty() && c.h - (py0 + H0) > K(30))
        TCentred(c, c.w / 2, py0 + H0 + K(30), hint, K(15), GlassHint);
    if (solid) Glass::Opaque(c);
}
