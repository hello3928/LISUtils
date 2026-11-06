[CmdletBinding()]
param(
    [string]$GameDirectory,
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

function Find-GameDirectory {
    $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -Name SteamPath -ErrorAction SilentlyContinue).SteamPath
    if (-not $steam) {
        return $null
    }
    $libraries = @($steam)
    $manifest = Join-Path $steam 'steamapps\libraryfolders.vdf'
    if (Test-Path $manifest) {
        $libraries += Select-String -Path $manifest -Pattern '"path"\s+"([^"]+)"' |
            ForEach-Object { $_.Matches[0].Groups[1].Value -replace '\\\\', '\' }
    }
    foreach ($library in $libraries | Select-Object -Unique) {
        $candidate = Join-Path $library 'steamapps\common\Life Is Strange'
        if (Test-Path (Join-Path $candidate 'Binaries\Win32\LifeIsStrange.exe')) {
            return $candidate
        }
    }
    return $null
}

if (-not $GameDirectory) {
    $GameDirectory = Find-GameDirectory
}
if (-not $GameDirectory) {
    throw 'Life is Strange was not found. Pass -GameDirectory with the game install folder.'
}

$source = Join-Path $PSScriptRoot "..\build\win32\$Configuration\dinput8.dll"
if (-not (Test-Path $source)) {
    throw "Build output not found at $source. Run: cmake --build --preset $($Configuration.ToLower())"
}

$destination = Join-Path $GameDirectory 'Binaries\Win32'
Copy-Item -Path $source -Destination $destination -Force
Write-Host "Installed dinput8.dll to $destination"
