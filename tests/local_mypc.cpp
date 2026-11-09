// MY PC drawn offscreen to a picture (no window, no device), with real saves, and a drag, two
// taps and SAVE played on it (PCVIEW_MOVE=1, PCVIEW_SAVE=1). Built with g++ -std=c++17 against
// frontend/, SDL2's headers, box_sprites.cpp (tools/boxsprites), launcher_cjk.cpp and the hub library.
// local_mypc <saves folder> <a GBA ROM path> <out.ppm> [w h] [side slot] [tab]
// Draws the MY PC page offscreen (no SDL window) to a PNG, with real saves: for checking its look.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "launcher.h"
#include <cstdio>
int main(int argc, char** argv)
{
    // argv: savesDir romPath outPpm [w h] [select side slot] [tab]
    const std::string saves = argv[1], rom = argv[2];
    const int W = argc > 4 ? atoi(argv[4]) : 2400, Hh = argc > 5 ? atoi(argv[5]) : 1080;
    Launcher::HubDir() = saves;
    Launcher::Game g;
    g.platform = Launcher::GBA; g.rom = rom; g.folder = saves; g.saveName = "leafgreen.sav"; g.name = "POKEMON LEAFGREEN"; g.known = true; g.code = "BPGE"; g.hasSave = true;
    Launcher::RomList().push_back(g);
    Launcher::PhoneUi() = true;
    Canvas c; c.Resize(W, Hh);
    for (auto& p : c.px) p = 0xFF203040;
    Launcher::Orient(c);
    const float u = Launcher::GU(c);
    Launcher::Focus().page = Launcher::SIDE_HUB;
    Launcher::HubMode() = 6;
    if (argc > 7) { Launcher::DrawPcPage(c, true, u, (int)(80 * u), W - (int)(14 * u), (int)(70 * u), Hh - (int)(14 * u)); Launcher::Pc().selSide = atoi(argv[6]); Launcher::Pc().selBox = atoi(argv[6]) == 1 ? Launcher::Pc().s[1].box : 0; Launcher::Pc().selSlot = atoi(argv[7]); for (auto& p : c.px) p = 0xFF203040; }
    if (argc > 8) Launcher::Pc().tab = atoi(argv[8]);
    Launcher::DrawPcPage(c, true, u, (int)(80 * u), W - (int)(14 * u), (int)(70 * u), Hh - (int)(14 * u));
    if (getenv("PCVIEW_MOVE"))
    {
        // Rattata (LeafGreen box 1, slot 0) into the Bank's box 1 slot 4, by a drag; then the party's 2nd to Bank slot 5 by taps; then Save.
        Launcher::PcState& P = Launcher::Pc();
        auto centre = [&](int side, int slot, int& x, int& y) { for (auto& h : P.hits) if (h.kind == Launcher::PC_CELL && h.side == side && h.value == slot) { x = h.x + h.w / 2; y = h.y + h.h / 2; return true; } return false; };
        int x0, y0, x1, y1;
        P.s[1].box = 0; for (auto& p : c.px) p = 0xFF203040; Launcher::DrawPcPage(c, true, u, (int)(80 * u), W - (int)(14 * u), (int)(70 * u), Hh - (int)(14 * u));
        if (centre(1, 0, x0, y0) && centre(0, 4, x1, y1)) { Launcher::PcPointer(0, x0, y0); Launcher::PcPointer(1, (x0 + x1) / 2, (y0 + y1) / 2); Launcher::PcPointer(1, x1, y1); Launcher::PcPointer(2, x1, y1); }
        printf("after drag: pending %d msg [%s]\n", P.pending, P.msg.c_str());
        for (auto& p : c.px) p = 0xFF203040; Launcher::DrawPcPage(c, true, u, (int)(80 * u), W - (int)(14 * u), (int)(70 * u), Hh - (int)(14 * u));
        if (centre(1, 1, x0, y0)) { Launcher::PcPointer(0, x0, y0); Launcher::PcPointer(2, x0, y0); }
        if (centre(0, 5, x1, y1)) { Launcher::PcPointer(0, x1, y1); Launcher::PcPointer(2, x1, y1); }
        printf("after taps: pending %d msg [%s]\n", P.pending, P.msg.c_str());
        if (getenv("PCVIEW_SAVE")) { Launcher::PcSave(); printf("after save: pending %d msg [%s]\n", P.pending, P.msg.c_str()); }
        for (auto& p : c.px) p = 0xFF203040; Launcher::DrawPcPage(c, true, u, (int)(80 * u), W - (int)(14 * u), (int)(70 * u), Hh - (int)(14 * u));
    }
    FILE* f = fopen(argv[3], "wb");
    fprintf(f, "P6\n%d %d\n255\n", c.w, c.h);
    for (uint32_t p : c.px) { unsigned char rgb[3] = { (unsigned char)(p >> 16), (unsigned char)(p >> 8), (unsigned char)p }; fwrite(rgb, 1, 3, f); }
    fclose(f);
    printf("ok, %zu hits\n", Launcher::Pc().hits.size());
}
