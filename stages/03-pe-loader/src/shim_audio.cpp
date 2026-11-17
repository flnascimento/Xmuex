// XAudio2 2.7 (DirectX SDK, criado via COM) sobre o XAudio2 2.9 do sistema. As vtables abaixo seguem o layout do 2.7.
#include "host.h"
#include <xaudio2.h>
#include <xaudio2fx.h>
#include <xapofx.h>
#include <vector>

static const GUID kCLSID_XAudio27 = {0x5a508685, 0xa254, 0x4fba, {0x9b, 0x82, 0x9a, 0x24, 0xb0, 0x03, 0x06, 0xaf}};
static const GUID kCLSID_Reverb27 = {0x6a93130e, 0x1d53, 0x41d1, {0xa9, 0xcf, 0xe7, 0x58, 0x80, 0x0b, 0xb1, 0x79}};

struct XA27_VoiceDetails { UINT32 CreationFlags, InputChannels, InputSampleRate; };
struct XA27_DeviceDetails { WCHAR DeviceID[256]; WCHAR DisplayName[256]; UINT32 Role; WAVEFORMATEXTENSIBLE OutputFormat; };

#pragma pack(push, 1)
struct XA27_Reverb {
    float WetDryMix; UINT32 ReflectionsDelay; BYTE ReverbDelay, RearDelay, PositionLeft, PositionRight, PositionMatrixLeft, PositionMatrixRight,
        EarlyDiffusion, LateDiffusion, LowEQGain, LowEQCutoff, HighEQGain, HighEQCutoff;
    float RoomFilterFreq, RoomFilterMain, RoomFilterHF, ReflectionsGain, ReverbGain, DecayTime, Density, RoomSize;
};
#pragma pack(pop)

// Carregado sob demanda: se o XAudio2 faltar, o jogo só fica sem som em vez de nem abrir.
typedef HRESULT(WINAPI* XAudio2Create_t)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);
typedef HRESULT(WINAPI* CreateAudioReverb_t)(IUnknown**);
typedef HRESULT(WINAPI* CreateFX_t)(REFCLSID, IUnknown**, const void*, UINT32);

class VoiceP;
static IXAudio2Voice* RealOf(void* proxy);

// Converte lista de envios (proxies -> vozes reais). Vive só durante a chamada.
struct Sends {
    std::vector<XAUDIO2_SEND_DESCRIPTOR> items;
    XAUDIO2_VOICE_SENDS list{};
    const XAUDIO2_VOICE_SENDS* get(const XAUDIO2_VOICE_SENDS* in) {
        if (!in) return nullptr;
        items.resize(in->SendCount);
        for (UINT32 i = 0; i < in->SendCount; ++i) { items[i].Flags = in->pSends[i].Flags; items[i].pOutputVoice = RealOf(in->pSends[i].pOutputVoice); }
        list.SendCount = in->SendCount;
        list.pSends = items.empty() ? nullptr : items.data();
        return &list;
    }
};

class VoiceP {
public:
    IXAudio2Voice* real;
    bool isReverb = false;
    explicit VoiceP(IXAudio2Voice* r) : real(r) {}
    // Layout IXAudio2Voice 2.7 — sem destrutor virtual para não alterar a vtable.
    virtual void STDMETHODCALLTYPE GetVoiceDetails(XA27_VoiceDetails* d) { XAUDIO2_VOICE_DETAILS t{}; real->GetVoiceDetails(&t); d->CreationFlags = t.CreationFlags; d->InputChannels = t.InputChannels; d->InputSampleRate = t.InputSampleRate; }
    virtual HRESULT STDMETHODCALLTYPE SetOutputVoices(const XAUDIO2_VOICE_SENDS* s) { Sends c; return real->SetOutputVoices(c.get(s)); }
    virtual HRESULT STDMETHODCALLTYPE SetEffectChain(const XAUDIO2_EFFECT_CHAIN* e) { return real->SetEffectChain(e); }
    virtual HRESULT STDMETHODCALLTYPE EnableEffect(UINT32 i, UINT32 op) { return real->EnableEffect(i, op); }
    virtual HRESULT STDMETHODCALLTYPE DisableEffect(UINT32 i, UINT32 op) { return real->DisableEffect(i, op); }
    virtual void STDMETHODCALLTYPE GetEffectState(UINT32 i, BOOL* e) { real->GetEffectState(i, e); }
    virtual HRESULT STDMETHODCALLTYPE SetEffectParameters(UINT32 i, const void* p, UINT32 sz, UINT32 op) {
        if (sz == sizeof(XA27_Reverb)) { // reverb do 2.7 não tem SideDelay nem DisableLateField
            const auto* o = (const XA27_Reverb*)p;
            XAUDIO2FX_REVERB_PARAMETERS n{};
            n.WetDryMix = o->WetDryMix; n.ReflectionsDelay = o->ReflectionsDelay; n.ReverbDelay = o->ReverbDelay; n.RearDelay = o->RearDelay; n.SideDelay = o->RearDelay;
            n.PositionLeft = o->PositionLeft; n.PositionRight = o->PositionRight; n.PositionMatrixLeft = o->PositionMatrixLeft; n.PositionMatrixRight = o->PositionMatrixRight;
            n.EarlyDiffusion = o->EarlyDiffusion; n.LateDiffusion = o->LateDiffusion; n.LowEQGain = o->LowEQGain; n.LowEQCutoff = o->LowEQCutoff; n.HighEQGain = o->HighEQGain; n.HighEQCutoff = o->HighEQCutoff;
            n.RoomFilterFreq = o->RoomFilterFreq; n.RoomFilterMain = o->RoomFilterMain; n.RoomFilterHF = o->RoomFilterHF; n.ReflectionsGain = o->ReflectionsGain; n.ReverbGain = o->ReverbGain;
            n.DecayTime = o->DecayTime; n.Density = o->Density; n.RoomSize = o->RoomSize; n.DisableLateField = FALSE;
            return real->SetEffectParameters(i, &n, sizeof(n), op);
        }
        return real->SetEffectParameters(i, p, sz, op);
    }
    virtual HRESULT STDMETHODCALLTYPE GetEffectParameters(UINT32 i, void* p, UINT32 sz) { return real->GetEffectParameters(i, p, sz); }
    virtual HRESULT STDMETHODCALLTYPE SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS* p, UINT32 op) { return real->SetFilterParameters(p, op); }
    virtual void STDMETHODCALLTYPE GetFilterParameters(XAUDIO2_FILTER_PARAMETERS* p) { real->GetFilterParameters(p); }
    virtual HRESULT STDMETHODCALLTYPE SetOutputFilterParameters(void* dest, const XAUDIO2_FILTER_PARAMETERS* p, UINT32 op) { return real->SetOutputFilterParameters(RealOf(dest), p, op); }
    virtual void STDMETHODCALLTYPE GetOutputFilterParameters(void* dest, XAUDIO2_FILTER_PARAMETERS* p) { real->GetOutputFilterParameters(RealOf(dest), p); }
    virtual HRESULT STDMETHODCALLTYPE SetVolume(float v, UINT32 op) { return real->SetVolume(v, op); }
    virtual void STDMETHODCALLTYPE GetVolume(float* v) { real->GetVolume(v); }
    virtual HRESULT STDMETHODCALLTYPE SetChannelVolumes(UINT32 n, const float* v, UINT32 op) { return real->SetChannelVolumes(n, v, op); }
    virtual void STDMETHODCALLTYPE GetChannelVolumes(UINT32 n, float* v) { real->GetChannelVolumes(n, v); }
    virtual HRESULT STDMETHODCALLTYPE SetOutputMatrix(void* dest, UINT32 s, UINT32 d, const float* m, UINT32 op) { return real->SetOutputMatrix(RealOf(dest), s, d, m, op); }
    virtual void STDMETHODCALLTYPE GetOutputMatrix(void* dest, UINT32 s, UINT32 d, float* m) { real->GetOutputMatrix(RealOf(dest), s, d, m); }
    virtual void STDMETHODCALLTYPE DestroyVoice() { real->DestroyVoice(); delete this; }
};

static IXAudio2Voice* RealOf(void* proxy) { return proxy ? ((VoiceP*)proxy)->real : nullptr; }

class SourceP : public VoiceP {
    IXAudio2SourceVoice* src;
public:
    explicit SourceP(IXAudio2SourceVoice* s) : VoiceP(s), src(s) {}
    virtual HRESULT STDMETHODCALLTYPE Start(UINT32 f, UINT32 op) { return src->Start(f, op); }
    virtual HRESULT STDMETHODCALLTYPE Stop(UINT32 f, UINT32 op) { return src->Stop(f, op); }
    virtual HRESULT STDMETHODCALLTYPE SubmitSourceBuffer(const XAUDIO2_BUFFER* b, const XAUDIO2_BUFFER_WMA* w) { return src->SubmitSourceBuffer(b, w); }
    virtual HRESULT STDMETHODCALLTYPE FlushSourceBuffers() { return src->FlushSourceBuffers(); }
    virtual HRESULT STDMETHODCALLTYPE Discontinuity() { return src->Discontinuity(); }
    virtual HRESULT STDMETHODCALLTYPE ExitLoop(UINT32 op) { return src->ExitLoop(op); }
    virtual void STDMETHODCALLTYPE GetState(XAUDIO2_VOICE_STATE* s) { src->GetState(s, 0); }
    virtual HRESULT STDMETHODCALLTYPE SetFrequencyRatio(float r, UINT32 op) { return src->SetFrequencyRatio(r, op); }
    virtual void STDMETHODCALLTYPE GetFrequencyRatio(float* r) { src->GetFrequencyRatio(r); }
    virtual HRESULT STDMETHODCALLTYPE SetSourceSampleRate(UINT32 r) { return src->SetSourceSampleRate(r); }
};

// Layout IXAudio2 2.7
class XA27 : public IUnknown {
    LONG rc = 1;
    IXAudio2* x = nullptr;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** pp) override { AddRef(); *pp = this; return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&rc);
        if (r == 0) { if (x) x->Release(); delete this; }
        return (ULONG)r;
    }
    virtual HRESULT STDMETHODCALLTYPE GetDeviceCount(UINT32* n) { *n = 1; return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE GetDeviceDetails(UINT32 i, XA27_DeviceDetails* d) {
        if (i != 0) return E_INVALIDARG;
        memset(d, 0, sizeof(*d));
        wcscpy_s(d->DeviceID, L"default");
        wcscpy_s(d->DisplayName, L"Xbox Audio");
        d->Role = 0xF; // GlobalDefaultDevice
        WAVEFORMATEX& f = d->OutputFormat.Format;
        f.wFormatTag = WAVE_FORMAT_EXTENSIBLE; f.nChannels = 2; f.nSamplesPerSec = 48000; f.wBitsPerSample = 32; f.nBlockAlign = 8; f.nAvgBytesPerSec = 48000 * 8; f.cbSize = 22;
        d->OutputFormat.Samples.wValidBitsPerSample = 32;
        d->OutputFormat.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
        static const GUID kFloat = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
        d->OutputFormat.SubFormat = kFloat;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE Initialize(UINT32 flags, UINT32 proc) {
        auto create = (XAudio2Create_t)HostSysProc("xaudio2_9.dll", "XAudio2Create");
        if (!create) { HLog("ERRO: xaudio2_9.dll indisponivel; jogo sem som"); return E_FAIL; }
        HRESULT hr = create(&x, flags & ~1u, proc);
        HLog("XAudio2 2.9: Initialize flags=0x%x proc=0x%x -> 0x%08lx", flags, proc, hr);
        return hr;
    }
    virtual HRESULT STDMETHODCALLTYPE RegisterForCallbacks(IXAudio2EngineCallback* c) { return x->RegisterForCallbacks(c); }
    virtual void STDMETHODCALLTYPE UnregisterForCallbacks(IXAudio2EngineCallback* c) { x->UnregisterForCallbacks(c); }
    virtual HRESULT STDMETHODCALLTYPE CreateSourceVoice(void** pp, const WAVEFORMATEX* fmt, UINT32 flags, float maxRatio, IXAudio2VoiceCallback* cb, const XAUDIO2_VOICE_SENDS* s, const XAUDIO2_EFFECT_CHAIN* e) {
        IXAudio2SourceVoice* v = nullptr;
        Sends c;
        HRESULT hr = x->CreateSourceVoice(&v, fmt, flags, maxRatio, cb, c.get(s), e);
        if (FAILED(hr)) { HLog("XAudio2: CreateSourceVoice fmt=%u canais=%u taxa=%lu -> 0x%08lx", fmt ? fmt->wFormatTag : 0, fmt ? fmt->nChannels : 0, fmt ? fmt->nSamplesPerSec : 0, hr); return hr; }
        *pp = new SourceP(v);
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE CreateSubmixVoice(void** pp, UINT32 ch, UINT32 rate, UINT32 flags, UINT32 stage, const XAUDIO2_VOICE_SENDS* s, const XAUDIO2_EFFECT_CHAIN* e) {
        IXAudio2SubmixVoice* v = nullptr;
        Sends c;
        HRESULT hr = x->CreateSubmixVoice(&v, ch, rate, flags, stage, c.get(s), e);
        if (FAILED(hr)) { HLog("XAudio2: CreateSubmixVoice -> 0x%08lx", hr); return hr; }
        *pp = new VoiceP(v);
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE CreateMasteringVoice(void** pp, UINT32 ch, UINT32 rate, UINT32 flags, UINT32, const XAUDIO2_EFFECT_CHAIN* e) {
        IXAudio2MasteringVoice* v = nullptr;
        HRESULT hr = x->CreateMasteringVoice(&v, ch, rate, flags, nullptr, e, AudioCategory_GameEffects);
        HLog("XAudio2: CreateMasteringVoice canais=%u taxa=%u -> 0x%08lx", ch, rate, hr);
        if (FAILED(hr)) return hr;
        if (HostFlagFile(L"mute.txt")) v->SetVolume(0.0f);
        *pp = new VoiceP(v);
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE StartEngine() { return x->StartEngine(); }
    virtual void STDMETHODCALLTYPE StopEngine() { x->StopEngine(); }
    virtual HRESULT STDMETHODCALLTYPE CommitChanges(UINT32 op) { return x->CommitChanges(op); }
    virtual void STDMETHODCALLTYPE GetPerformanceData(XAUDIO2_PERFORMANCE_DATA* p) { x->GetPerformanceData(p); }
    virtual void STDMETHODCALLTYPE SetDebugConfiguration(const XAUDIO2_DEBUG_CONFIGURATION*, void*) {}
};

static HRESULT WINAPI S_CoCreateInstance(REFCLSID c, IUnknown* outer, DWORD ctx, REFIID iid, void** pp) {
    if (c == kCLSID_XAudio27) { HLog("CoCreateInstance(XAudio2 2.7) -> shim"); *pp = static_cast<IUnknown*>(new XA27()); return S_OK; }
    if (c == kCLSID_Reverb27) {
        IUnknown* r = nullptr;
        auto create = (CreateAudioReverb_t)HostSysProc("xaudio2_9.dll", "CreateAudioReverb");
        HRESULT hr = create ? create(&r) : E_FAIL;
        HLog("CoCreateInstance(Reverb 2.7) -> 0x%08lx", hr);
        if (FAILED(hr)) return hr;
        *pp = r;
        return S_OK;
    }
    return CoCreateInstance(c, outer, ctx, iid, pp);
}

static HRESULT WINAPI S_CreateFX(REFCLSID, IUnknown** out) {
    FXMASTERINGLIMITER_PARAMETERS p{FXMASTERINGLIMITER_DEFAULT_RELEASE, FXMASTERINGLIMITER_DEFAULT_LOUDNESS};
    auto create = (CreateFX_t)HostSysProc("xaudio2_9.dll", "CreateFX");
    HRESULT hr = create ? create(__uuidof(FXMasteringLimiter), out, &p, sizeof(p)) : E_FAIL;
    HLog("CreateFX(limiter) -> 0x%08lx", hr);
    return hr;
}

const ShimEntry g_audioShims[] = {
    {"ole32.dll", "CoCreateInstance", (void*)&S_CoCreateInstance},
    {"xapofx1_5.dll", "CreateFX", (void*)&S_CreateFX},
    {nullptr, nullptr, nullptr},
};
