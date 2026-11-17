# Gera o certificado de desenvolvimento usado para assinar o pacote (uma vez, por maquina).
# Nunca commitado (certs/ esta no .gitignore): cada pessoa que compilar gera o seu.
$root = Split-Path $PSScriptRoot
$certDir = "$root\certs"
New-Item -ItemType Directory -Force $certDir | Out-Null
$pfx = "$certDir\dev.pfx"
if (Test-Path $pfx) { "ja existe: $pfx"; exit 0 }

$cert = New-SelfSignedCertificate -Type Custom -Subject "CN=ShenmueUwp" -KeyUsage DigitalSignature `
    -FriendlyName "Xmuex Dev Cert" -CertStoreLocation "Cert:\CurrentUser\My" `
    -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
$pwd = ConvertTo-SecureString -String "dev" -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $pwd | Out-Null
Remove-Item "Cert:\CurrentUser\My\$($cert.Thumbprint)"
"certificado gerado em $pfx (assunto deve bater com Publisher do Package.appxmanifest: CN=ShenmueUwp)"
