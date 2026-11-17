// D3D11/DXGI: device real; swapchain proxy (o jogo desenha numa textura nossa, copiada para um swapchain de CoreWindow no
// Present). Enumeração DXGI falsa com modos fixos, porque no console ela pode não existir. D3DReflect adaptado do _43 para o _47.
#include "host.h"
#include <d3d11_2.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <dxgi1_3.h>
#include <wrl/client.h>
#include <cstdio>
#include <map>

using Microsoft::WRL::ComPtr;

template <class T> class Com : public T {
    LONG m_rc = 1;
public:
    virtual ~Com() {}
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&m_rc); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&m_rc);
        if (r == 0) delete this;
        return (ULONG)r;
    }
};

#define OBJECT_STUBS \
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) override { return S_OK; } \
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) override { return S_OK; } \
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) override { return DXGI_ERROR_NOT_FOUND; }

// ---------------------------------------------------------------- enumeração falsa
struct ModeRes { UINT w, h; };
static const ModeRes kModes[] = {{1280, 720}, {1920, 1080}};

class FakeOutput : public Com<IDXGIOutput> {
public:
    OBJECT_STUBS
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** pp) override {
        if (id == __uuidof(IUnknown) || id == __uuidof(IDXGIObject) || id == __uuidof(IDXGIOutput)) { AddRef(); *pp = this; return S_OK; }
        *pp = nullptr; return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetParent(REFIID, void** pp) override { *pp = nullptr; return E_NOINTERFACE; }
    HRESULT STDMETHODCALLTYPE GetDesc(DXGI_OUTPUT_DESC* d) override {
        memset(d, 0, sizeof(*d));
        wcscpy_s(d->DeviceName, L"\\\\.\\DISPLAY1");
        d->DesktopCoordinates = {0, 0, 1920, 1080};
        d->AttachedToDesktop = TRUE;
        d->Rotation = DXGI_MODE_ROTATION_IDENTITY;
        d->Monitor = (HMONITOR)0x10;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDisplayModeList(DXGI_FORMAT fmt, UINT, UINT* num, DXGI_MODE_DESC* out) override {
        UINT n = (UINT)(sizeof(kModes) / sizeof(kModes[0]));
        if (!out) { *num = n; return S_OK; }
        UINT c = min(*num, n);
        for (UINT i = 0; i < c; ++i) {
            memset(&out[i], 0, sizeof(out[i]));
            out[i].Width = kModes[i].w; out[i].Height = kModes[i].h;
            out[i].RefreshRate = {60, 1}; out[i].Format = fmt;
            out[i].ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
            out[i].Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        }
        *num = c;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(const DXGI_MODE_DESC* m, DXGI_MODE_DESC* o, IUnknown*) override {
        *o = *m; o->Width = 1920; o->Height = 1080; o->RefreshRate = {60, 1};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE WaitForVBlank() override { Sleep(16); return S_OK; }
    HRESULT STDMETHODCALLTYPE TakeOwnership(IUnknown*, BOOL) override { return S_OK; }
    void STDMETHODCALLTYPE ReleaseOwnership() override {}
    HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities(DXGI_GAMMA_CONTROL_CAPABILITIES*) override { return DXGI_ERROR_UNSUPPORTED; }
    HRESULT STDMETHODCALLTYPE SetGammaControl(const DXGI_GAMMA_CONTROL*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetGammaControl(DXGI_GAMMA_CONTROL*) override { return DXGI_ERROR_UNSUPPORTED; }
    HRESULT STDMETHODCALLTYPE SetDisplaySurface(IDXGISurface*) override { return DXGI_ERROR_UNSUPPORTED; }
    HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData(IDXGISurface*) override { return DXGI_ERROR_UNSUPPORTED; }
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS*) override { return DXGI_ERROR_UNSUPPORTED; }
};

class FakeAdapter : public Com<IDXGIAdapter> {
public:
    OBJECT_STUBS
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** pp) override {
        if (id == __uuidof(IUnknown) || id == __uuidof(IDXGIObject) || id == __uuidof(IDXGIAdapter)) { AddRef(); *pp = this; return S_OK; }
        *pp = nullptr; return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetParent(REFIID, void** pp) override { *pp = nullptr; return E_NOINTERFACE; }
    HRESULT STDMETHODCALLTYPE EnumOutputs(UINT i, IDXGIOutput** pp) override {
        if (i > 0) { *pp = nullptr; return DXGI_ERROR_NOT_FOUND; }
        *pp = new FakeOutput();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDesc(DXGI_ADAPTER_DESC* d) override {
        memset(d, 0, sizeof(*d));
        wcscpy_s(d->Description, L"Xbox GPU");
        d->DedicatedVideoMemory = (SIZE_T)4ull * 1024 * 1024 * 1024;
        d->SharedSystemMemory = (SIZE_T)1ull * 1024 * 1024 * 1024;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CheckInterfaceSupport(REFGUID, LARGE_INTEGER* v) override { if (v) v->QuadPart = 0x000A000000000000ll; return S_OK; }
};

static HRESULT MakeSwapChain(IUnknown* devUnk, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** out);

class FakeFactory : public Com<IDXGIFactory> {
public:
    OBJECT_STUBS
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** pp) override {
        if (id == __uuidof(IUnknown) || id == __uuidof(IDXGIObject) || id == __uuidof(IDXGIFactory)) { AddRef(); *pp = this; return S_OK; }
        HLog("DXGI: FakeFactory QI nao suportado");
        *pp = nullptr; return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetParent(REFIID, void** pp) override { *pp = nullptr; return E_NOINTERFACE; }
    HRESULT STDMETHODCALLTYPE EnumAdapters(UINT i, IDXGIAdapter** pp) override {
        if (i > 0) { *pp = nullptr; return DXGI_ERROR_NOT_FOUND; }
        *pp = new FakeAdapter();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetWindowAssociation(HWND* h) override { *h = nullptr; return S_OK; }
    HRESULT STDMETHODCALLTYPE CreateSwapChain(IUnknown* dev, DXGI_SWAP_CHAIN_DESC* d, IDXGISwapChain** out) override { return MakeSwapChain(dev, d, out); }
    HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(HMODULE, IDXGIAdapter**) override { return DXGI_ERROR_UNSUPPORTED; }
};

static HRESULT WINAPI S_CreateDXGIFactory(REFIID id, void** pp) {
    HLog("CreateDXGIFactory (falso)");
    auto* f = new FakeFactory();
    HRESULT hr = f->QueryInterface(id, pp);
    f->Release();
    return hr;
}

// ---------------------------------------------------------------- swapchain proxy
struct ProcMem { DWORD cb, faults; SIZE_T peakWs, ws, peakPaged, paged, peakNp, np, commit, peakCommit, priv; };
typedef BOOL(WINAPI* GetMem_t)(HANDLE, ProcMem*, DWORD);

static std::string MemoryText() {
    static auto f = (GetMem_t)HostSysProc("kernelbase.dll", "K32GetProcessMemoryInfo");
    ProcMem m{};
    m.cb = sizeof(m);
    char b[96] = "mem ?";
    if (f && f(GetCurrentProcess(), &m, sizeof(m))) sprintf_s(b, "mem %.0f MB (pico %.0f MB)", m.commit / 1048576.0, m.peakCommit / 1048576.0);
    return b;
}

static void DumpFrame(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11Texture2D* tex, unsigned long long n) {
    D3D11_TEXTURE2D_DESC d;
    tex->GetDesc(&d);
    if (d.SampleDesc.Count > 1) return;
    d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> st;
    if (FAILED(dev->CreateTexture2D(&d, nullptr, &st))) return;
    ctx->CopyResource(st.Get(), tex);
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ctx->Map(st.Get(), 0, D3D11_MAP_READ, 0, &m))) return;
    std::wstring dir = g_localW + L"\\frames";
    CreateDirectoryW(dir.c_str(), nullptr);
    wchar_t name[64];
    swprintf_s(name, L"\\f%06llu.bmp", n);
    HANDLE f = CreateFileW((dir + name).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        BITMAPFILEHEADER fh{}; BITMAPINFOHEADER ih{};
        DWORD row = d.Width * 3, pad = (4 - row % 4) % 4, size = (row + pad) * d.Height;
        fh.bfType = 0x4D42; fh.bfOffBits = sizeof(fh) + sizeof(ih); fh.bfSize = fh.bfOffBits + size;
        ih.biSize = sizeof(ih); ih.biWidth = (LONG)d.Width; ih.biHeight = (LONG)d.Height; ih.biPlanes = 1; ih.biBitCount = 24; ih.biSizeImage = size;
        DWORD w;
        WriteFile(f, &fh, sizeof(fh), &w, nullptr);
        WriteFile(f, &ih, sizeof(ih), &w, nullptr);
        std::string line(row + pad, '\0');
        for (int y = (int)d.Height - 1; y >= 0; --y) {
            const BYTE* s = (const BYTE*)m.pData + (size_t)y * m.RowPitch;
            for (UINT x = 0; x < d.Width; ++x) { line[x * 3] = (char)s[x * 4 + 2]; line[x * 3 + 1] = (char)s[x * 4 + 1]; line[x * 3 + 2] = (char)s[x * 4]; }
            WriteFile(f, line.data(), (DWORD)line.size(), &w, nullptr);
        }
        CloseHandle(f);
    }
    ctx->Unmap(st.Get(), 0);
    HLog("frame %llu salvo", n);
}

class ProxySwapChain : public Com<IDXGISwapChain> {
    ComPtr<ID3D11Device> m_dev;
    ComPtr<ID3D11DeviceContext> m_ctx;
    ComPtr<IDXGISwapChain1> m_real;
    ComPtr<ID3D11Texture2D> m_off;
    DXGI_SWAP_CHAIN_DESC m_desc;
    unsigned long long m_frames = 0;
    ULONGLONG m_t0 = 0;
    bool m_dump;
    unsigned m_dumpEvery = 300;
    BOOL m_fullscreen = FALSE;

    static DXGI_FORMAT RealFormat(DXGI_FORMAT f) { return f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || f == DXGI_FORMAT_B8G8R8A8_UNORM ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM; }

    HRESULT MakeOffscreen() {
        m_off.Reset();
        D3D11_TEXTURE2D_DESC t{};
        t.Width = m_desc.BufferDesc.Width; t.Height = m_desc.BufferDesc.Height; t.MipLevels = 1; t.ArraySize = 1;
        t.Format = m_desc.BufferDesc.Format; t.SampleDesc = m_desc.SampleDesc; t.Usage = D3D11_USAGE_DEFAULT;
        t.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        HRESULT hr = m_dev->CreateTexture2D(&t, nullptr, &m_off);
        if (FAILED(hr)) HLog("ERRO: textura do backbuffer %ux%u fmt=%d: 0x%08lx", t.Width, t.Height, t.Format, hr);
        return hr;
    }

public:
    ProxySwapChain(ID3D11Device* dev, const DXGI_SWAP_CHAIN_DESC& d) : m_dev(dev), m_desc(d) {
        dev->GetImmediateContext(&m_ctx);
        m_dump = HostFlagFile(L"framedump.txt");
        if (m_dump) {
            FILE* f = nullptr;
            if (!_wfopen_s(&f, (g_localW + L"\\framedump.txt").c_str(), L"r") && f) {
                unsigned v = 0;
                if (fscanf_s(f, "%u", &v) == 1 && v >= 10) m_dumpEvery = v;
                fclose(f);
            }
        }
    }

    HRESULT Init() {
        if (m_desc.BufferDesc.Width == 0) m_desc.BufferDesc.Width = 1920;
        if (m_desc.BufferDesc.Height == 0) m_desc.BufferDesc.Height = 1080;
        HRESULT hr = MakeOffscreen();
        if (FAILED(hr)) return hr;
        ComPtr<IDXGIDevice1> dxgiDev;
        ComPtr<IDXGIAdapter> ad;
        ComPtr<IDXGIFactory2> fac;
        if (FAILED(hr = m_dev.As(&dxgiDev)) || FAILED(hr = dxgiDev->GetAdapter(&ad)) || FAILED(hr = ad->GetParent(IID_PPV_ARGS(&fac)))) { HLog("ERRO: fabrica DXGI real: 0x%08lx", hr); return hr; }
        DXGI_SWAP_CHAIN_DESC1 r{};
        r.Width = m_desc.BufferDesc.Width; r.Height = m_desc.BufferDesc.Height; r.Format = RealFormat(m_desc.BufferDesc.Format);
        r.SampleDesc.Count = 1; r.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; r.BufferCount = 2;
        r.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; r.Scaling = DXGI_SCALING_STRETCH; r.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        hr = fac->CreateSwapChainForCoreWindow(m_dev.Get(), g_coreWindowUnk, &r, nullptr, &m_real);
        if (FAILED(hr)) { HLog("ERRO: CreateSwapChainForCoreWindow: 0x%08lx", hr); return hr; }
        HLog("swapchain: %ux%u fmt=%d amostras=%u (proxy pronto)", r.Width, r.Height, m_desc.BufferDesc.Format, m_desc.SampleDesc.Count);
        return S_OK;
    }

    OBJECT_STUBS
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** pp) override {
        if (id == __uuidof(IUnknown) || id == __uuidof(IDXGIObject) || id == __uuidof(IDXGIDeviceSubObject) || id == __uuidof(IDXGISwapChain)) { AddRef(); *pp = this; return S_OK; }
        *pp = nullptr; return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetParent(REFIID id, void** pp) override { return S_CreateDXGIFactory(id, pp); }
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID id, void** pp) override { return m_dev->QueryInterface(id, pp); }

    HRESULT STDMETHODCALLTYPE Present(UINT sync, UINT flags) override {
        if (flags & DXGI_PRESENT_TEST) return S_OK;
        if (!m_t0) { m_t0 = GetTickCount64(); HLog("primeiro Present"); }
        ++m_frames;
        if (m_dump && (m_frames == 30 || m_frames % m_dumpEvery == 0)) DumpFrame(m_dev.Get(), m_ctx.Get(), m_off.Get(), m_frames);
        if (m_frames % 600 == 0) {
            ULONGLONG now = GetTickCount64();
            HLog("%llu quadros, %.1f fps, %s", m_frames, 600000.0 / (double)(now - m_t0), MemoryText().c_str());
            m_t0 = now;
        }
        ComPtr<ID3D11Texture2D> bb;
        if (m_real && SUCCEEDED(m_real->GetBuffer(0, IID_PPV_ARGS(&bb)))) {
            if (m_desc.SampleDesc.Count > 1) m_ctx->ResolveSubresource(bb.Get(), 0, m_off.Get(), 0, RealFormat(m_desc.BufferDesc.Format));
            else m_ctx->CopyResource(bb.Get(), m_off.Get());
            HRESULT hr = m_real->Present(sync, 0);
            if (FAILED(hr)) {
                HLog("Present falhou: 0x%08lx (removed=0x%08lx)", hr, m_dev->GetDeviceRemovedReason());
                return hr;
            }
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetBuffer(UINT, REFIID id, void** pp) override { return m_off ? m_off->QueryInterface(id, pp) : E_FAIL; }
    HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL f, IDXGIOutput*) override { m_fullscreen = f; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* f, IDXGIOutput** o) override { if (f) *f = m_fullscreen; if (o) *o = nullptr; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* d) override { *d = m_desc; return S_OK; }
    HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT, UINT w, UINT h, DXGI_FORMAT fmt, UINT) override {
        if (w == 0) w = m_desc.BufferDesc.Width;
        if (h == 0) h = m_desc.BufferDesc.Height;
        if (fmt != DXGI_FORMAT_UNKNOWN) m_desc.BufferDesc.Format = fmt;
        HLog("ResizeBuffers %ux%u", w, h);
        m_desc.BufferDesc.Width = w; m_desc.BufferDesc.Height = h;
        m_ctx->ClearState();
        m_ctx->Flush();
        HRESULT hr = MakeOffscreen();
        if (FAILED(hr)) return hr;
        if (m_real) {
            hr = m_real->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
            if (FAILED(hr)) HLog("ERRO: ResizeBuffers real: 0x%08lx", hr);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** o) override { *o = new FakeOutput(); return S_OK; }
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS*) override { return DXGI_ERROR_FRAME_STATISTICS_DISJOINT; }
    HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* c) override { *c = (UINT)m_frames; return S_OK; }
};

static HRESULT MakeSwapChain(IUnknown* devUnk, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** out) {
    ComPtr<ID3D11Device> dev;
    HRESULT hr = devUnk->QueryInterface(IID_PPV_ARGS(&dev));
    if (FAILED(hr)) return hr;
    auto* sc = new ProxySwapChain(dev.Get(), *desc);
    hr = sc->Init();
    if (FAILED(hr)) { sc->Release(); return hr; }
    *out = sc;
    return S_OK;
}

static HRESULT WINAPI S_D3D11CreateDeviceAndSwapChain(IDXGIAdapter*, D3D_DRIVER_TYPE type, HMODULE sw, UINT flags, const D3D_FEATURE_LEVEL* fls, UINT nfl, UINT sdk,
                                                      const DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** ppSc, ID3D11Device** ppDev, D3D_FEATURE_LEVEL* pFl, ID3D11DeviceContext** ppCtx) {
    HLog("D3D11CreateDeviceAndSwapChain tipo=%d flags=0x%x niveis=%u primeiro=0x%x %ux%u fmt=%d", type, flags, nfl, nfl ? fls[0] : 0, desc ? desc->BufferDesc.Width : 0, desc ? desc->BufferDesc.Height : 0, desc ? desc->BufferDesc.Format : 0);
    ComPtr<ID3D11Device> dev;
    ComPtr<ID3D11DeviceContext> ctx;
    D3D_FEATURE_LEVEL got = (D3D_FEATURE_LEVEL)0;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, sw, flags & ~D3D11_CREATE_DEVICE_DEBUG, fls, nfl, sdk, &dev, &got, &ctx);
    HLog("D3D11CreateDevice -> 0x%08lx nivel=0x%x", hr, got);
    if (FAILED(hr)) {
        // O jogo exige 11_0. Testa níveis menores só para o log dizer se o console está em modo App.
        const D3D_FEATURE_LEVEL low[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3};
        for (auto l : low) {
            ComPtr<ID3D11Device> d2;
            HRESULT h2 = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &l, 1, sdk, &d2, nullptr, nullptr);
            HLog("  teste nivel 0x%x -> 0x%08lx", l, h2);
        }
        HLog("ERRO: sem Direct3D 11 no nivel exigido. No console: marque o pacote como Game no Dev Home.");
        return hr;
    }
    if (ppSc && desc) {
        DXGI_SWAP_CHAIN_DESC d = *desc;
        hr = MakeSwapChain(dev.Get(), &d, ppSc);
        if (FAILED(hr)) return hr;
    }
    if (ppDev) *ppDev = dev.Detach();
    if (ppCtx) *ppCtx = ctx.Detach();
    if (pFl) *pFl = got;
    return S_OK;
}

// ---------------------------------------------------------------- D3DReflect (_43 -> _47)
// As structs de saída do _47 são maiores que as do _43; copiamos só o tamanho antigo para não estourar a memória do jogo.
static const size_t kOldSigDesc = 32, kOldVarDesc = 32, kOldTypeDesc = 28;
static const GUID kIID_Reflect47 = {0x8d536ca1, 0x0cca, 0x4956, {0xa8, 0x37, 0x78, 0x69, 0x63, 0x75, 0x55, 0x84}};
static const GUID kIID_Reflect43 = {0x0a233719, 0x3960, 0x4578, {0x9d, 0x7c, 0x20, 0x3b, 0x8b, 0x1d, 0x9c, 0xc1}};

template <class Wrapper, class Real> static Wrapper* WrapCached(Real* r) {
    static SRWLOCK lock = SRWLOCK_INIT;
    static std::map<Real*, Wrapper*> cache;
    if (!r) return nullptr;
    AcquireSRWLockExclusive(&lock);
    auto*& w = cache[r];
    if (!w) w = new Wrapper(r);
    Wrapper* out = w;
    ReleaseSRWLockExclusive(&lock);
    return out;
}

class TypeW : public ID3D11ShaderReflectionType {
    ID3D11ShaderReflectionType* r;
public:
    explicit TypeW(ID3D11ShaderReflectionType* x) : r(x) {}
    static TypeW* Of(ID3D11ShaderReflectionType* x) { return WrapCached<TypeW>(x); }
    static ID3D11ShaderReflectionType* Unwrap(ID3D11ShaderReflectionType* x) { return x ? static_cast<TypeW*>(x)->r : nullptr; }
    HRESULT STDMETHODCALLTYPE GetDesc(D3D11_SHADER_TYPE_DESC* d) override {
        D3D11_SHADER_TYPE_DESC t{};
        HRESULT hr = r->GetDesc(&t);
        if (SUCCEEDED(hr)) memcpy(d, &t, kOldTypeDesc);
        return hr;
    }
    ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetMemberTypeByIndex(UINT i) override { return Of(r->GetMemberTypeByIndex(i)); }
    ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetMemberTypeByName(LPCSTR n) override { return Of(r->GetMemberTypeByName(n)); }
    LPCSTR STDMETHODCALLTYPE GetMemberTypeName(UINT i) override { return r->GetMemberTypeName(i); }
    HRESULT STDMETHODCALLTYPE IsEqual(ID3D11ShaderReflectionType* o) override { return r->IsEqual(Unwrap(o)); }
    ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetSubType() override { return Of(r->GetSubType()); }
    ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetBaseClass() override { return Of(r->GetBaseClass()); }
    UINT STDMETHODCALLTYPE GetNumInterfaces() override { return r->GetNumInterfaces(); }
    ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetInterfaceByIndex(UINT i) override { return Of(r->GetInterfaceByIndex(i)); }
    HRESULT STDMETHODCALLTYPE IsOfType(ID3D11ShaderReflectionType* o) override { return r->IsOfType(Unwrap(o)); }
    HRESULT STDMETHODCALLTYPE ImplementsInterface(ID3D11ShaderReflectionType* o) override { return r->ImplementsInterface(Unwrap(o)); }
};

class CBW;
class VarW : public ID3D11ShaderReflectionVariable {
    ID3D11ShaderReflectionVariable* r;
public:
    explicit VarW(ID3D11ShaderReflectionVariable* x) : r(x) {}
    static VarW* Of(ID3D11ShaderReflectionVariable* x) { return WrapCached<VarW>(x); }
    HRESULT STDMETHODCALLTYPE GetDesc(D3D11_SHADER_VARIABLE_DESC* d) override {
        D3D11_SHADER_VARIABLE_DESC t{};
        HRESULT hr = r->GetDesc(&t);
        if (SUCCEEDED(hr)) memcpy(d, &t, kOldVarDesc);
        return hr;
    }
    ID3D11ShaderReflectionType* STDMETHODCALLTYPE GetType() override { return TypeW::Of(r->GetType()); }
    ID3D11ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetBuffer() override;
    UINT STDMETHODCALLTYPE GetInterfaceSlot(UINT i) override { return r->GetInterfaceSlot(i); }
};

class CBW : public ID3D11ShaderReflectionConstantBuffer {
    ID3D11ShaderReflectionConstantBuffer* r;
public:
    explicit CBW(ID3D11ShaderReflectionConstantBuffer* x) : r(x) {}
    static CBW* Of(ID3D11ShaderReflectionConstantBuffer* x) { return WrapCached<CBW>(x); }
    HRESULT STDMETHODCALLTYPE GetDesc(D3D11_SHADER_BUFFER_DESC* d) override { return r->GetDesc(d); }
    ID3D11ShaderReflectionVariable* STDMETHODCALLTYPE GetVariableByIndex(UINT i) override { return VarW::Of(r->GetVariableByIndex(i)); }
    ID3D11ShaderReflectionVariable* STDMETHODCALLTYPE GetVariableByName(LPCSTR n) override { return VarW::Of(r->GetVariableByName(n)); }
};

ID3D11ShaderReflectionConstantBuffer* VarW::GetBuffer() { return CBW::Of(r->GetBuffer()); }

class ReflW : public Com<ID3D11ShaderReflection> {
    ComPtr<ID3D11ShaderReflection> r;
    HRESULT Sig(HRESULT hr, const D3D11_SIGNATURE_PARAMETER_DESC& t, D3D11_SIGNATURE_PARAMETER_DESC* d) {
        if (SUCCEEDED(hr)) memcpy(d, &t, kOldSigDesc);
        return hr;
    }
public:
    explicit ReflW(ID3D11ShaderReflection* x) : r(x) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** pp) override {
        if (id == __uuidof(IUnknown) || id == kIID_Reflect43 || id == kIID_Reflect47) { AddRef(); *pp = this; return S_OK; }
        *pp = nullptr; return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetDesc(D3D11_SHADER_DESC* d) override { return r->GetDesc(d); }
    ID3D11ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetConstantBufferByIndex(UINT i) override { return CBW::Of(r->GetConstantBufferByIndex(i)); }
    ID3D11ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetConstantBufferByName(LPCSTR n) override { return CBW::Of(r->GetConstantBufferByName(n)); }
    HRESULT STDMETHODCALLTYPE GetResourceBindingDesc(UINT i, D3D11_SHADER_INPUT_BIND_DESC* d) override { return r->GetResourceBindingDesc(i, d); }
    HRESULT STDMETHODCALLTYPE GetInputParameterDesc(UINT i, D3D11_SIGNATURE_PARAMETER_DESC* d) override { D3D11_SIGNATURE_PARAMETER_DESC t{}; return Sig(r->GetInputParameterDesc(i, &t), t, d); }
    HRESULT STDMETHODCALLTYPE GetOutputParameterDesc(UINT i, D3D11_SIGNATURE_PARAMETER_DESC* d) override { D3D11_SIGNATURE_PARAMETER_DESC t{}; return Sig(r->GetOutputParameterDesc(i, &t), t, d); }
    HRESULT STDMETHODCALLTYPE GetPatchConstantParameterDesc(UINT i, D3D11_SIGNATURE_PARAMETER_DESC* d) override { D3D11_SIGNATURE_PARAMETER_DESC t{}; return Sig(r->GetPatchConstantParameterDesc(i, &t), t, d); }
    ID3D11ShaderReflectionVariable* STDMETHODCALLTYPE GetVariableByName(LPCSTR n) override { return VarW::Of(r->GetVariableByName(n)); }
    HRESULT STDMETHODCALLTYPE GetResourceBindingDescByName(LPCSTR n, D3D11_SHADER_INPUT_BIND_DESC* d) override { return r->GetResourceBindingDescByName(n, d); }
    UINT STDMETHODCALLTYPE GetMovInstructionCount() override { return r->GetMovInstructionCount(); }
    UINT STDMETHODCALLTYPE GetMovcInstructionCount() override { return r->GetMovcInstructionCount(); }
    UINT STDMETHODCALLTYPE GetConversionInstructionCount() override { return r->GetConversionInstructionCount(); }
    UINT STDMETHODCALLTYPE GetBitwiseInstructionCount() override { return r->GetBitwiseInstructionCount(); }
    D3D_PRIMITIVE STDMETHODCALLTYPE GetGSInputPrimitive() override { return r->GetGSInputPrimitive(); }
    BOOL STDMETHODCALLTYPE IsSampleFrequencyShader() override { return r->IsSampleFrequencyShader(); }
    UINT STDMETHODCALLTYPE GetNumInterfaceSlots() override { return r->GetNumInterfaceSlots(); }
    HRESULT STDMETHODCALLTYPE GetMinFeatureLevel(D3D_FEATURE_LEVEL* l) override { return r->GetMinFeatureLevel(l); }
    UINT STDMETHODCALLTYPE GetThreadGroupSize(UINT* x, UINT* y, UINT* z) override { return r->GetThreadGroupSize(x, y, z); }
    UINT64 STDMETHODCALLTYPE GetRequiresFlags() override { return r->GetRequiresFlags(); }
};

static HRESULT WINAPI S_D3DReflect(LPCVOID src, SIZE_T size, REFIID, void** out) {
    ComPtr<ID3D11ShaderReflection> r;
    HRESULT hr = D3DReflect(src, size, kIID_Reflect47, (void**)r.GetAddressOf());
    if (FAILED(hr)) { HLog("D3DReflect falhou: 0x%08lx", hr); return hr; }
    *out = static_cast<ID3D11ShaderReflection*>(new ReflW(r.Get()));
    return S_OK;
}

const ShimEntry g_dxShims[] = {
    {"d3d11.dll", "D3D11CreateDeviceAndSwapChain", (void*)&S_D3D11CreateDeviceAndSwapChain},
    {"dxgi.dll", "CreateDXGIFactory", (void*)&S_CreateDXGIFactory},
    {"d3dcompiler_43.dll", "D3DReflect", (void*)&S_D3DReflect},
    {nullptr, nullptr, nullptr},
};
