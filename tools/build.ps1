# Compila tudo e gera o pacote final com os dois apps (launcher C# + ShenmueGame nativo). Imprime so SUCESSO/FALHA.
$ms = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
$sdk = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64'
$root = Split-Path $PSScriptRoot
$cert = "$root\certs\dev.pfx"
if (-not (Test-Path $cert)) { "BUILD: FALHA - rode tools\gen_cert.ps1 antes (gera $cert)"; exit 1 }
$log = "$root\build.log"
& "$root\stages\03-pe-loader\build.ps1" exe
if ($LASTEXITCODE -ne 0) { exit 1 }
& $ms "$root\stages\01-launcher\src\ShenmueLauncher.csproj" /restore /nologo /v:m /p:Platform=x64 /p:Configuration=Debug /p:AppxPackageDir="$root\stages\01-launcher\out\" > $log 2>&1
if ($LASTEXITCODE -ne 0) { "BUILD: FALHA (veja build.log)"; Select-String -Path $log -Pattern ' error ' | Select-Object -First 5 | ForEach-Object { $_.Line -replace '\[C:.*','' }; exit 1 }

# O build CoreCLR so aceita apps gerenciados no manifesto; o app nativo do jogo entra aqui, no pacote final.
$manifest = [xml](Get-Content "$root\stages\01-launcher\src\Package.appxmanifest" -Encoding UTF8)
$ver = $manifest.Package.Identity.Version
$pkgDir = "$root\stages\01-launcher\out\ShenmueLauncher_${ver}_x64_Debug_Test"
$msix = "$pkgDir\ShenmueLauncher_${ver}_x64_Debug.msix"
$layout = "$root\stages\01-launcher\out\layout"
if (Test-Path $layout) { Remove-Item $layout -Recurse -Force }
& "$sdk\makeappx.exe" unpack /p $msix /d $layout /o >> $log 2>&1
Remove-Item -LiteralPath "$layout\AppxSignature.p7x", "$layout\AppxBlockMap.xml", "$layout\AppxMetadata", "$layout\[Content_Types].xml" -Recurse -Force -ErrorAction SilentlyContinue
$text = [IO.File]::ReadAllText("$layout\AppxManifest.xml")
$fragment = [IO.File]::ReadAllText("$root\stages\01-launcher\src\GameApp.fragment.xml")
$text = $text.Replace('</Applications>', $fragment + '</Applications>')
[IO.File]::WriteAllText("$layout\AppxManifest.xml", $text)
# Mods embutidos (gerados por tools/import_mods.py; fora do git)
$payload = "$root\stages\05-mods\payload"
if (Test-Path $payload) { Copy-Item $payload "$layout\mods" -Recurse -Force }
$final = "$root\stages\01-launcher\out\Shenmue_$ver.msix"
& "$sdk\makeappx.exe" pack /d $layout /p $final /o >> $log 2>&1
if ($LASTEXITCODE -ne 0) { "BUILD: FALHA no makeappx (veja build.log)"; exit 1 }
& "$sdk\signtool.exe" sign /fd SHA256 /a /f "$cert" /p dev $final >> $log 2>&1
if ($LASTEXITCODE -ne 0) { "BUILD: FALHA no signtool (veja build.log)"; exit 1 }
"BUILD: SUCESSO -> $final"
exit 0
