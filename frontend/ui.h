// Software-drawn overlay: pause menu, touch cursor and status messages.
// Drawn into an ARGB buffer the size of the output, so the window and the
// headless screenshots show the same thing.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "ui_text.h"

struct Canvas
{
    int w = 0, h = 0;
    std::vector<uint32_t> px; // ARGB; alpha 0 means "show the game underneath"
    // The paused game blurred (glass.h, Glass::MakeBackdrop) while the menu is open; empty otherwise.
    std::vector<uint32_t> backdrop;
    // The same moment unblurred (w x h), and the DS's two screens (256 x 192 each, top then bottom):
    // the pictures in a settings page's preview (ui_settings.h) are made from them. Empty when
    // there is nothing to read (the preview then draws plain shapes).
    std::vector<uint32_t> sharp, dsTop, dsBottom;

    void Resize(int nw, int nh)
    {
        if (nw != w || nh != h)
        {
            w = nw;
            h = nh;
            px.assign((size_t)w * h, 0);
            x0d = y0d = 0;
            x1d = w;
            y1d = h;
        }
    }
    // The rectangle that has been drawn into since the last Clear. Only that part is
    // cleared and sent to the screen: the canvas is as large as the window.
    int x0d = 0, y0d = 0, x1d = 0, y1d = 0;
    bool Dirty() const { return x1d > x0d && y1d > y0d; }
    void Clear()
    {
        for (int yy = y0d; yy < y1d; yy++)
            std::fill(px.begin() + (size_t)yy * w + x0d, px.begin() + (size_t)yy * w + x1d, 0);
        x0d = w;
        y0d = h;
        x1d = y1d = 0;
    }

    void Fill(int x, int y, int rw, int rh, uint32_t argb)
    {
        int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
        int x1 = x + rw > w ? w : x + rw, y1 = y + rh > h ? h : y + rh;
        if (x1 <= x0 || y1 <= y0) return;
        if (x0 < x0d) x0d = x0;
        if (y0 < y0d) y0d = y0;
        if (x1 > x1d) x1d = x1;
        if (y1 > y1d) y1d = y1;
        for (int yy = y0; yy < y1; yy++)
            for (int xx = x0; xx < x1; xx++)
                px[(size_t)yy * w + xx] = argb;
    }

    // 5x7 glyphs, one row per byte, bit 4 is the leftmost pixel.
    static const uint8_t* Glyph(char c)
    {
        static const uint8_t font[][7] = {
            { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }, // A
            { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E }, // B
            { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E }, // C
            { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E }, // D
            { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F }, // E
            { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 }, // F
            { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F }, // G
            { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }, // H
            { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E }, // I
            { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C }, // J
            { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 }, // K
            { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F }, // L
            { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 }, // M
            { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 }, // N
            { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E }, // O
            { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 }, // P
            { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D }, // Q
            { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 }, // R
            { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E }, // S
            { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 }, // T
            { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E }, // U
            { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 }, // V
            { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11 }, // W
            { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 }, // X
            { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 }, // Y
            { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F }, // Z
            { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E }, // 0
            { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E }, // 1
            { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F }, // 2
            { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E }, // 3
            { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 }, // 4
            { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E }, // 5
            { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E }, // 6
            { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 }, // 7
            { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E }, // 8
            { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C }, // 9
            { 0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00 }, // :
            { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 }, // -
            { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 }, // +
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06 }, // .
            { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 }, // /
            { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 }, // <
            { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 }, // >
            { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 }, // !
            { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 }, // (
            { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 }, // )
            // PlayStation's button symbols (controls.h, PS_*): cross, circle, square, triangle. Smaller
            // than the letters, so the cross and the circle are not taken for an X and an O.
            { 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x00 },
            { 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00 },
            { 0x00, 0x1F, 0x11, 0x11, 0x11, 0x1F, 0x00 },
            { 0x00, 0x04, 0x0A, 0x0A, 0x11, 0x1F, 0x00 },
            { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 }, // % (VOLUME)
        };
        if (c == '\'') { static const uint8_t apostrophe[] = { 4, 4, 8, 0, 0, 0, 0 }; return apostrophe; }
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c >= 'A' && c <= 'Z') return font[c - 'A'];
        if (c >= '0' && c <= '9') return font[26 + c - '0'];
        static const char punct[] = ":-+./<>!()\x01\x02\x03\x04%";
        const char* p = c ? strchr(punct, c) : nullptr;
        return p ? font[36 + (p - punct)] : nullptr;
    }

    // (In the interface's language, UiLang: its UTF-8 letters are one character each.)
    static int TextWidth(const std::string& s, int scale)
    {
        return UiLang::Units(UiLang::Tr(s)) * scale;
    }

    static int TextWidthRaw(const std::string& text,int scale) { return UiLang::Units(text)*scale; }
    void TextRaw(int x,int y,const std::string& text,int scale,uint32_t argb) { UiText::DrawRaw(*this,x,y,text,scale,argb); }
    void Text(int x,int y,const std::string& text,int scale,uint32_t argb)
    {
        UiText::Draw(*this,x,y,text,scale,argb);
    }

};

// The accent colours of COLOUR (the game list's settings, GAME LIST): the game list's lines and
// frame (launcher.h, ThemeNamed) and the pause menu's chosen row, its box and text shaded from it.
inline uint32_t AccentNamed(const std::string& name)
{
    static const struct { const char* name; uint32_t argb; } colours[] = {
        { "red", 0xFFE0242C }, { "blue", 0xFF2A7BE0 }, { "green", 0xFF2EB85C }, { "purple", 0xFF9B4DE0 },
        { "gold", 0xFFE0A82C }, { "pink", 0xFFE0479A }, { "teal", 0xFF1FB5A8 },
    };
    for (const auto& c : colours)
        if (name == c.name) return c.argb;
    return colours[0].argb;
}

#include "glass.h"
#include "ui_clean_font.h"

// Button presses the menu understands, edge-triggered.
struct MenuInput
{
    bool up = false, down = false, left = false, right = false, accept = false, back = false;
};

struct MenuItem
{
    std::string label;
    std::string value; // shown right-aligned; empty for plain actions
    bool info = false; // the value is only shown (no < > around it): nothing to change here
    int slider = -1;   // 0 to 10: also drawn as a bar of ten blocks, that many lit (VOLUME)
    int icon = -1;     // Glass::Icon, for a tile (Menu::columns)
    bool disabled = false; // shown greyed; choosing it does nothing (this game or engine has no such thing)
    // Settings pages (Menu::style 1, ui_settings.h): what the row is (Menu::K_*), its choices as the
    // preview names them and which is on now, the picture drawn for each (Menu::P_*, art: the
    // picture's variant for each choice), and the preview's description and tip.
    int kind = 0;
    std::vector<std::string> choices;
    std::vector<int> art;
    int choice = -1;
    int preview = -1;
    std::string desc, tip;
};

struct Menu
{
    bool open = false;
    int selected = 0;
    std::vector<MenuItem> items;
    // 0: a list (every page but one). More: the items are tiles in rows of that many (the pause
    // menu's first page: eight glass tiles, four to a row; two to a row on a portrait screen).
    int columns = 0;
    mutable int drawnColumns = 0; // (as last drawn: Update moves up and down by this many)
    // 1: a settings page (ui_settings.h): a header with a back button, the page's icon and title,
    // rows in sections on the left and a preview of the chosen row on the right.
    int style = 0;
    int pageIcon = -1;
    enum { K_PLAIN, K_TOGGLE, K_PICKER, K_LINK, K_SECTION, K_BACK };
    // A preview's pictures (MenuItem::preview), one per choice (its art: which variant).
    enum { P_NONE, P_LAYOUT, P_LARGE, P_SECOND, P_ASPECT, P_FULLSCREEN, P_SMOOTH, P_BATTLE, P_FPS, P_COMPANION, P_PARTY, P_FILTER };

    // CLEAN text (on: Noto Sans, ui_clean_font.h, in title case) or the pixel font (the Retro look).
    // Japanese and Korean menus keep the pixel font, which has their letters.
    static inline bool Clean = true;
    static bool UseClean() { return Clean && UiLang::lang < 5; }
    static int Px(int scale, bool big = false) { return (big ? 14 : 12) * scale; }
    static int TW(const std::string& s, int scale, bool big = false)
    {
        return UseClean() ? CleanFont::Width(CleanFont::Title(UiLang::Tr(s)), Px(scale, big), big) : Canvas::TextWidth(s, scale);
    }
    // (y: the top of a pixel-font line; clean text sits on the same baseline as its capitals.)
    static void TX(Canvas& c, int x, int y, const std::string& s, int scale, uint32_t col, bool big = false)
    {
        if (UseClean()) CleanFont::Draw(c, x, y + 7 * scale, CleanFont::Title(UiLang::Tr(s)), Px(scale, big), col, big);
        else c.Text(x, y, s, scale, col);
    }
    static void TXRaw(Canvas& c, int x, int y, const std::string& s, int scale, uint32_t col)
    {
        if (UseClean()) CleanFont::Draw(c, x, y + 7 * scale, s, Px(scale), col);
        else c.TextRaw(x, y, s, scale, col);
    }

    // Returns the index of an item that was activated this frame, or -1.
    // dir is -1/+1 when left/right was used on it, 0 for accept.
    int Update(const MenuInput& in, int& dir)
    {
        dir = 0;
        if (!open || items.empty()) return -1;
        int n = (int)items.size();
        if (style == 1) return UpdateSettings(in, dir);
        if (columns > 0)
        {
            const int columns = drawnColumns > 0 ? drawnColumns : this->columns;
            if (in.left) selected = (selected + n - 1) % n;
            if (in.right) selected = (selected + 1) % n;
            if (in.up) selected = (selected + n - columns % n) % n;
            if (in.down) selected = (selected + columns) % n;
            return in.accept && !items[selected].disabled ? selected : -1;
        }
        if (in.up) selected = (selected + n - 1) % n;
        if (in.down) selected = (selected + 1) % n;
        if (in.accept) return items[selected].disabled ? -1 : selected;
        if (in.left && !items[selected].value.empty() && !items[selected].disabled)
        {
            dir = -1;
            return selected;
        }
        if (in.right && !items[selected].value.empty() && !items[selected].disabled)
        {
            dir = 1;
            return selected;
        }
        return -1;
    }

    // Where the box and its rows were last drawn (Draw), in the canvas's pixels, and the first
    // row shown (a list longer than the screen scrolls with the chosen row: CHEATS).
    mutable int drawnX = 0, drawnY = 0, drawnW = 0, drawnH = 0, drawnLineH = 0, drawnFirstY = 0;
    mutable int scroll = 0;
    // Where a slider's bar was drawn (the same on every row): its left edge and one block's step.
    mutable int barX = 0, barStep = 0;
    // Where each tile was drawn (columns > 0): x, y, size.
    mutable std::vector<int> tileX, tileY;
    mutable int tileSize = 0;

    // The value a tap at x on a slider's row stands for (0 to 10: the block tapped and those
    // before it; left of the bar, 0), or -1 for a tap beside the bar.
    int SliderAt(int x) const
    {
        if (barStep <= 0 || x >= barX + 10 * barStep + barStep / 2) return -1;
        if (x < barX) return x >= barX - 2 * barStep ? 0 : -1;
        return std::min(10, (x - barX) / barStep + 1);
    }

    // The row (or tile) at (x, y) of the canvas the menu was drawn on (touch screens): its index,
    // -1 for a point in the box but on no row, -2 for a point outside the box.
    int ItemAt(int x, int y) const
    {
        if (style == 1) return SettingsItemAt(x, y);
        if (drawnW <= 0 || x < drawnX || x >= drawnX + drawnW || y < drawnY || y >= drawnY + drawnH) return -2;
        if (columns > 0)
        {
            for (size_t i = 0; i < tileX.size() && i < items.size(); i++)
                if (x >= tileX[i] && x < tileX[i] + tileSize && y >= tileY[i] && y < tileY[i] + tileSize) return (int)i;
            return -1;
        }
        if (drawnLineH <= 0 || y < drawnFirstY) return -1;
        const int i = scroll + (y - drawnFirstY) / drawnLineH;
        return i < (int)items.size() ? i : -1;
    }

    // Informational values describe a row; they are not left/right settings. A tap
    // activates the row (party details, dated state slots, backup details) like Enter.
    void Tap(int x,int y,MenuInput& input,int& sliderValue)
    {
        const int item=ItemAt(x,y);
        if(style==1){SettingsTap(item,x,input);return;}
        if(item==-2)input.back=true;
        else if(item>=0)
        {
            selected=item;
            if(columns>0)input.accept=true;
            else if(items[item].slider>=0 && SliderAt(x)>=0){sliderValue=SliderAt(x);input.accept=true;}
            else if(items[item].value.empty()||items[item].info)input.accept=true;
            else if(x<drawnX+drawnW/3)input.left=true;
            else input.right=true;
        }
    }

    // note: a few lines of text in a strip under the box (CHEATS: what the database says about
    // the chosen cheat), wrapped to the box's width.
    // The accent (COLOUR, AccentNamed): kept for the game list; the glass menu is white on the game.
    static inline uint32_t Accent = 0xFFE0242C;

    // The glass colours (glass.h): panels a little lighter than the blurred game, the chosen row
    // or tile lighter still with a bright edge, text white, hints a soft grey.
    static constexpr uint32_t GlassPanel = 0x2BFFFFFF, GlassEdge = 0x3DFFFFFF;
    static constexpr uint32_t GlassChosen = 0x4AFFFFFF, GlassChosenEdge = 0xF2FFFFFF;
    static constexpr uint32_t GlassText = 0xFFF2F5F9, GlassHint = 0xFFB9C3CF, GlassOff = 0xFF7E8894;
    // Lists: a little shade under the glass so white text reads over bright parts of the game,
    // and the chosen row an outline with a faint fill (a bright fill hid its text).
    static constexpr uint32_t GlassShade = 0x4D000000, GlassRow = 0x1FFFFFFF;
    // Frosted glass (Glass::Frost, the blurred game made lighter and more colourful) with a faint
    // white over it and a bright edge; the old grey shade where there is no blurred game to frost.
    static constexpr uint32_t FrostTint = 0x0FFFFFFF, FrostEdge = 0x5CFFFFFF, FrostChosen = 0x2EFFFFFF;
    static void FrostBox(Canvas& c, bool solid, int x, int y, int w, int h, float r, bool chosen = false, float edge = 1.0f)
    {
        if (solid) Glass::Frost(c, x, y, w, h, r);
        else Glass::Round(c, x, y, w, h, r, GlassShade);
        Glass::Round(c, x, y, w, h, r, chosen ? FrostChosen : FrostTint, chosen ? GlassChosenEdge : FrostEdge, edge);
    }

    // The first page's tiles (columns > 0): rows of glass squares, each an icon over its name,
    // centred on the blurred game with no panel round them; "FULLDEX" and "PAUSED" above.
    void DrawTiles(Canvas& c, const std::string& hint) const
    {
        const bool solid = Glass::Backdrop(c);
        int scale = std::min(c.w, c.h) / 240;
        if (scale < 1) scale = 1;
        // (Two to a row on a portrait screen, so the tiles stay large enough to tap.)
        const int cols = c.h > c.w ? std::min(2, columns) : columns;
        drawnColumns = cols;
        const int n = (int)items.size(), rows = (n + cols - 1) / cols;
        const float avW = c.w * 0.86f, avH = c.h * (c.h > c.w ? 0.7f : 0.6f);
        const int s = (int)std::min(avW / (cols + 0.1f * (cols - 1)), avH / (rows + 0.1f * (rows - 1)));
        const int gap = s / 10;
        const int gridW = cols * s + (cols - 1) * gap, gridH = rows * s + (rows - 1) * gap;
        const int gx = (c.w - gridW) / 2, gy = (c.h - gridH) / 2;
        tileX.assign(n, 0); tileY.assign(n, 0); tileSize = s;
        drawnX = gx - gap; drawnY = gy - gap; drawnW = gridW + 2 * gap; drawnH = gridH + 2 * gap;
        drawnLineH = 0; drawnFirstY = 0; scroll = 0;
        const int headY = gy - 9 * scale - gap;
        TXRaw(c, gx, headY, "Fulldex", scale, GlassText);
        // (One size for every tile's name: the largest at which the longest still fits.)
        int ls = scale;
        for (int i = 0; i < n; i++)
            while (ls > 1 && TW(items[i].label, ls, true) > s * 0.88f) ls--;
        TX(c, gx + gridW - TW("PAUSED", scale), headY, "PAUSED", scale, GlassHint);
        for (int i = 0; i < n; i++)
        {
            const int x = gx + (i % cols) * (s + gap), y = gy + (i / cols) * (s + gap);
            tileX[i] = x; tileY[i] = y;
            const bool sel = i == selected, off = items[i].disabled;
            if (sel) Glass::Glow(c, x, y, s, s, s * 0.09f, s / 200.0f);
            FrostBox(c, solid, x, y, s, s, s * 0.09f, sel, sel ? (float)std::max(2, scale) : 1.0f);
            const uint32_t ink = off ? GlassOff : GlassText;
            if (items[i].icon >= 0) Glass::DrawIcon(c, items[i].icon, x + s * 0.5f, y + s * 0.4f, s * 0.19f, ink);
            TX(c, x + (s - TW(items[i].label, ls, true)) / 2, y + (int)(s * 0.72f), items[i].label, ls, ink, true);
            if (!items[i].value.empty())
            {
                const int vs = std::max(1, ls - 1);
                TX(c, x + s - TW(items[i].value, vs) - s / 14, y + s / 14, items[i].value, vs, off ? GlassOff : GlassHint);
            }
        }
        TX(c, (c.w - TW(hint, scale)) / 2, gy + gridH + gap + 2 * scale, hint, scale, GlassHint);
        if (solid) Glass::Opaque(c);
    }

    // boxUnits: the box's width at scale 1 (0: 190, as the other pages). CHEATS passes 260 so a
    // code's name (32 letters) fits beside "< OFF >".
    void Draw(Canvas& c, const std::string& title, const std::string& hint, const std::string& note = "", int boxUnits = 0) const
    {
        if (style == 1) { DrawSettings(c, title, hint); return; }
        if (columns > 0) { DrawTiles(c, hint); return; }
        int scale = std::min(c.w, c.h) / 240;
        if (scale < 1) scale = 1;
        int lineH = (UiLang::Tall() ? 16 : 11) * scale;
        // (Rows closer together if they would not all fit on the screen, down to 9 * scale.)
        // With no title the box starts with the first item.
        const bool hasTitle = !title.empty();
        // (As many rows as fit at the smallest spacing; the rest scroll into view.)
        const int fit = std::max(1, (c.h - 4 - 6 * scale) / ((UiLang::Tall() ? 14 : 9) * scale) - (hasTitle ? 3 : 2));
        const int shown = std::min((int)items.size(), fit);
        if (selected < scroll) scroll = selected;
        if (selected >= scroll + shown) scroll = selected - shown + 1;
        scroll = std::max(0, std::min(scroll, (int)items.size() - shown));
        const int rows = shown + (hasTitle ? 3 : 2);
        if (lineH * rows + 6 * scale > c.h - 4) lineH = std::max(UiLang::LineUnits() * scale, (c.h - 4 - 6 * scale) / rows);
        // (A value as drawn: translated before its arrows go round it, as the whole would not be
        // in the language's table.)
        auto shownValue = [](const MenuItem& item) { return item.info ? item.value : "< " + UiLang::Tr(item.value) + " >"; };
        int boxW = (boxUnits > 0 ? boxUnits : 190) * scale;
        if (UiLang::lang)
            for (const auto& item : items)
                boxW = std::max(boxW, TW(item.label, scale) + (item.value.empty() ? 0 : TW(shownValue(item), scale)) + 20 * scale);
        if (boxW > c.w - 8) boxW = c.w - 8;
        int boxH = lineH * rows + 6 * scale;
        // The note's lines (up to 4, the last cut short), and the box moved up to make room.
        std::vector<std::string> noteLines;
        if (!note.empty())
        {
            const int maxW = boxW - 12 * scale;
            noteLines = UiLang::Wrap(UiLang::Tr(note), maxW / scale);
            if (noteLines.size() > 4) { noteLines.resize(4); noteLines[3] = UiLang::Elide(noteLines[3]+"...",maxW/scale); }
        }
        const int noteH = noteLines.empty() ? 0 : (int)noteLines.size() * UiLang::LineUnits() * scale + 6 * scale;
        int bx = (c.w - boxW) / 2, by = std::max(2, (c.h - boxH - noteH) / 2);
        // (Kept for ItemAt: where the rows were drawn.)
        drawnX = bx;
        drawnY = by;
        drawnW = boxW;
        drawnH = boxH;
        drawnLineH = lineH;
        drawnFirstY = by + (hasTitle ? lineH : 0) + 5 * scale - 2 * scale;

        const bool solid = Glass::Backdrop(c);
        const float radius = 5.0f * scale;
        FrostBox(c, solid, bx, by, boxW, boxH, radius);
        if (hasTitle) TX(c, bx + (boxW - TW(title, scale, true)) / 2, by + 3 * scale, title, scale, GlassText, true);

        int y = by + (hasTitle ? lineH : 0) + 5 * scale;
        // (Marks at the box's right edge where rows are scrolled out of view, above or below.)
        if (scroll > 0) TX(c, bx + boxW - 8 * scale, y - lineH + 2 * scale, "+", scale, GlassHint);
        if (scroll + shown < (int)items.size()) TX(c, bx + boxW - 8 * scale, y + shown * lineH - 2 * scale, "+", scale, GlassHint);
        for (int i = scroll; i < scroll + shown; i++)
        {
            bool sel = i == selected;
            if (sel) Glass::Round(c, bx + 2 * scale, y - 2 * scale, boxW - 4 * scale, lineH, 3.0f * scale, GlassRow, GlassChosenEdge, (float)std::max(1, scale / 2));
            uint32_t col = items[i].disabled ? GlassOff : sel ? 0xFFFFFFFF : GlassText;
            TX(c, bx + 6 * scale, y, items[i].label, scale, col);
            if (!items[i].value.empty())
            {
                const std::string v = shownValue(items[i]);
                // (A slider's value right-aligned in room for its widest, "< 100% >", the bar left
                // of it, so the bar stays put as the number changes.)
                const int vx = bx + boxW - 6 * scale - TW(items[i].slider >= 0 ? "< 100% >" : v, scale);
                TX(c, items[i].slider >= 0 ? bx + boxW - 6 * scale - TW(v, scale) : vx, y, v, scale, col);
                if (items[i].slider >= 0)
                {
                    barStep = 5 * scale;
                    barX = vx - 4 * scale - 10 * barStep;
                    for (int k = 0; k < 10; k++)
                        c.Fill(barX + k * barStep, y, 4 * scale, 7 * scale,
                               k < items[i].slider ? 0xFFFFFFFF : (sel ? 0xFF9AA5B1 : 0xFF5C6672));
                }
            }
            y += lineH;
        }
        TX(c, bx + (boxW - TW(hint, scale)) / 2, y + 4 * scale, hint, scale, GlassHint);
        if (!noteLines.empty())
        {
            const int ny = by + boxH + 3 * scale;
            FrostBox(c, solid, bx, ny, boxW, noteH, radius);
            for (size_t i = 0; i < noteLines.size(); i++)
                TXRaw(c, bx + 6 * scale, ny + 4 * scale + (int)i * UiLang::LineUnits() * scale, noteLines[i], scale, GlassText);
        }
        if (solid) Glass::Opaque(c);
    }

    // Settings pages (style 1): ui_settings.h.
    int UpdateSettings(const MenuInput& in, int& dir);
    int SettingsItemAt(int x, int y) const;
    void SettingsTap(int item, int x, MenuInput& input);
    void DrawSettings(Canvas& c, const std::string& title, const std::string& hint) const;
    int FirstRow() const; // the first row that can be chosen (not a section's heading or the back button)
    // Where the settings page was last drawn: each item's row (y, -1 when not shown), the rows' x
    // and size, the controls' box on the right of them, and the back button.
    mutable std::vector<int> rowY;
    mutable int rowX = 0, rowW = 0, rowH = 0, ctlX = 0, ctlW = 0, backX = 0, backY = 0, backS = 0, scrollPx = 0;
};

#include "ui_settings.h"

