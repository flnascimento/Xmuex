# Auditoria de imports

SDK: `C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64`
Permitido = presente em WindowsApp.lib, d3d11.lib, dxgi.lib, xinputuap.lib, xaudio2.lib, d3dcompiler.lib.


## SharpDX.XInput.dll
- sha1 `30094ccfbe0688b8e2851d32e206974c06861764` · machine `0x14c` · subsystem 3 · ASLR/reloc sim · TLS não · tamanho imagem 40 KiB

| DLL | total | UWP ok | precisa shim | observação |
|---|---|---|---|---|
| mscoree.dll | 1 | 0 | 1 |  |

Funções sem equivalente UWP direto:
- **mscoree.dll**: _CorDllMain

## SharpDX.dll
- sha1 `c2091f69192903676ed6b181bbf8346b819c43a2` · machine `0x14c` · subsystem 3 · ASLR/reloc sim · TLS não · tamanho imagem 288 KiB

| DLL | total | UWP ok | precisa shim | observação |
|---|---|---|---|---|
| mscoree.dll | 1 | 0 | 1 |  |

Funções sem equivalente UWP direto:
- **mscoree.dll**: _CorDllMain

## SteamLauncher.exe
- sha1 `5195049134dd3072bf9271d4f0533fbcff223fb4` · machine `0x14c` · subsystem 2 · ASLR/reloc sim · TLS não · tamanho imagem 1152 KiB

| DLL | total | UWP ok | precisa shim | observação |
|---|---|---|---|---|
| mscoree.dll | 1 | 0 | 1 |  |

Funções sem equivalente UWP direto:
- **mscoree.dll**: _CorExeMain

## sm1\GameHangon_Winx64_Release.dll
- sha1 `8e2a0f9a5e7ab88aef0660866fb32ddac3c1ecd7` · machine `0x8664` · subsystem 2 · ASLR/reloc sim · TLS sim · tamanho imagem 1508 KiB

| DLL | total | UWP ok | precisa shim | observação |
|---|---|---|---|---|
| api-ms-win-crt-math-l1-1-0.dll | 5 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-runtime-l1-1-0.dll | 15 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-stdio-l1-1-0.dll | 2 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-string-l1-1-0.dll | 1 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| kernel32.dll | 30 | 30 | 0 |  |
| msvcp140.dll | 2 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| vcruntime140.dll | 12 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |

## sm1\Shenmue.exe
- sha1 `8d78635639af929045e97ad1ad0f1d9e11a5073b` · machine `0x8664` · subsystem 2 · ASLR/reloc sim · TLS sim · tamanho imagem 163948 KiB

| DLL | total | UWP ok | precisa shim | observação |
|---|---|---|---|---|
| advapi32.dll | 12 | 12 | 0 |  |
| api-ms-win-crt-convert-l1-1-0.dll | 7 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-environment-l1-1-0.dll | 1 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-filesystem-l1-1-0.dll | 5 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-heap-l1-1-0.dll | 8 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-locale-l1-1-0.dll | 1 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-math-l1-1-0.dll | 27 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-runtime-l1-1-0.dll | 27 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-stdio-l1-1-0.dll | 32 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-string-l1-1-0.dll | 23 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-time-l1-1-0.dll | 6 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-utility-l1-1-0.dll | 1 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| d3d11.dll | 1 | 1 | 0 |  |
| d3dcompiler_43.dll | 1 | 0 | 1 | DLL legada DirectX SDK: substituir por equivalente UWP |
| dbghelp.dll | 3 | 0 | 3 |  |
| dwmapi.dll | 1 | 0 | 1 |  |
| dxgi.dll | 1 | 1 | 0 |  |
| gamehangon_winx64_release.dll | 4 | – | – | DLL do próprio jogo (analisar/mapear junto) |
| imm32.dll | 2 | 0 | 2 |  |
| kernel32.dll | 76 | 76 | 0 |  |
| msvcp140.dll | 77 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| normaliz.dll | 1 | 1 | 0 |  |
| ole32.dll | 3 | 3 | 0 |  |
| oo2core_4_win64.dll | 5 | – | – | DLL do próprio jogo (analisar/mapear junto) |
| oo2ext_4_win64.dll | 2 | – | – | DLL do próprio jogo (analisar/mapear junto) |
| shell32.dll | 1 | 1 | 0 |  |
| steam_api64.dll | 12 | – | – | DLL do próprio jogo (analisar/mapear junto) |
| user32.dll | 44 | 0 | 44 |  |
| vcruntime140.dll | 16 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| wldap32.dll | 16 | 0 | 16 |  |
| ws2_32.dll | 29 | 29 | 0 |  |
| xapofx1_5.dll | 1 | 0 | 1 | DLL legada DirectX SDK: substituir por equivalente UWP |
| xinput1_3.dll | 1 | 0 | 1 | DLL legada DirectX SDK: substituir por equivalente UWP |

Funções sem equivalente UWP direto:
- **d3dcompiler_43.dll**: D3DReflect
- **dbghelp.dll**: MiniDumpWriteDump, SymFromAddr, SymInitialize
- **dwmapi.dll**: DwmGetWindowAttribute
- **imm32.dll**: ImmGetContext, ImmSetCompositionWindow
- **user32.dll**: AdjustWindowRect, BeginPaint, ClientToScreen, ClipCursor, CloseClipboard, CreateWindowExA, DefWindowProcA, DispatchMessageA, EmptyClipboard, EndPaint, EnumDisplayDevicesA, FillRect, GetClientRect, GetClipboardData, GetForegroundWindow, GetKeyNameTextA, GetKeyState, GetKeyboardLayout, GetMonitorInfoA, GetRawInputData, GetWindowLongA, GetWindowRect, LoadCursorA, MapVirtualKeyA, MapVirtualKeyExA, MapWindowPoints, OpenClipboard, PeekMessageA, PostMessageA, PostQuitMessage, RegisterClassA, RegisterRawInputDevices, RegisterWindowMessageA, ReleaseCapture, SetCapture, SetClipboardData, SetCursor, SetCursorPos, SetTimer, SetWindowLongA, SetWindowPos, ShowCursor, ShowWindow, TranslateMessage
- **wldap32.dll**: #ord143, #ord200, #ord211, #ord22, #ord26, #ord27, #ord30, #ord301, #ord32, #ord33, #ord35, #ord41, #ord46, #ord50, #ord60, #ord79
- **xapofx1_5.dll**: CreateFX
- **xinput1_3.dll**: #ord2

## sm1\TootleDLL_2015_64.dll
- sha1 `74486f1b65d156af079f2dfd1d409dc2f68de279` · machine `0x8664` · subsystem 2 · ASLR/reloc sim · TLS sim · tamanho imagem 244 KiB

| DLL | total | UWP ok | precisa shim | observação |
|---|---|---|---|---|
| api-ms-win-crt-heap-l1-1-0.dll | 5 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-math-l1-1-0.dll | 7 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-runtime-l1-1-0.dll | 13 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-stdio-l1-1-0.dll | 12 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-utility-l1-1-0.dll | 3 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| d3d9.dll | 1 | 0 | 1 |  |
| d3dx9_43.dll | 7 | 0 | 7 |  |
| kernel32.dll | 18 | 18 | 0 |  |
| msvcp140.dll | 3 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| user32.dll | 25 | 0 | 25 |  |
| vcruntime140.dll | 11 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |

Funções sem equivalente UWP direto:
- **d3d9.dll**: Direct3DCreate9
- **d3dx9_43.dll**: D3DXMatrixLookAtRH, D3DXMatrixMultiply, D3DXMatrixOrthoOffCenterRH, D3DXMatrixScaling, D3DXMatrixTranslation, D3DXOptimizeFaces, D3DXVec3Transform
- **user32.dll**: AdjustWindowRect, CreateWindowExA, DefWindowProcA, DestroyWindow, DispatchMessageA, GetClientRect, GetDesktopWindow, GetMessageA, GetWindowLongA, GetWindowRect, InvalidateRect, IsDialogMessageA, LoadCursorA, MoveWindow, PeekMessageA, PostQuitMessage, RegisterClassExA, ReleaseCapture, SetCapture, SetWindowLongA, ShowWindow, TranslateMessage, UnregisterClassA, UpdateWindow, ValidateRect

## sm1\libfbxsdk.dll
- sha1 `1a82dddb447b02608be4fc90f08d691e62eef728` · machine `0x8664` · subsystem 3 · ASLR/reloc sim · TLS sim · tamanho imagem 7900 KiB

| DLL | total | UWP ok | precisa shim | observação |
|---|---|---|---|---|
| advapi32.dll | 7 | 7 | 0 |  |
| kernel32.dll | 141 | 141 | 0 |  |

## sm1\oo2core_4_win64.dll
- sha1 `0a8e128fd4355bccfde19cba3d4b4461eaa0cadf` · machine `0x8664` · subsystem 2 · ASLR/reloc sim · TLS não · tamanho imagem 884 KiB

| DLL | total | UWP ok | precisa shim | observação |
|---|---|---|---|---|
| kernel32.dll | 66 | 66 | 0 |  |

## sm1\oo2ext_4_win64.dll
- sha1 `76394dac3051d45675eb26da3b9a4d354dd6a8c2` · machine `0x8664` · subsystem 2 · ASLR/reloc sim · TLS sim · tamanho imagem 1128 KiB

| DLL | total | UWP ok | precisa shim | observação |
|---|---|---|---|---|
| dbghelp.dll | 5 | 0 | 5 |  |
| kernel32.dll | 121 | 120 | 1 |  |
| user32.dll | 6 | 0 | 6 |  |
| winmm.dll | 1 | 1 | 0 |  |

Funções sem equivalente UWP direto:
- **dbghelp.dll**: SymCleanup, SymGetLineFromAddr64, SymGetSymFromAddr64, SymInitialize, SymSetOptions
- **kernel32.dll**: K32EnumProcessModules
- **user32.dll**: IsWindowVisible, MessageBoxA, MessageBoxIndirectA, SetCapture, SetFocus, ShowWindow

## sm1\renderdoc.dll
- sha1 `cac9fa237aa41b2dcd3456655e988cd16d95610a` · machine `0x8664` · subsystem 2 · ASLR/reloc sim · TLS não · tamanho imagem 10888 KiB

| DLL | total | UWP ok | precisa shim | observação |
|---|---|---|---|---|
| advapi32.dll | 1 | 1 | 0 |  |
| comdlg32.dll | 1 | 0 | 1 |  |
| gdi32.dll | 8 | 0 | 8 |  |
| kernel32.dll | 101 | 97 | 4 |  |
| msvcp100.dll | 44 | 0 | 44 |  |
| msvcr100.dll | 115 | 0 | 115 |  |
| ole32.dll | 1 | 1 | 0 |  |
| psapi.dll | 1 | 1 | 0 |  |
| shell32.dll | 2 | 0 | 2 |  |
| user32.dll | 16 | 0 | 16 |  |
| ws2_32.dll | 20 | 20 | 0 |  |

Funções sem equivalente UWP direto:
- **comdlg32.dll**: GetOpenFileNameW
- **gdi32.dll**: ChoosePixelFormat, CreateCompatibleDC, CreateDIBSection, DeleteObject, DescribePixelFormat, GetPixelFormat, SetPixelFormat, SwapBuffers
- **kernel32.dll**: Module32First, Module32FirstW, Module32Next, Module32NextW
- **msvcp100.dll**: ??0?$basic_ios@DU?$char_traits@D@std@@@std@@IEAA@XZ, ??0?$basic_istream@DU?$char_traits@D@std@@@std@@QEAA@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@_N@Z, ??0?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAA@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@_N@Z, ??0?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAA@XZ, ??0_Lockit@std@@QEAA@H@Z, ??1?$basic_ios@DU?$char_traits@D@std@@@std@@UEAA@XZ, ??1?$basic_istream@DU?$char_traits@D@std@@@std@@UEAA@XZ, ??1?$basic_ostream@DU?$char_traits@D@std@@@std@@UEAA@XZ, ??1?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAA@XZ, ??1_Container_base12@std@@QEAA@XZ, ??1_Lockit@std@@QEAA@XZ, ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@H@Z, ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@I@Z, ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@_N@Z, ??_7?$basic_istream@DU?$char_traits@D@std@@@std@@6B@, ??_7?$basic_ostream@DU?$char_traits@D@std@@@std@@6B@, ?_BADOFF@std@@3_JB, ?_Decref@facet@locale@std@@QEAAPEAV123@XZ, ?_Ipfx@?$basic_istream@DU?$char_traits@D@std@@@std@@QEAA_N_N@Z, ?_Lock@?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAAXXZ, ?_Orphan_all@_Container_base0@std@@QEAAXXZ, ?_Osfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAXXZ, ?_Pninc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAPEADXZ, ?_Swap_all@_Container_base0@std@@QEAAXAEAU12@@Z, ?_Unlock@?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAAXXZ, ?_Xfunc@tr1@std@@YAXXZ, ?_Xlength_error@std@@YAXPEBD@Z, ?_Xout_of_range@std@@YAXPEBD@Z, ?flush@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@XZ, ?imbue@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAXAEBVlocale@2@@Z, ?sbumpc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHXZ, ?setbuf@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAPEAV12@PEAD_J@Z, ?setstate@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAXH_N@Z, ?sgetc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHXZ, ?showmanyc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JXZ, ?snextc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHXZ, ?sputc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHD@Z, ?sputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA_JPEBD_J@Z, ?sync@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHXZ, ?uflow@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHXZ, ?uncaught_exception@std@@YA_NXZ, ?widen@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBADD@Z, ?xsgetn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JPEAD_J@Z, ?xsputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JPEBD_J@Z
- **msvcr100.dll**: ??0exception@std@@QEAA@AEBQEBD@Z, ??0exception@std@@QEAA@AEBV01@@Z, ??1exception@std@@UEAA@XZ, ??2@YAPEAX_K@Z, ??3@YAXPEAX@Z, ??_U@YAPEAX_K@Z, ??_V@YAXPEAX@Z, ?_type_info_dtor_internal_method@type_info@@QEAAXXZ, ?terminate@@YAXXZ, ?what@exception@std@@UEBAPEBDXZ, _CxxThrowException, __C_specific_handler, __CppXcptFilter, __CxxFrameHandler3, __clean_type_info_names_internal, __crt_debugger_hook, __dllonexit, __iob_func, _amsg_exit, _encoded_null, _errno, _finite, _fseeki64, _ftelli64, _initterm, _initterm_e, _invalid_parameter_noinfo, _isnan, _itoa_s, _localtime64_s, _lock, _malloc_crt, _onexit, _purecall, _set_invalid_parameter_handler, _set_purecall_handler, _setjmp, _snwprintf_s, _strdup, _stricmp, _strtoi64, _strtoui64, _time64, _unlock, _vsnprintf_s, _wfopen_s, acos, acosf, asin, atan, atan2, atof, atoi, ceil, ceilf, cos, cosf, exit, exp, fclose, feof, fgetc, floor, floorf, fprintf, fread, free, frexp, fseek, ftell, fwrite, isalpha, isdigit, ldexp, ldiv, log, logf, longjmp, malloc, memchr, memcmp, memcpy, memmove, memset, pow, powf, printf, realloc, sin, sinf, sprintf, sprintf_s, sqrt, sqrtf, sscanf_s, strchr, strncmp, strnlen, strstr, strtod, strtol, strtoul, swprintf_s, tan, tanf, tolower, towlower, vswprintf_s, wcscat_s, wcschr, wcscpy_s, wcsftime, wcsncpy_s, wcsrchr, wcsstr
- **shell32.dll**: SHCreateDirectoryExW, SHGetKnownFolderPath
- **user32.dll**: CreateWindowExW, DefWindowProcW, DestroyWindow, GetAsyncKeyState, GetClientRect, GetDC, GetForegroundWindow, GetParent, IsWindowVisible, LoadCursorW, MessageBoxA, MessageBoxW, RegisterClassExW, ReleaseDC, WindowFromDC, wsprintfW

## sm1\steam_api64.dll
- sha1 `e77433094c56433685a68a4436e81b76c3b5e1f5` · machine `0x8664` · subsystem 2 · ASLR/reloc sim · TLS não · tamanho imagem 260 KiB

| DLL | total | UWP ok | precisa shim | observação |
|---|---|---|---|---|
| advapi32.dll | 3 | 3 | 0 |  |
| kernel32.dll | 79 | 79 | 0 |  |
| shell32.dll | 1 | 0 | 1 |  |

Funções sem equivalente UWP direto:
- **shell32.dll**: ShellExecuteW

## sm1\zlibwapi.dll
- sha1 `270ba087f76b459c3650a0f12d72a7b905d5a918` · machine `0x8664` · subsystem 2 · ASLR/reloc sim · TLS não · tamanho imagem 132 KiB

| DLL | total | UWP ok | precisa shim | observação |
|---|---|---|---|---|
| api-ms-win-crt-convert-l1-1-0.dll | 1 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-heap-l1-1-0.dll | 2 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-runtime-l1-1-0.dll | 14 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-stdio-l1-1-0.dll | 16 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-time-l1-1-0.dll | 1 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-utility-l1-1-0.dll | 2 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| kernel32.dll | 22 | 22 | 0 |  |
| vcruntime140.dll | 7 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |

## sm2\Shenmue2.exe
- sha1 `1005fbf609c5505df58b97c82489f856051c880b` · machine `0x8664` · subsystem 2 · ASLR/reloc sim · TLS sim · tamanho imagem 133748 KiB

| DLL | total | UWP ok | precisa shim | observação |
|---|---|---|---|---|
| advapi32.dll | 12 | 12 | 0 |  |
| api-ms-win-crt-convert-l1-1-0.dll | 8 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-environment-l1-1-0.dll | 1 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-filesystem-l1-1-0.dll | 5 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-heap-l1-1-0.dll | 8 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-locale-l1-1-0.dll | 1 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-math-l1-1-0.dll | 27 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-runtime-l1-1-0.dll | 27 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-stdio-l1-1-0.dll | 32 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-string-l1-1-0.dll | 23 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-time-l1-1-0.dll | 6 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| api-ms-win-crt-utility-l1-1-0.dll | 1 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| d3d11.dll | 1 | 1 | 0 |  |
| d3dcompiler_43.dll | 1 | 0 | 1 | DLL legada DirectX SDK: substituir por equivalente UWP |
| dbghelp.dll | 3 | 0 | 3 |  |
| dwmapi.dll | 1 | 0 | 1 |  |
| dxgi.dll | 1 | 1 | 0 |  |
| gamehangon_winx64_release.dll | 4 | – | – | DLL do próprio jogo (analisar/mapear junto) |
| imm32.dll | 2 | 0 | 2 |  |
| kernel32.dll | 78 | 78 | 0 |  |
| msvcp140.dll | 77 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| normaliz.dll | 1 | 1 | 0 |  |
| ole32.dll | 3 | 3 | 0 |  |
| oo2core_4_win64.dll | 5 | – | – | DLL do próprio jogo (analisar/mapear junto) |
| oo2ext_4_win64.dll | 2 | – | – | DLL do próprio jogo (analisar/mapear junto) |
| shell32.dll | 1 | 1 | 0 |  |
| steam_api64.dll | 12 | – | – | DLL do próprio jogo (analisar/mapear junto) |
| user32.dll | 44 | 0 | 44 |  |
| vcruntime140.dll | 16 | – | – | CRT: empacotar VC runtime UWP / fornecer via host |
| wldap32.dll | 16 | 0 | 16 |  |
| ws2_32.dll | 29 | 29 | 0 |  |
| xapofx1_5.dll | 1 | 0 | 1 | DLL legada DirectX SDK: substituir por equivalente UWP |
| xinput1_3.dll | 1 | 0 | 1 | DLL legada DirectX SDK: substituir por equivalente UWP |

Funções sem equivalente UWP direto:
- **d3dcompiler_43.dll**: D3DReflect
- **dbghelp.dll**: MiniDumpWriteDump, SymFromAddr, SymInitialize
- **dwmapi.dll**: DwmGetWindowAttribute
- **imm32.dll**: ImmGetContext, ImmSetCompositionWindow
- **user32.dll**: AdjustWindowRect, BeginPaint, ClientToScreen, ClipCursor, CloseClipboard, CreateWindowExA, DefWindowProcA, DispatchMessageA, EmptyClipboard, EndPaint, EnumDisplayDevicesA, FillRect, GetClientRect, GetClipboardData, GetForegroundWindow, GetKeyNameTextA, GetKeyState, GetKeyboardLayout, GetMonitorInfoA, GetRawInputData, GetWindowLongA, GetWindowRect, LoadCursorA, MapVirtualKeyA, MapVirtualKeyExA, MapWindowPoints, OpenClipboard, PeekMessageA, PostMessageA, PostQuitMessage, RegisterClassA, RegisterRawInputDevices, RegisterWindowMessageA, ReleaseCapture, SetCapture, SetClipboardData, SetCursor, SetCursorPos, SetTimer, SetWindowLongA, SetWindowPos, ShowCursor, ShowWindow, TranslateMessage
- **wldap32.dll**: #ord143, #ord200, #ord211, #ord22, #ord26, #ord27, #ord30, #ord301, #ord32, #ord33, #ord35, #ord41, #ord46, #ord50, #ord60, #ord79
- **xapofx1_5.dll**: CreateFX
- **xinput1_3.dll**: #ord2
