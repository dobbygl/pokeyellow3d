param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$BuildDirectory = 'build'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Version -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') { throw 'Invalid package version' }
$project = (Get-Location).Path
$build = (Resolve-Path $BuildDirectory).Path
$name = "pokeyellow3d-windows-x86_64-$Version"
$destination = Join-Path $project "dist/$name"
if (Test-Path $destination) { throw "Package directory already exists: $destination" }
New-Item -ItemType Directory -Path "$destination/THIRD_PARTY" -Force | Out-Null
Copy-Item "$build/pokeyellow3d.exe", 'README.md', 'PALLET3D.md' $destination
Copy-Item '.github/release/RUN-windows.md' "$destination/RUN.md"
Set-Content -Path "$destination/Start.cmd" -Encoding ascii -Value @('@echo off', 'cd /d "%~dp0"', 'pokeyellow3d.exe %*')

# Release DLLs only; never copy the build tree, ROMs, saves or extracted assets.
$installed = Join-Path $build 'vcpkg_installed/x64-windows'
$dlls = @(Get-ChildItem "$installed/bin" -File -Filter '*.dll')
if ($dlls.Count -lt 3) { throw 'Missing release dependency DLLs' }
$dlls | Copy-Item -Destination $destination
foreach ($port in Get-ChildItem "$installed/share" -Directory) {
    $copyright = Join-Path $port.FullName 'copyright'
    if (Test-Path $copyright) {
        Copy-Item $copyright "$destination/THIRD_PARTY/$($port.Name).txt"
    }
}
$crt = @(Get-ChildItem "$env:VCToolsRedistDir/x64" -Directory -Filter 'Microsoft.VC*.CRT')
if ($crt.Count -ne 1) { throw 'Cannot identify the x64 MSVC redistributable directory' }
Get-ChildItem $crt[0].FullName -File -Filter '*.dll' | Copy-Item -Destination $destination
foreach ($required in 'SDL2.dll', 'libEGL.dll', 'libGLESv2.dll', 'msvcp140.dll', 'vcruntime140.dll') {
    if (-not (Test-Path "$destination/$required")) { throw "Missing packaged dependency: $required" }
}
Get-ChildItem $destination -File | Where-Object { $_.Extension -in '.exe', '.dll' } |
    Sort-Object Name | ForEach-Object {
        "$((Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower())  $($_.Name)"
    } | Set-Content "$destination/DEPENDENCIES.txt" -Encoding utf8

$archive = Join-Path $project "dist/$name.zip"
Compress-Archive -Path $destination -DestinationPath $archive
"$((Get-FileHash $archive -Algorithm SHA256).Hash.ToLower())  $name.zip" |
    Set-Content "$archive.sha256" -Encoding ascii

# Exercise the extracted artifact without the developer or vcpkg DLL search path.
$verify = Join-Path $build 'qa/package-verification'
New-Item -ItemType Directory -Path $verify -Force | Out-Null
Expand-Archive $archive -DestinationPath $verify
$unpacked = Join-Path $verify $name
Copy-Item "$build/render_preview_synthetic.exe" "$unpacked/package-render-check.exe"
$originalPath = $env:PATH
Push-Location $unpacked
try {
    $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
    $env:SDL_VIDEODRIVER = 'windows'
    $env:SDL_AUDIODRIVER = 'dummy'
    $launcher = & ./pokeyellow3d.exe --list-games 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or $launcher -notmatch 'Pokemon Yellow') {
        throw "Extracted launcher failed: $launcher"
    }
    $render = & ./package-render-check.exe 2>&1 | Out-String
    $render | Write-Output
    if ($LASTEXITCODE -ne 0 -or $render -notmatch 'driver=windows' -or $render -notmatch 'ANGLE') {
        throw 'Extracted package must render on native Windows/ANGLE without developer PATH'
    }
} finally {
    $env:PATH = $originalPath
    Pop-Location
    Remove-Item "$unpacked/package-render-check.exe"
}
Write-Output "PASS: extracted $name launcher and Windows/ANGLE rendering; no ROM required"
