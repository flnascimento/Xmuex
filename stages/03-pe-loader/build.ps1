# Compila o runtime nativo. Uso: build.ps1 [test|exe]. "exe" gera ShenmueGame.exe + ShTls*.dll e copia para o launcher.
param([string]$Mode = 'exe')
$vcall = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat'
$here = $PSScriptRoot
$out = "$here\out"
New-Item -ItemType Directory -Force $out | Out-Null
$log = "$out\build_$Mode.log"
if ($Mode -eq 'test') {
  $cmd = "`"$vcall`" x64 >nul && cd /d `"$out`" && cl /nologo /std:c++17 /EHsc /O2 /W3 /Fe:pe_map_test.exe `"$here\test\pe_map_test.cpp`" `"$here\src\pe_loader.cpp`" kernel32.lib"
  cmd /c $cmd *> $log
  $ok = $LASTEXITCODE -eq 0
} else {
  $ok = $true
  Remove-Item $log -ErrorAction SilentlyContinue
  foreach ($n in 'A','B','C') {
    $c2 = "`"$vcall`" x64 store 10.0.26100.0 >nul && cd /d `"$out`" && cl /nologo /O2 /MD /LD /DSLOT_FN=SlotInfo$n /Fe:ShTls$n.dll `"$here\src\tls_slot.cpp`" /link /APPCONTAINER WindowsApp.lib"
    cmd /c $c2 *>> $log
    if ($LASTEXITCODE -ne 0) { $ok = $false }
  }
  $src = (Get-ChildItem "$here\src\*.cpp" | Where-Object { $_.Name -ne 'tls_slot.cpp' } | ForEach-Object { "`"$($_.FullName)`"" }) -join ' '
  $flags = '/nologo /std:c++17 /EHsc /O2 /W3 /MD /Zi /DHOST_UWP /DUNICODE /D_UNICODE /bigobj /wd4100 /wd4996 /Zc:threadSafeInit-'
  $cmd = "`"$vcall`" x64 store 10.0.26100.0 >nul && cd /d `"$out`" && cl $flags /Fe:ShenmueGame.exe $src /link /APPCONTAINER /SUBSYSTEM:WINDOWS /DEBUG /OPT:REF /BASE:0x7FF600000000 WindowsApp.lib d3d11.lib dxgi.lib d3dcompiler.lib ShTlsA.lib ShTlsB.lib ShTlsC.lib"
  if ($ok) { cmd /c $cmd *>> $log; $ok = $LASTEXITCODE -eq 0 }
  if ($ok) {
    $dest = (Resolve-Path "$here\..\01-launcher\src").Path + '\native'
    New-Item -ItemType Directory -Force $dest | Out-Null
    Remove-Item "$dest\ShenmueHost.dll", "$dest\ShTlsD.dll" -ErrorAction SilentlyContinue
    Copy-Item "$out\ShenmueGame.exe", "$out\ShTlsA.dll", "$out\ShTlsB.dll", "$out\ShTlsC.dll" $dest -Force
  }
}
if ($ok) { "BUILD($Mode): SUCESSO" } else { "BUILD($Mode): FALHA"; Select-String -Path $log -Pattern ' error |fatal error|LNK[0-9]+' | Select-Object -First 25 | ForEach-Object { $_.Line } }
exit $(if ($ok) { 0 } else { 1 })
