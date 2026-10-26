$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Build = Join-Path $Root 'KR_BUILD\cmake_build'
$ReleasePayload = Join-Path $Root 'bin'
if (Test-Path -LiteralPath (Join-Path $ReleasePayload 'KR_Mycelia_Memory.exe') -PathType Leaf) {
    $Build = $ReleasePayload
}
$Install = Join-Path $env:LOCALAPPDATA 'Programs\KRMyceliaMemory'
$Data = Join-Path $env:LOCALAPPDATA 'KRMyceliaNativeMemory'
$StartMenu = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs'
$Link = Join-Path $StartMenu 'KR Mycelia Memory Native.lnk'
$Programs = @('KR_Mycelia_Memory.exe','KR_Mycelia_Service.exe','KR_Mycelia_MCP.exe','KR_Mycelia_Import.exe')
$RuntimeNames = @('zlib1.dll','libgcc_s_seh-1.dll','libwinpthread-1.dll')
$DocumentationNames = @('LICENSE','README.md','CHANGELOG.md')
$ToolchainBin = Join-Path (Split-Path -Parent $Root) 'KR-Compiler\KR-Compiler-Windows-x64-v4.3.1\toolchain\bin'
$RuntimeSources = @{}
$ManifestName = 'install-manifest.json'

foreach ($Name in $Programs) {
    $Source = Join-Path $Build $Name
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Aktueller Release-Build fehlt: $Source"
    }
}
foreach ($Name in $RuntimeNames) {
    $Source = Join-Path $Build $Name
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        $Source = Join-Path $ToolchainBin $Name
    }
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Erforderliche Laufzeitbibliothek fehlt: $Name"
    }
    $RuntimeSources[$Name] = $Source
}
$DocumentationSources = @{}
foreach ($Name in $DocumentationNames) {
    $Source = Join-Path $Root $Name
    if (Test-Path -LiteralPath $Source -PathType Leaf) {
        $DocumentationSources[$Name] = $Source
    }
}
$PayloadNames = $Programs + $RuntimeNames + @($DocumentationSources.Keys)

# A running installed executable cannot be replaced reliably. First request a
# normal GUI close where possible, then stop only the four product processes.
foreach ($Name in @('KR_Mycelia_Memory','KR_Mycelia_Service','KR_Mycelia_MCP','KR_Mycelia_Import')) {
    Get-Process -Name $Name -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.MainWindowHandle -ne 0) { [void]$_.CloseMainWindow() }
    }
}
Start-Sleep -Milliseconds 500
foreach ($Name in @('KR_Mycelia_Memory','KR_Mycelia_Service','KR_Mycelia_MCP','KR_Mycelia_Import')) {
    Get-Process -Name $Name -ErrorAction SilentlyContinue | Stop-Process -Force
}

New-Item -ItemType Directory -Path $Install -Force | Out-Null
New-Item -ItemType Directory -Path $Data -Force | Out-Null
$Backup = Join-Path $Install '.update-backup'
if (Test-Path -LiteralPath $Backup) { Remove-Item -LiteralPath $Backup -Recurse -Force }
New-Item -ItemType Directory -Path $Backup | Out-Null
foreach ($Name in $PayloadNames) {
    $Current = Join-Path $Install $Name
    if (Test-Path -LiteralPath $Current) { Copy-Item -LiteralPath $Current -Destination $Backup -Force }
}
$CurrentManifest = Join-Path $Install $ManifestName
if (Test-Path -LiteralPath $CurrentManifest) { Copy-Item -LiteralPath $CurrentManifest -Destination $Backup -Force }

try {
    foreach ($Name in $Programs) {
        $Source = Join-Path $Build $Name
        $Destination = Join-Path $Install $Name
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $Source).Hash -ne
            (Get-FileHash -Algorithm SHA256 -LiteralPath $Destination).Hash) {
            throw "Prüfsummenfehler bei $Name"
        }
    }
    foreach ($Name in $RuntimeNames) {
        $Source = $RuntimeSources[$Name]
        $Destination = Join-Path $Install $Name
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $Source).Hash -ne
            (Get-FileHash -Algorithm SHA256 -LiteralPath $Destination).Hash) {
            throw "Prüfsummenfehler bei $Name"
        }
    }
    foreach ($Name in $DocumentationSources.Keys) {
        $Source = $DocumentationSources[$Name]
        $Destination = Join-Path $Install $Name
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $Source).Hash -ne
            (Get-FileHash -Algorithm SHA256 -LiteralPath $Destination).Hash) {
            throw "Prüfsummenfehler bei $Name"
        }
    }

    $SelfTest = Start-Process -FilePath (Join-Path $Install 'KR_Mycelia_Memory.exe') -ArgumentList '--persistence-self-test' -Wait -PassThru
    if ($SelfTest.ExitCode -ne 0) { throw "DAD-Persistenz-Selbsttest fehlgeschlagen (Code $($SelfTest.ExitCode))." }

    Start-Process -FilePath (Join-Path $Install 'KR_Mycelia_Service.exe') -WindowStyle Hidden
    $ServiceTest = Start-Process -FilePath (Join-Path $Install 'KR_Mycelia_Memory.exe') -ArgumentList '--service-e2e-test' -Wait -PassThru
    if ($ServiceTest.ExitCode -ne 0) { throw "Installierter Dienst-/DAD-Neustarttest fehlgeschlagen (Code $($ServiceTest.ExitCode))." }
    Start-Process -FilePath (Join-Path $Install 'KR_Mycelia_Service.exe') -WindowStyle Hidden

    $Shell = New-Object -ComObject WScript.Shell
    $Shortcut = $Shell.CreateShortcut($Link)
    $Shortcut.TargetPath = Join-Path $Install 'KR_Mycelia_Memory.exe'
    $Shortcut.WorkingDirectory = $Install
    $Shortcut.Description = 'KR Mycelia Memory – lokales dauerhaftes KI-Gedächtnis'
    $Shortcut.Save()

    $Manifest = [ordered]@{ installed_at=(Get-Date).ToString('o'); version=3; edition='Windows 11 Premium'; architecture='x64'; files=@{} }
    foreach ($Name in $PayloadNames) {
        $Manifest.files[$Name] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $Install $Name)).Hash
    }
    $Manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $Install $ManifestName) -Encoding UTF8
} catch {
    foreach ($Name in @('KR_Mycelia_Memory','KR_Mycelia_Service','KR_Mycelia_MCP','KR_Mycelia_Import')) {
        Get-Process -Name $Name -ErrorAction SilentlyContinue | Stop-Process -Force
    }
    foreach ($Name in $PayloadNames) {
        $InstalledFile = Join-Path $Install $Name
        if (Test-Path -LiteralPath $InstalledFile) { Remove-Item -LiteralPath $InstalledFile -Force }
    }
    $InstalledManifest = Join-Path $Install $ManifestName
    if (Test-Path -LiteralPath $InstalledManifest) { Remove-Item -LiteralPath $InstalledManifest -Force }
    foreach ($File in Get-ChildItem -LiteralPath $Backup -File -ErrorAction SilentlyContinue) {
        Copy-Item -LiteralPath $File.FullName -Destination $Install -Force
    }
    throw
} finally {
    if (Test-Path -LiteralPath $Backup) { Remove-Item -LiteralPath $Backup -Recurse -Force }
}

Write-Host "Installiert und geprüft: $Install" -ForegroundColor Green
Write-Host 'DAD-Persistenz-Selbsttest: bestanden.' -ForegroundColor Green
Write-Host 'Installierter Dienst-/DAD-Neustarttest: bestanden.' -ForegroundColor Green
Write-Host 'Hintergrunddienst: gestartet. Vorhandene Einstellungen für Autostart und Kontexterfassung wurden beibehalten.'
