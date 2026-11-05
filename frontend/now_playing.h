// NOW PLAYING: what the player's own music app (Spotify, YouTube Music in a browser, Media Player...)
// is playing, for the bar under the pause menu's tiles, and its previous / play-pause / next.
//
// Windows: the system's media controls (Windows.Media.Control, the ones the volume pop-up shows),
// reached through WinRT's plain COM interfaces. The compilers Fulldex is built with have no headers
// for them, so the few interfaces used are declared here, by their place in the vtable, as
// Microsoft's metadata lists them (windows-rs, Windows/Media/Control/mod.rs). combase.dll is
// loaded at run time: nothing new to link, and a Windows without it just shows no bar.
// Everything runs on its own thread (the calls are asynchronous and may take a moment); the game
// and the menu only read the latest Info and queue commands.
// Other systems: nothing yet (Info stays inactive), so the bar never shows.
#pragma once
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace NowPlaying
{
struct Info
{
    bool active = false;  // an app has a media session, playing or paused
    bool playing = false;
    bool canPrev = false, canNext = false;
    std::string title, artist, app;
};

#ifdef _WIN32
}
#include <windows.h>
#include <unknwn.h>
namespace NowPlaying
{
namespace Detail
{
typedef struct HSTRING__* HSTR;
typedef HRESULT(WINAPI* RoInitializeFn)(int);
typedef HRESULT(WINAPI* RoGetActivationFactoryFn)(HSTR, REFIID, void**);
typedef HRESULT(WINAPI* WindowsCreateStringFn)(const wchar_t*, UINT32, HSTR*);
typedef HRESULT(WINAPI* WindowsDeleteStringFn)(HSTR);
typedef const wchar_t*(WINAPI* WindowsGetStringRawBufferFn)(HSTR, UINT32*);

// A WinRT method by its place in the object's vtable (0-5 are IUnknown and IInspectable).
template <typename... A>
inline HRESULT Call(void* obj, int slot, A... a)
{
    typedef HRESULT(STDMETHODCALLTYPE * Fn)(void*, A...);
    return (*reinterpret_cast<Fn**>(obj))[slot](obj, a...);
}
inline void Release(void* obj) { if (obj) static_cast<IUnknown*>(obj)->Release(); }

struct Api
{
    RoInitializeFn RoInitialize = nullptr;
    RoGetActivationFactoryFn RoGetActivationFactory = nullptr;
    WindowsCreateStringFn WindowsCreateString = nullptr;
    WindowsDeleteStringFn WindowsDeleteString = nullptr;
    WindowsGetStringRawBufferFn WindowsGetStringRawBuffer = nullptr;
    bool Load()
    {
        HMODULE m = LoadLibraryW(L"combase.dll");
        if (!m) return false;
        RoInitialize = (RoInitializeFn)(void*)GetProcAddress(m, "RoInitialize");
        RoGetActivationFactory = (RoGetActivationFactoryFn)(void*)GetProcAddress(m, "RoGetActivationFactory");
        WindowsCreateString = (WindowsCreateStringFn)(void*)GetProcAddress(m, "WindowsCreateString");
        WindowsDeleteString = (WindowsDeleteStringFn)(void*)GetProcAddress(m, "WindowsDeleteString");
        WindowsGetStringRawBuffer = (WindowsGetStringRawBufferFn)(void*)GetProcAddress(m, "WindowsGetStringRawBuffer");
        return RoInitialize && RoGetActivationFactory && WindowsCreateString && WindowsDeleteString && WindowsGetStringRawBuffer;
    }
    std::string Text(HSTR s)
    {
        if (!s) return "";
        UINT32 n = 0;
        const wchar_t* w = WindowsGetStringRawBuffer(s, &n);
        std::string out;
        if (w && n)
        {
            const int len = WideCharToMultiByte(CP_UTF8, 0, w, (int)n, nullptr, 0, nullptr, nullptr);
            out.resize(len > 0 ? len : 0);
            if (len > 0) WideCharToMultiByte(CP_UTF8, 0, w, (int)n, &out[0], len, nullptr, nullptr);
        }
        WindowsDeleteString(s);
        return out;
    }
};

// IIDs (Windows.Media.Control, and IAsyncInfo).
static const IID IID_ManagerStatics = { 0x2050c4ee, 0x11a0, 0x57de, { 0xae, 0xd7, 0xc9, 0x7c, 0x70, 0x33, 0x82, 0x45 } };
static const IID IID_AsyncInfo = { 0x00000036, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
// Vtable places.
enum
{
    STATICS_REQUEST = 6,                                        // RequestAsync
    MANAGER_CURRENT = 6,                                        // GetCurrentSession
    SESSION_APP = 6, SESSION_PROPERTIES = 7, SESSION_PLAYBACK = 9, // SourceAppUserModelId, TryGetMediaPropertiesAsync, GetPlaybackInfo
    SESSION_NEXT = 16, SESSION_PREV = 17, SESSION_TOGGLE = 20,  // TrySkipNextAsync, TrySkipPreviousAsync, TryTogglePlayPauseAsync
    PROPS_TITLE = 6, PROPS_ARTIST = 9,
    PLAYBACK_CONTROLS = 6, PLAYBACK_STATUS = 7,                 // status: 4 playing, 5 paused
    CONTROLS_NEXT = 12, CONTROLS_PREV = 13,                     // IsNextEnabled, IsPreviousEnabled
    ASYNC_RESULTS = 8,                                          // IAsyncOperation<T>::GetResults
    ASYNCINFO_STATUS = 7,                                       // IAsyncInfo::get_Status: 0 started, 1 completed
};

// Waits (up to ms) for an asynchronous operation and takes its result.
inline void* Await(void* op, int ms)
{
    if (!op) return nullptr;
    void* info = nullptr;
    void* result = nullptr;
    if (SUCCEEDED(static_cast<IUnknown*>(op)->QueryInterface(IID_AsyncInfo, &info)) && info)
    {
        int status = 0;
        for (int waited = 0; waited < ms; waited += 5)
        {
            if (FAILED(Call(info, ASYNCINFO_STATUS, &status)) || status != 0) break;
            Sleep(5);
        }
        if (status == 1) Call(op, ASYNC_RESULTS, &result);
        Release(info);
    }
    Release(op);
    return result;
}

// Waits (up to ms) for an operation whose result is not an object (IAsyncOperation<bool>), and lets it go.
inline void AwaitDone(void* op, int ms)
{
    if (!op) return;
    void* info = nullptr;
    if (SUCCEEDED(static_cast<IUnknown*>(op)->QueryInterface(IID_AsyncInfo, &info)) && info)
    {
        int status = 0;
        for (int waited = 0; waited < ms; waited += 5)
        {
            if (FAILED(Call(info, ASYNCINFO_STATUS, &status)) || status != 0) break;
            Sleep(5);
        }
        Release(info);
    }
    Release(op);
}

// The app's name, from its id ("Spotify.exe", "SpotifyAB.SpotifyMusic_...!Spotify", "Chrome"...).
inline std::string AppName(std::string id)
{
    const size_t bang = id.rfind('!');
    if (bang != std::string::npos) id = id.substr(bang + 1);
    if (id.size() > 4 && _stricmp(id.c_str() + id.size() - 4, ".exe") == 0) id.resize(id.size() - 4);
    std::string low = id;
    for (char& ch : low) ch = (char)tolower((unsigned char)ch);
    if (low.find("spotify") != std::string::npos) return "Spotify";
    if (low == "308046b0af4a39cb" || low.find("firefox") != std::string::npos) return "Firefox";
    if (low.find("msedge") != std::string::npos) return "Edge";
    if (low.find("chrome") != std::string::npos) return "Chrome";
    if (low.find("zunemusic") != std::string::npos) return "Media Player";
    if (low.find("itunes") != std::string::npos || low.find("applemusic") != std::string::npos) return "Apple Music";
    if (low.find("vlc") != std::string::npos) return "VLC";
    if (low.rfind("microsoft.", 0) == 0) id = id.substr(10);
    return id;
}
} // namespace Detail

struct Watcher
{
    std::mutex lock;
    Info info;
    std::atomic<int> command{ -1 };     // 0 previous, 1 play / pause, 2 next
    std::atomic<int> wanted{ 0 };        // how often to look: 0 not at all, 1 now and then, 2 often (menu open)
    std::atomic<bool> quit{ false };
    std::thread thread;

    void Run()
    {
        Detail::Api api;
        if (!api.Load()) return;
        api.RoInitialize(1); // (RO_INIT_MULTITHREADED)
        void* manager = nullptr;
        {
            const wchar_t* name = L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager";
            Detail::HSTR cls = nullptr;
            void* statics = nullptr;
            if (SUCCEEDED(api.WindowsCreateString(name, (UINT32)wcslen(name), &cls)) &&
                SUCCEEDED(api.RoGetActivationFactory(cls, Detail::IID_ManagerStatics, &statics)) && statics)
            {
                void* op = nullptr;
                if (SUCCEEDED(Detail::Call(statics, Detail::STATICS_REQUEST, &op))) manager = Detail::Await(op, 3000);
                Detail::Release(statics);
            }
            if (cls) api.WindowsDeleteString(cls);
        }
        if (!manager) { fprintf(stderr, "[now playing] the system's media controls are not available\n"); return; }
        int idle = 0, misses = 0;
        while (!quit)
        {
            const int c = command.exchange(-1);
            const int w = wanted;
            if (c < 0 && (w == 0 || idle > 0)) { Sleep(50); if (idle > 0) idle -= 50; continue; }
            idle = w == 2 ? 400 : 2000;
            Info now;
            void* session = nullptr;
            if (SUCCEEDED(Detail::Call(manager, Detail::MANAGER_CURRENT, &session)) && session)
            {
                if (c >= 0)
                {
                    void* op = nullptr;
                    const int slot = c == 0 ? Detail::SESSION_PREV : c == 2 ? Detail::SESSION_NEXT : Detail::SESSION_TOGGLE;
                    // (These give IAsyncOperation<bool>: a yes / no, not an object to release.)
                    if (SUCCEEDED(Detail::Call(session, slot, &op))) Detail::AwaitDone(op, 500);
                    Sleep(150); // (the app takes a moment to change its song or state)
                }
                Detail::HSTR id = nullptr;
                if (SUCCEEDED(Detail::Call(session, Detail::SESSION_APP, &id))) now.app = Detail::AppName(api.Text(id));
                void* playback = nullptr;
                if (SUCCEEDED(Detail::Call(session, Detail::SESSION_PLAYBACK, &playback)) && playback)
                {
                    int status = 0;
                    Detail::Call(playback, Detail::PLAYBACK_STATUS, &status);
                    now.playing = status == 4;
                    now.active = status == 4 || status == 5;
                    void* controls = nullptr;
                    if (SUCCEEDED(Detail::Call(playback, Detail::PLAYBACK_CONTROLS, &controls)) && controls)
                    {
                        boolean next = 0, prev = 0;
                        Detail::Call(controls, Detail::CONTROLS_NEXT, &next);
                        Detail::Call(controls, Detail::CONTROLS_PREV, &prev);
                        now.canNext = next; now.canPrev = prev;
                        Detail::Release(controls);
                    }
                    Detail::Release(playback);
                }
                if (now.active)
                {
                    void* op = nullptr;
                    void* props = nullptr;
                    if (SUCCEEDED(Detail::Call(session, Detail::SESSION_PROPERTIES, &op))) props = Detail::Await(op, 1000);
                    if (props)
                    {
                        Detail::HSTR t = nullptr, a = nullptr;
                        if (SUCCEEDED(Detail::Call(props, Detail::PROPS_TITLE, &t))) now.title = api.Text(t);
                        if (SUCCEEDED(Detail::Call(props, Detail::PROPS_ARTIST, &a))) now.artist = api.Text(a);
                        Detail::Release(props);
                    }
                    if (now.title.empty()) now.active = false; // (a session with nothing in it)
                }
                Detail::Release(session);
            }
            // (While an app changes song or between pause and play it can show no song for a moment:
            // the last one is kept for a few looks, so the bar does not vanish and the tiles jump.)
            if (!now.active && misses < 4)
            {
                std::lock_guard<std::mutex> g(lock);
                if (info.active) { misses++; if (c == 1) info.playing = !info.playing; continue; }
            }
            if (now.active) misses = 0;
            std::lock_guard<std::mutex> g(lock);
            info = now;
        }
        Detail::Release(manager);
    }
};

inline Watcher& Get() { static Watcher w; return w; }
inline void Start() { Watcher& w = Get(); if (!w.thread.joinable()) w.thread = std::thread([&w] { w.Run(); }); }
inline void Stop() { Watcher& w = Get(); w.quit = true; if (w.thread.joinable()) w.thread.join(); }
inline void Want(int how) { Get().wanted = how; }
inline Info Now() { Watcher& w = Get(); std::lock_guard<std::mutex> g(w.lock); return w.info; }
inline void Command(int c) { Get().command = c; }
inline bool Supported() { return true; }
#else
inline void Start() {}
inline void Stop() {}
inline void Want(int) {}
inline Info Now() { return Info(); }
inline void Command(int) {}
inline bool Supported() { return false; }
#endif
} // namespace NowPlaying
