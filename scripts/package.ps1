# Builds Xfusion from scratch in build-release\, runs the tests and leaves dist\Xfusion-<version>-win64.zip (the
# executable, the license notices and the docs) with its SHA-256. The cores and the BIOS files are not included: the
# cores have their own licenses and are downloaded from Options > Emulation > Manage Cores.
#
#   scripts\package.ps1 [-SkipTests] [-OutDir <folder>]
param(
    [switch]$SkipTests,
    [string]$OutDir
)
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$msys = 'C:\msys64'
if (-not $OutDir) { $OutDir = Join-Path $repo 'dist' }
$build = Join-Path $repo 'build-release'

$version = (Select-String -Path (Join-Path $repo 'CMakeLists.txt') -Pattern 'project\(Xfusion VERSION ([0-9.]+)').Matches[0].Groups[1].Value
$commit = (git -C $repo rev-parse --short HEAD).Trim()
$dirty = [bool](git -C $repo status --porcelain)
# The date of the commit stands in for "now" wherever a tool would stamp the time, so the same commit gives the same file.
$epoch = (git -C $repo log -1 --format=%ct).Trim()

if (Test-Path $build) { Remove-Item $build -Recurse -Force }
$env:MSYSTEM = 'CLANG64'
$env:CHERE_INVOKING = '1'
$env:SOURCE_DATE_EPOCH = $epoch
$src = (& "$msys\usr\bin\cygpath.exe" -u $repo)
$bld = (& "$msys\usr\bin\cygpath.exe" -u $build)
& "$msys\usr\bin\bash.exe" -lc "cd '$src' && cmake -G Ninja -B '$bld' -S . -DCMAKE_BUILD_TYPE=Release -DXFUSION_STATIC=ON && ninja -C '$bld'"
if ($LASTEXITCODE -ne 0) { throw 'The build failed' }

if (-not $SkipTests) {
    $env:QT_QPA_PLATFORM = 'offscreen'
    foreach ($test in 'core_tests', 'patch_tests', 'disc_tests', 'library_tests', 'save_tests', 'settings_tests', 'input_tests') {
        & (Join-Path $build "$test.exe") | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "$test failed" }
    }
    Remove-Item Env:QT_QPA_PLATFORM
}

$name = "Xfusion-$version-win64"
$stage = Join-Path $build $name
New-Item -ItemType Directory $stage | Out-Null
Copy-Item (Join-Path $build 'xfusion.exe') $stage
foreach ($file in 'README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md') { Copy-Item (Join-Path $repo $file) $stage }
Copy-Item (Join-Path $repo 'docs') (Join-Path $stage 'docs') -Recurse
@"
Xfusion $version ($commit)

Run xfusion.exe. Everything it writes (settings, saves, states, cores) goes in this folder, or in
%LOCALAPPDATA%\Xfusion when this folder cannot be written to. Delete the folder to remove it.
The cores and the Sega CD BIOS files are not included: see README.md ("Empezar").
"@ | Set-Content (Join-Path $stage 'LEEME.txt') -Encoding utf8

New-Item -ItemType Directory -Force $OutDir | Out-Null
$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item $zip }
# Fixed timestamps and order, so that the zip does not change from one run to the next.
Add-Type -AssemblyName System.IO.Compression
$stamp = [DateTimeOffset]::FromUnixTimeSeconds([long]$epoch)
$stream = [IO.File]::Create($zip)
$archive = New-Object IO.Compression.ZipArchive($stream, [IO.Compression.ZipArchiveMode]::Create)
foreach ($file in (Get-ChildItem $stage -Recurse -File | Sort-Object FullName)) {
    $entryName = $name + '/' + $file.FullName.Substring($stage.Length + 1).Replace('\', '/')
    $entry = $archive.CreateEntry($entryName, [IO.Compression.CompressionLevel]::Optimal)
    $entry.LastWriteTime = $stamp
    $in = [IO.File]::OpenRead($file.FullName)
    $out = $entry.Open()
    $in.CopyTo($out)
    $out.Dispose(); $in.Dispose()
}
$archive.Dispose(); $stream.Dispose()

$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
"$hash  $name.zip" | Set-Content (Join-Path $OutDir "$name.zip.sha256") -Encoding ascii
$exeHash = (Get-FileHash (Join-Path $build 'xfusion.exe') -Algorithm SHA256).Hash.ToLower()
Write-Host "Package: $zip"
Write-Host "SHA-256 of the zip: $hash"
Write-Host "SHA-256 of xfusion.exe: $exeHash"
if ($dirty) { Write-Warning 'The working tree has changes that are not committed: the package is not reproducible from the commit.' }
