// Controle via Windows.Gaming.Input, exposto ao jogo como XInput.
#include "host.h"
#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>
#include <cstdio>
#include <vector>

using namespace winrt;
using namespace winrt::Windows::Gaming::Input;

// ---------------------------------------------------------------- XInput
struct XInGamepad { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; };
struct XInState { DWORD dwPacketNumber; XInGamepad Gamepad; };

// Roteiro de botões para testes automáticos: LocalState\autopad.txt com linhas "segundos botoesHex duracaoMs".
struct PadEvent { double t; WORD buttons; DWORD ms; };
static std::vector<PadEvent> g_script;
static ULONGLONG g_scriptT0 = 0;
static bool g_scriptLoaded = false;

static void LoadScript() {
    g_scriptLoaded = true;
    FILE* f = nullptr;
    if (_wfopen_s(&f, (g_localW + L"\\autopad.txt").c_str(), L"r") || !f) return;
    double t; unsigned b, ms;
    while (fscanf_s(f, "%lf %x %u", &t, &b, &ms) == 3) g_script.push_back({t, (WORD)b, ms});
    fclose(f);
    g_scriptT0 = GetTickCount64();
    HLog("autopad: %zu eventos", g_script.size());
}

static DWORD g_packet = 0;

static DWORD WINAPI S_XInputGetState(DWORD idx, XInState* st) {
    if (!g_scriptLoaded) LoadScript();
    if (!g_script.empty()) {
        if (idx != 0) return ERROR_DEVICE_NOT_CONNECTED;
        double now = (double)(GetTickCount64() - g_scriptT0) / 1000.0;
        WORD b = 0;
        for (auto& e : g_script)
            if (now >= e.t && now < e.t + e.ms / 1000.0) b |= e.buttons;
        memset(st, 0, sizeof(*st));
        st->dwPacketNumber = ++g_packet;
        st->Gamepad.wButtons = b;
        return ERROR_SUCCESS;
    }
    static std::vector<Gamepad> pads;
    static ULONGLONG nextRefresh = 0;
    static SRWLOCK lock = SRWLOCK_INIT;
    ULONGLONG now = GetTickCount64();
    AcquireSRWLockExclusive(&lock);
    DWORD rc = ERROR_DEVICE_NOT_CONNECTED;
    try {
        if (now >= nextRefresh) {
            nextRefresh = now + 1000;
            pads.clear();
            for (auto const& p : Gamepad::Gamepads()) pads.push_back(p);
        }
        if (idx < pads.size()) {
            GamepadReading r = pads[idx].GetCurrentReading();
            WORD b = 0;
            auto has = [&](GamepadButtons x) { return (r.Buttons & x) == x; };
            if (has(GamepadButtons::DPadUp)) b |= 0x0001;
            if (has(GamepadButtons::DPadDown)) b |= 0x0002;
            if (has(GamepadButtons::DPadLeft)) b |= 0x0004;
            if (has(GamepadButtons::DPadRight)) b |= 0x0008;
            if (has(GamepadButtons::Menu)) b |= 0x0010;
            if (has(GamepadButtons::View)) b |= 0x0020;
            if (has(GamepadButtons::LeftThumbstick)) b |= 0x0040;
            if (has(GamepadButtons::RightThumbstick)) b |= 0x0080;
            if (has(GamepadButtons::LeftShoulder)) b |= 0x0100;
            if (has(GamepadButtons::RightShoulder)) b |= 0x0200;
            if (has(GamepadButtons::A)) b |= 0x1000;
            if (has(GamepadButtons::B)) b |= 0x2000;
            if (has(GamepadButtons::X)) b |= 0x4000;
            if (has(GamepadButtons::Y)) b |= 0x8000;
            auto axis = [](double v) { double s = v * 32767.0; return (SHORT)(s < -32768.0 ? -32768 : s > 32767.0 ? 32767 : s); };
            st->dwPacketNumber = ++g_packet;
            st->Gamepad.wButtons = b;
            st->Gamepad.bLeftTrigger = (BYTE)(r.LeftTrigger * 255.0);
            st->Gamepad.bRightTrigger = (BYTE)(r.RightTrigger * 255.0);
            st->Gamepad.sThumbLX = axis(r.LeftThumbstickX);
            st->Gamepad.sThumbLY = axis(r.LeftThumbstickY);
            st->Gamepad.sThumbRX = axis(r.RightThumbstickX);
            st->Gamepad.sThumbRY = axis(r.RightThumbstickY);
            rc = ERROR_SUCCESS;
        }
    } catch (hresult_error const& e) {
        static bool logged = false;
        if (!logged) { logged = true; HLog("ERRO Gamepad: 0x%08lx", (unsigned long)e.code().value); }
    }
    ReleaseSRWLockExclusive(&lock);
    return rc;
}

const ShimEntry g_inputShims[] = {
    {"xinput1_3.dll", "#2", (void*)&S_XInputGetState},
    {nullptr, nullptr, nullptr},
};

