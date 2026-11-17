// user32 falso: uma "janela" sem superfície real com fila de mensagens própria. A imagem vai para o SwapChainPanel (shim_dx).
#include "host.h"
#include <deque>
#include <map>
#include <vector>

struct FakeClass { WNDPROC proc; };
struct FakeWindow { WNDPROC proc; int x, y, w, h; std::map<int, LONG> longs; };
struct FakeTimer { HWND hwnd; UINT_PTR id; UINT ms; ULONGLONG next; };

static std::map<std::string, FakeClass> g_classes;
static std::map<HWND, FakeWindow> g_windows;
static std::deque<MSG> g_queue;
static std::vector<FakeTimer> g_timers;
static CRITICAL_SECTION g_cs;
static INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;
static UINT_PTR g_nextHwnd = 0x10010;
static UINT g_nextMsg = 0xC100;
static BYTE g_keys[256];
static HWND g_mainWnd = nullptr;

static const int kDisplayW = 1920, kDisplayH = 1080;

static BOOL CALLBACK InitCs(PINIT_ONCE, PVOID, PVOID*) { InitializeCriticalSectionEx(&g_cs, 4000, 0); return TRUE; }
static void Ensure() { InitOnceExecuteOnce(&g_once, InitCs, nullptr, nullptr); }

struct Lock { Lock() { Ensure(); EnterCriticalSection(&g_cs); } ~Lock() { LeaveCriticalSection(&g_cs); } };

static void Post(HWND h, UINT m, WPARAM w, LPARAM l) {
    Lock lk;
    MSG msg{};
    msg.hwnd = h; msg.message = m; msg.wParam = w; msg.lParam = l; msg.time = (DWORD)GetTickCount64();
    g_queue.push_back(msg);
}

void HostQueueKey(unsigned vk, bool down) {
    if (vk < 256) g_keys[vk] = down ? 0x80 : 0;
    if (g_mainWnd) Post(g_mainWnd, down ? WM_KEYDOWN : WM_KEYUP, vk, down ? 1 : (LPARAM)0xC0000001);
}

static ATOM WINAPI S_RegisterClassA(const WNDCLASSA* wc) {
    Lock lk;
    std::string name = HIWORD(wc->lpszClassName) ? wc->lpszClassName : "#atom";
    g_classes[name] = {wc->lpfnWndProc};
    HLog("RegisterClassA(%s)", name.c_str());
    return (ATOM)(0xC000 + g_classes.size());
}

static WNDPROC ProcOf(HWND h) {
    Lock lk;
    auto it = g_windows.find(h);
    return it == g_windows.end() ? nullptr : it->second.proc;
}

static HWND WINAPI S_CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR title, DWORD style, int x, int y, int w, int h, HWND parent, HMENU, HINSTANCE inst, LPVOID param) {
    HWND hwnd;
    WNDPROC proc = nullptr;
    {
        Lock lk;
        std::string name = HIWORD(cls) ? cls : "#atom";
        auto it = g_classes.find(name);
        if (it != g_classes.end()) proc = it->second.proc;
        else if (!g_classes.empty()) proc = g_classes.begin()->second.proc;
        hwnd = (HWND)g_nextHwnd;
        g_nextHwnd += 0x10;
        if (w <= 0 || w == (int)CW_USEDEFAULT) w = kDisplayW;
        if (h <= 0 || h == (int)CW_USEDEFAULT) h = kDisplayH;
        g_windows[hwnd] = {proc, x, y, w, h, {}};
        if (!g_mainWnd) g_mainWnd = hwnd;
    }
    HLog("CreateWindowExA(%s, \"%s\") -> %p (%dx%d)", HIWORD(cls) ? cls : "#atom", title ? title : "", hwnd, w, h);
    if (proc) {
        CREATESTRUCTA cs{};
        cs.lpCreateParams = param; cs.hInstance = inst; cs.hwndParent = parent; cs.cx = w; cs.cy = h; cs.x = x; cs.y = y;
        cs.style = (LONG)style; cs.lpszName = title; cs.lpszClass = cls; cs.dwExStyle = ex;
        proc(hwnd, WM_NCCREATE, 0, (LPARAM)&cs);
        proc(hwnd, WM_CREATE, 0, (LPARAM)&cs);
        Post(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(w, h));
        Post(hwnd, WM_ACTIVATEAPP, TRUE, 0);
        Post(hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
        Post(hwnd, WM_SETFOCUS, 0, 0);
    }
    return hwnd;
}

static LRESULT WINAPI S_DefWindowProcA(HWND, UINT m, WPARAM, LPARAM) { return m == WM_NCCREATE ? TRUE : 0; }

static BOOL WINAPI S_PeekMessageA(LPMSG out, HWND h, UINT mn, UINT mx, UINT remove) {
    Lock lk;
    ULONGLONG now = GetTickCount64();
    for (auto& t : g_timers)
        if (now >= t.next) {
            t.next = now + t.ms;
            MSG m{}; m.hwnd = t.hwnd; m.message = WM_TIMER; m.wParam = t.id; m.time = (DWORD)now;
            g_queue.push_back(m);
        }
    for (auto it = g_queue.begin(); it != g_queue.end(); ++it) {
        if (h && it->hwnd != h) continue;
        if ((mn || mx) && (it->message < mn || it->message > mx)) continue;
        *out = *it;
        if (remove & PM_REMOVE) g_queue.erase(it);
        return TRUE;
    }
    return FALSE;
}

static BOOL WINAPI S_TranslateMessage(const MSG*) { return FALSE; }

static LRESULT WINAPI S_DispatchMessageA(const MSG* m) {
    WNDPROC p = ProcOf(m->hwnd);
    if (p) return p(m->hwnd, m->message, m->wParam, m->lParam);
    return 0;
}

static BOOL WINAPI S_PostMessageA(HWND h, UINT m, WPARAM w, LPARAM l) { Post(h ? h : g_mainWnd, m, w, l); return TRUE; }
static void WINAPI S_PostQuitMessage(int code) { Post(nullptr, WM_QUIT, (WPARAM)code, 0); }

static UINT WINAPI S_RegisterWindowMessageA(LPCSTR) { Lock lk; return g_nextMsg++; }

static BOOL WINAPI S_ShowWindow(HWND, int) { return TRUE; }

static BOOL WINAPI S_GetClientRect(HWND h, LPRECT r) {
    Lock lk;
    auto it = g_windows.find(h);
    r->left = r->top = 0;
    r->right = it != g_windows.end() ? it->second.w : kDisplayW;
    r->bottom = it != g_windows.end() ? it->second.h : kDisplayH;
    return TRUE;
}

static BOOL WINAPI S_GetWindowRect(HWND h, LPRECT r) {
    Lock lk;
    auto it = g_windows.find(h);
    int x = it != g_windows.end() ? it->second.x : 0, y = it != g_windows.end() ? it->second.y : 0;
    r->left = x; r->top = y;
    r->right = x + (it != g_windows.end() ? it->second.w : kDisplayW);
    r->bottom = y + (it != g_windows.end() ? it->second.h : kDisplayH);
    return TRUE;
}

static BOOL WINAPI S_AdjustWindowRect(LPRECT, DWORD, BOOL) { return TRUE; }

static BOOL WINAPI S_SetWindowPos(HWND h, HWND, int x, int y, int cx, int cy, UINT flags) {
    bool resized = false;
    {
        Lock lk;
        auto it = g_windows.find(h);
        if (it == g_windows.end()) return FALSE;
        if (!(flags & SWP_NOMOVE)) { it->second.x = x; it->second.y = y; }
        if (!(flags & SWP_NOSIZE) && cx > 0 && cy > 0 && (cx != it->second.w || cy != it->second.h)) { it->second.w = cx; it->second.h = cy; resized = true; }
    }
    if (resized) Post(h, WM_SIZE, SIZE_RESTORED, MAKELPARAM(cx, cy));
    return TRUE;
}

static LONG WINAPI S_SetWindowLongA(HWND h, int i, LONG v) {
    Lock lk;
    auto it = g_windows.find(h);
    if (it == g_windows.end()) return 0;
    LONG old = it->second.longs[i];
    it->second.longs[i] = v;
    return old;
}

static LONG WINAPI S_GetWindowLongA(HWND h, int i) {
    Lock lk;
    auto it = g_windows.find(h);
    return it == g_windows.end() ? 0 : it->second.longs[i];
}

static HWND WINAPI S_GetForegroundWindow() { return g_mainWnd; }
static BOOL WINAPI S_ClientToScreen(HWND, LPPOINT) { return TRUE; }
static int WINAPI S_MapWindowPoints(HWND, HWND, LPPOINT, UINT) { return 0; }
static HDC WINAPI S_BeginPaint(HWND, LPPAINTSTRUCT ps) { if (ps) memset(ps, 0, sizeof(*ps)); return (HDC)1; }
static BOOL WINAPI S_EndPaint(HWND, const PAINTSTRUCT*) { return TRUE; }
static int WINAPI S_FillRect(HDC, const RECT*, HBRUSH) { return 1; }
static HCURSOR WINAPI S_LoadCursorA(HINSTANCE, LPCSTR) { return (HCURSOR)1; }
static HCURSOR WINAPI S_SetCursor(HCURSOR) { return nullptr; }
static LONG g_cursorCount = 0;
static int WINAPI S_ShowCursor(BOOL s) { return (int)InterlockedAdd(&g_cursorCount, s ? 1 : -1); }
static BOOL WINAPI S_SetCursorPos(int, int) { return TRUE; }
static BOOL WINAPI S_ClipCursor(const RECT*) { return TRUE; }
static HWND WINAPI S_SetCapture(HWND h) { return h; }
static BOOL WINAPI S_ReleaseCapture() { return TRUE; }
static SHORT WINAPI S_GetKeyState(int vk) { return vk >= 0 && vk < 256 ? (SHORT)(g_keys[vk] ? 0x8000 : 0) : 0; }
static int WINAPI S_GetKeyNameTextA(LONG, LPSTR b, int n) { if (b && n > 0) { strncpy_s(b, n, "Key", _TRUNCATE); return 3; } return 0; }
static HKL WINAPI S_GetKeyboardLayout(DWORD) { return (HKL)0x04090409; }
static UINT WINAPI S_MapVirtualKeyA(UINT c, UINT) { return c; }
static UINT WINAPI S_MapVirtualKeyExA(UINT c, UINT, HKL) { return c; }
static BOOL WINAPI S_RegisterRawInputDevices(const void*, UINT, UINT) { return TRUE; }
static UINT WINAPI S_GetRawInputData(HRAWINPUT, UINT, LPVOID, PUINT sz, UINT) { if (sz) *sz = 0; return (UINT)-1; }
static BOOL WINAPI S_OpenClipboard(HWND) { return FALSE; }
static BOOL WINAPI S_CloseClipboard() { return TRUE; }
static BOOL WINAPI S_EmptyClipboard() { return TRUE; }
static HANDLE WINAPI S_GetClipboardData(UINT) { return nullptr; }
static HANDLE WINAPI S_SetClipboardData(UINT, HANDLE) { return nullptr; }
static BOOL WINAPI S_IsWindowVisible(HWND) { return TRUE; }
static HWND WINAPI S_SetFocus(HWND h) { return h; }

static UINT_PTR WINAPI S_SetTimer(HWND h, UINT_PTR id, UINT ms, TIMERPROC) {
    Lock lk;
    for (auto& t : g_timers)
        if (t.hwnd == h && t.id == id) { t.ms = ms; t.next = GetTickCount64() + ms; return id; }
    g_timers.push_back({h, id, ms, GetTickCount64() + ms});
    return id;
}

static BOOL WINAPI S_EnumDisplayDevicesA(LPCSTR, DWORD i, PDISPLAY_DEVICEA d, DWORD) {
    if (i > 0) return FALSE;
    strcpy_s(d->DeviceName, "\\\\.\\DISPLAY1");
    strcpy_s(d->DeviceString, "Xbox Display");
    d->StateFlags = DISPLAY_DEVICE_ATTACHED_TO_DESKTOP | DISPLAY_DEVICE_PRIMARY_DEVICE;
    return TRUE;
}

static BOOL WINAPI S_GetMonitorInfoA(HMONITOR, LPMONITORINFO mi) {
    mi->rcMonitor = {0, 0, kDisplayW, kDisplayH};
    mi->rcWork = mi->rcMonitor;
    mi->dwFlags = MONITORINFOF_PRIMARY;
    return TRUE;
}

static int WINAPI S_MessageBoxA(HWND, LPCSTR text, LPCSTR cap, UINT) {
    HLog("MessageBox: [%s] %s", cap ? cap : "", text ? text : "");
    return IDOK;
}

static int WINAPI S_MessageBoxIndirectA(const void*) { HLog("MessageBoxIndirect"); return IDOK; }

#define U(n) {"user32.dll", #n, (void*)&S_##n}
const ShimEntry g_userShims[] = {
    U(RegisterClassA), U(CreateWindowExA), U(DefWindowProcA), U(PeekMessageA), U(TranslateMessage), U(DispatchMessageA), U(PostMessageA),
    U(PostQuitMessage), U(RegisterWindowMessageA), U(ShowWindow), U(GetClientRect), U(GetWindowRect), U(AdjustWindowRect), U(SetWindowPos),
    U(SetWindowLongA), U(GetWindowLongA), U(GetForegroundWindow), U(ClientToScreen), U(MapWindowPoints), U(BeginPaint), U(EndPaint), U(FillRect),
    U(LoadCursorA), U(SetCursor), U(ShowCursor), U(SetCursorPos), U(ClipCursor), U(SetCapture), U(ReleaseCapture), U(GetKeyState),
    U(GetKeyNameTextA), U(GetKeyboardLayout), U(MapVirtualKeyA), U(MapVirtualKeyExA), U(RegisterRawInputDevices), U(GetRawInputData),
    U(OpenClipboard), U(CloseClipboard), U(EmptyClipboard), U(GetClipboardData), U(SetClipboardData), U(IsWindowVisible), U(SetFocus),
    U(SetTimer), U(EnumDisplayDevicesA), U(GetMonitorInfoA), U(MessageBoxA), U(MessageBoxIndirectA),
    {nullptr, nullptr, nullptr},
};
