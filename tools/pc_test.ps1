# Testa o runtime no sandbox UWP deste PC, sem Xbox. Usa os dados do jogo da instalacao Steam local (junction, sem copiar 13 GB).
# Uso: pc_test.ps1 [-Game 1|2] [-Seconds 40] [-SkipInstall] [-Autopad "linhas"] [-Strict]
# -Strict bloqueia as DLLs do sistema que podem nao existir no console.
param([int]$Game = 1, [int]$Seconds = 40, [switch]$SkipInstall, [string]$Autopad = '', [switch]$Strict, [int]$DumpEvery = 300, [switch]$FileLog, [switch]$ImportPcSaves)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$steam = 'C:\Program Files (x86)\Steam\steamapps\common\SMLaunch'
$pkg = Get-ChildItem "$root\stages\01-launcher\out" -Directory -Filter 'ShenmueLauncher_*' | Sort-Object Name | Select-Object -Last 1
$layout = "$root\stages\01-launcher\out\layout"

Get-Process ShenmueGame, ShenmueLauncher -ErrorAction SilentlyContinue | Stop-Process -Force
if (-not $SkipInstall) {
    foreach ($d in Get-ChildItem "$($pkg.FullName)\Dependencies\x64\*.appx") {
        try { Add-AppxPackage -Path $d.FullName -ErrorAction Stop } catch { if ($_.Exception.Message -notmatch '0x80073D06|higher version') { Write-Output "dep $($d.Name): $($_.Exception.Message)" } }
    }
    Add-AppxPackage -Register "$layout\AppxManifest.xml" -ForceApplicationShutdown
}
$p = Get-AppxPackage ShenmueUwpLauncher
$local = "$env:LOCALAPPDATA\Packages\$($p.PackageFamilyName)\LocalState"
foreach ($sub in 'sm1', 'sm2') {
    $dst = "$local\game\$sub"
    New-Item -ItemType Directory -Force $dst | Out-Null
    foreach ($f in Get-ChildItem "$steam\$sub" -File) {
        $t = "$dst\$($f.Name)"
        if (-not (Test-Path $t) -or (Get-Item $t).Length -ne $f.Length) { Copy-Item $f.FullName $t -Force }
    }
    if (-not (Test-Path "$dst\archives")) {
        # archives\dx11\data fica de verdade (hardlink por arquivo, sem copiar): é onde mods de
        # textura (Streamer2k) instalam/removem arquivos; nunca mexe na pasta real da Steam. O
        # resto da árvore continua por junction (não muda com o jogo rodando).
        New-Item -ItemType Directory -Force "$dst\archives\dx11\data" | Out-Null
        foreach ($d in Get-ChildItem "$steam\$sub\archives" -Directory) {
            if ($d.Name -ne 'dx11') { cmd /c mklink /J "$dst\archives\$($d.Name)" "$($d.FullName)" | Out-Null }
        }
        foreach ($d in Get-ChildItem "$steam\$sub\archives\dx11" -Directory) {
            if ($d.Name -ne 'data') { cmd /c mklink /J "$dst\archives\dx11\$($d.Name)" "$($d.FullName)" | Out-Null }
        }
        foreach ($f in Get-ChildItem "$steam\$sub\archives\dx11\data" -File) {
            New-Item -ItemType HardLink -Path "$dst\archives\dx11\data\$($f.Name)" -Target $f.FullName | Out-Null
        }
    }
}
"$DumpEvery" | Out-File "$local\framedump.txt" -Encoding ascii
'1' | Out-File "$local\mute.txt" -Encoding ascii
'1' | Out-File "$local\watchdog.txt" -Encoding ascii
if ($FileLog) { '1' | Out-File "$local\filelog.txt" -Encoding ascii } else { Remove-Item "$local\filelog.txt" -ErrorAction SilentlyContinue }
# -ImportPcSaves: copia os saves da Steam Cloud deste PC (userdata\<id>\758330\remote) para LocalState\cloud
if ($ImportPcSaves) {
    $remote = Get-ChildItem 'C:\Program Files (x86)\Steam\userdata\*\758330\remote' -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($remote) { New-Item -ItemType Directory -Force "$local\cloud" | Out-Null; Copy-Item "$($remote.FullName)\*" "$local\cloud" -Recurse -Force; Write-Output "saves do PC copiados de $($remote.FullName)" }
}
if ($Strict) { '1' | Out-File "$local\strict.txt" -Encoding ascii } else { Remove-Item "$local\strict.txt" -ErrorAction SilentlyContinue }
if ($Autopad) { $Autopad | Out-File "$local\autopad.txt" -Encoding ascii } else { Remove-Item "$local\autopad.txt" -ErrorAction SilentlyContinue }
Remove-Item "$local\host.log" -ErrorAction SilentlyContinue
Remove-Item "$local\frames" -Recurse -Force -ErrorAction SilentlyContinue

# A primeira ativacao logo apos registrar o pacote costuma falhar no PC; tenta de novo.
try { Start-Process "shenmue-uwp-run:$Game" } catch { Start-Sleep -Seconds 3; Start-Process "shenmue-uwp-run:$Game" }
$t0 = Get-Date
while (((Get-Date) - $t0).TotalSeconds -lt $Seconds) {
    Start-Sleep -Seconds 2
    if (((Get-Date) - $t0).TotalSeconds -gt 8 -and -not (Get-Process ShenmueGame -ErrorAction SilentlyContinue)) { Write-Output 'PROCESSO ENCERROU'; break }
}
$alive = [bool](Get-Process ShenmueGame -ErrorAction SilentlyContinue)
Write-Output "vivo ao final: $alive"
Get-Process ShenmueGame -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output "LOCAL=$local"
if (Test-Path "$local\host.log") { Copy-Item "$local\host.log" "$root\stages\03-pe-loader\out\host_last.log" -Force; Write-Output 'log copiado' } else { Write-Output 'SEM host.log' }
if (Test-Path "$local\frames") { Get-ChildItem "$local\frames" | ForEach-Object { Write-Output "frame: $($_.Name) $($_.Length)" } }
