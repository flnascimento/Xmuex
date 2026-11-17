// ShenmueGame.exe: app UWP nativo (CoreWindow, sem XAML/CLR). É aberto pelo launcher via protocolo shenmue-uwp-run:1 ou :2.
// Precisa ser um .exe próprio: o executável do jogo assume que o TLS dele é o índice 0 do processo, que é sempre do .exe principal.
#include "host.h"
#include <unknwn.h>
#include <cstdio>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.ViewManagement.h>

using namespace winrt;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::ViewManagement;

int HostStart(const wchar_t* gameRoot, int game);
extern IUnknown* g_coreWindowUnk;

// Etapas antes do host.log existir. Se este arquivo nem aparecer, o .exe não chegou a carregar.
static void BootLog(const char* text) {
    try {
        std::wstring p = std::wstring(winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path().c_str()) + L"\\boot.log";
        HANDLE f = CreateFileW(p.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) return;
        DWORD w;
        WriteFile(f, text, (DWORD)strlen(text), &w, nullptr);
        WriteFile(f, "\r\n", 2, &w, nullptr);
        CloseHandle(f);
    } catch (...) {}
}

struct App : implements<App, IFrameworkViewSource, IFrameworkView> {
    int m_game = 0;

    IFrameworkView CreateView() { return *this; }

    void Initialize(CoreApplicationView const& view) {
        BootLog("Initialize");
        view.Activated([this](CoreApplicationView const&, IActivatedEventArgs const& args) {
            if (args.Kind() == ActivationKind::Protocol) {
                auto uri = args.as<ProtocolActivatedEventArgs>().Uri();
                std::wstring s = uri.AbsoluteUri().c_str();
                m_game = s.find(L'2', s.find(L':')) != std::wstring::npos ? 2 : 1;
            }
            CoreWindow::GetForCurrentThread().Activate();
        });
    }

    void Load(hstring const&) {}
    void Uninitialize() {}

    void SetWindow(CoreWindow const& window) {
        BootLog("SetWindow");
        ApplicationView::GetForCurrentView().SetDesiredBoundsMode(ApplicationViewBoundsMode::UseCoreWindow);
        g_coreWindowUnk = reinterpret_cast<IUnknown*>(get_abi(window));
        g_coreWindowUnk->AddRef();
        // No Xbox o B dispara "voltar" do sistema; sem tratar, o console tira o app da tela.
        SystemNavigationManager::GetForCurrentView().BackRequested([](auto&&, BackRequestedEventArgs const& e) { e.Handled(true); });
        // Botões do controle também chegam como teclas (VK_GAMEPAD_*). O jogo já lê o controle via XInput; repassar como
        // teclado faria a interface trocar para ícones de teclado.
        auto isPad = [](KeyEventArgs const& a) { unsigned vk = (unsigned)a.VirtualKey(); return vk >= 0xC3 && vk <= 0xDA; };
        window.KeyDown([isPad](CoreWindow const&, KeyEventArgs const& a) {
            if (isPad(a)) { a.Handled(true); return; }
            HostQueueKey((unsigned)a.VirtualKey(), true);
        });
        window.KeyUp([isPad](CoreWindow const&, KeyEventArgs const& a) {
            if (isPad(a)) { a.Handled(true); return; }
            HostQueueKey((unsigned)a.VirtualKey(), false);
        });
    }

    void Run() {
        BootLog("Run");
        CoreWindow window = CoreWindow::GetForCurrentThread();
        window.Activate();
        window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
        std::wstring local = winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path().c_str();
        if (m_game == 0) { // aberto direto pelo tile: usa o último jogo escolhido no launcher
            m_game = 1;
            FILE* f = nullptr;
            if (!_wfopen_s(&f, (local + L"\\lastgame.txt").c_str(), L"r") && f) {
                if (fgetc(f) == '2') m_game = 2;
                fclose(f);
            }
        }
        int rc = HostStart((local + L"\\game").c_str(), m_game);
        char msg[64];
        sprintf_s(msg, "HostStart jogo=%d rc=%d", m_game, rc);
        BootLog(msg);
        if (rc != 0) {
            CoreApplication::Exit(); // detalhes em LocalState\\host.log (aparecem no launcher)
            return;
        }
        window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
    }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    BootLog("wWinMain");
    try {
        CoreApplication::Run(make<App>());
    } catch (hresult_error const& e) {
        char msg[64];
        sprintf_s(msg, "ERRO WinRT 0x%08lx", (unsigned long)e.code().value);
        BootLog(msg);
    }
    return 0;
}
