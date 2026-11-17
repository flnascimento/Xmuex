# Abre o launcher no PC, tira um print e clica de verdade em JOGAR (Shenmue I) via UI Automation. Saida: print em -Shot.
param([string]$Shot)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Drawing, System.Windows.Forms
Get-Process ShenmueGame, ShenmueLauncher -ErrorAction SilentlyContinue | Stop-Process -Force
$p = Get-AppxPackage ShenmueUwpLauncher
Start-Process "shell:AppsFolder\$($p.PackageFamilyName)!App"
Start-Sleep -Seconds 14
Write-Output "launcher vivo: $([bool](Get-Process ShenmueLauncher -ErrorAction SilentlyContinue))"
$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
$bmp.Save($Shot, [System.Drawing.Imaging.ImageFormat]::Png)
$root = [System.Windows.Automation.AutomationElement]::RootElement
$cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty, 'JOGAR')
$btn = $null
for ($i = 0; $i -lt 10 -and -not $btn; $i++) { $btn = $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $cond); if (-not $btn) { Start-Sleep -Seconds 1 } }
if (-not $btn) { Write-Output 'botao JOGAR nao encontrado'; exit 1 }
$btn.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
Write-Output 'clicou em JOGAR'
Start-Sleep -Seconds 14
Write-Output "jogo vivo: $([bool](Get-Process ShenmueGame -ErrorAction SilentlyContinue))"
Get-Process ShenmueGame, ShenmueLauncher -ErrorAction SilentlyContinue | Stop-Process -Force
