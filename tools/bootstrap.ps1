# Executa elevado: Modo de Desenvolvedor + VS Build Tools (UWP/C++/SDK). Log em C:\ShenmueSetup\bootstrap.log
$ErrorActionPreference = 'Stop'
$dest = 'C:\ShenmueSetup'
New-Item -ItemType Directory -Force $dest | Out-Null
Start-Transcript -Path "$dest\bootstrap.log" -Force | Out-Null
$k = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock'
New-Item -Path $k -Force | Out-Null
Set-ItemProperty $k -Name AllowDevelopmentWithoutDevLicense -Value 1 -Type DWord
Set-ItemProperty $k -Name AllowAllTrustedApps -Value 1 -Type DWord
$exe = "$dest\vs_BuildTools.exe"
if (-not (Test-Path $exe)) { Invoke-WebRequest https://aka.ms/vs/17/release/vs_BuildTools.exe -OutFile $exe }
$args = @('--add','Microsoft.VisualStudio.Workload.UniversalBuildTools','--add','Microsoft.VisualStudio.Workload.VCTools',
  '--add','Microsoft.VisualStudio.ComponentGroup.UWP.VC.BuildTools','--add','Microsoft.VisualStudio.Component.Windows11SDK.26100',
  '--includeRecommended','--quiet','--norestart','--wait')
$p = Start-Process $exe -ArgumentList $args -Wait -PassThru
"EXITCODE=$($p.ExitCode)" | Out-File "$dest\done.txt"
Stop-Transcript | Out-Null
