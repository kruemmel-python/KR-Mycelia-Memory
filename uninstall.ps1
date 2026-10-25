$ErrorActionPreference = 'Stop'
$Install = Join-Path $env:LOCALAPPDATA 'Programs\KRMyceliaMemory'
$Expected = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'Programs\KRMyceliaMemory'))
$Resolved = [IO.Path]::GetFullPath($Install)
if ($Resolved -ne $Expected -or [IO.Path]::GetFileName($Resolved) -ne 'KRMyceliaMemory') {
    throw "Unsicheres Deinstallationsziel abgewiesen: $Resolved"
}
$Link = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\KR Mycelia Memory Native.lnk'
foreach ($Name in @('KR_Mycelia_Memory','KR_Mycelia_Service','KR_Mycelia_MCP','KR_Mycelia_Import')) {
    Get-Process -Name $Name -ErrorAction SilentlyContinue | Stop-Process -Force
}
if (Test-Path -LiteralPath $Link) { Remove-Item -LiteralPath $Link -Force }
if (Test-Path -LiteralPath $Install) { Remove-Item -LiteralPath $Install -Recurse -Force }
Write-Host 'Anwendung entfernt. Persönliche Erinnerungen, Einstellungen und DPAPI-Schlüssel unter LocalAppData\KRMyceliaNativeMemory wurden nicht gelöscht.'
