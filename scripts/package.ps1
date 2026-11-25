# Builds Xfusion from scratch in build-release\, runs the tests and leaves dist\Xfusion-<version>-win64.zip with its SHA-256: a portable
# folder that runs as soon as it is unzipped (ADR-0030, ADR-0033). It holds the executable, the two cores (Genesis Plus GX and
# PicoDrive, as they were built by scripts\setup-*.ps1 from their pinned commits, with their licenses), the notices and the docs.
# The Sega CD BIOS files are not included: they are not ours to give.
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

# The cores come from the clones that scripts\setup-*.ps1 make and build, at the commit each script pins, without changes.
function Pinned($script, $variable) {
    (Select-String -Path (Join-Path $repo "scripts\$script") -Pattern "^\`$$variable = '([0-9a-f]{40})'").Matches[0].Groups[1].Value
}
$cores = @(
    @{ Id = 'genesis_plus_gx'; Name = 'Genesis Plus GX'; Url = 'https://github.com/libretro/Genesis-Plus-GX'; Dir = 'external\genesis_plus_gx'
       Commit = (Pinned 'setup-genesis-plus-gx.ps1' 'gpgxCommit')
       Licenses = 'LICENSE.txt', 'core\ntsc\license.txt', 'core\sound\minimp3\LICENSE.txt', 'core\sound\tremor\COPYING',
                  'libretro\deps\libchdr\LICENSE.txt', 'libretro\deps\lzma-19.00\LICENSE', 'libretro\deps\zstd\LICENSE', 'libretro\deps\zstd\COPYING' },
    @{ Id = 'picodrive'; Name = 'PicoDrive'; Url = 'https://github.com/notaz/picodrive'; Dir = 'external\picodrive'
       Commit = (Pinned 'setup-picodrive.ps1' 'picodriveCommit')
       Licenses = 'COPYING', 'pico\cd\libchdr\LICENSE.txt', 'pico\cd\libchdr\deps\lzma-24.05\LICENSE', 'pico\cd\libchdr\deps\zlib-1.3.1\LICENSE',
                  'pico\cd\libchdr\deps\zstd-1.5.6\LICENSE', 'pico\cd\libchdr\deps\zstd-1.5.6\COPYING', 'pico\sound\emu2413\LICENSE', 'platform\common\dr_libs\LICENSE' }
)
foreach ($core in $cores) {
    $dir = Join-Path $repo $core.Dir
    $dll = Join-Path $dir "$($core.Id)_libretro.dll"
    if (-not (Test-Path $dll)) { throw "$($core.Name) is not built: run scripts\setup-$($core.Id -replace '_', '-').ps1" }
    $head = (git -C $dir rev-parse HEAD).Trim()
    if ($head -ne $core.Commit) { throw "$($core.Name) is at $head, not at the pinned $($core.Commit)" }
    if (git -C $dir status --porcelain --untracked-files=no) { throw "$($core.Name) has changes: a core with changes cannot go in the package (ADR-0006)" }
    foreach ($license in $core.Licenses) { if (-not (Test-Path (Join-Path $dir $license))) { throw "$($core.Name): the license $license is missing" } }
    $core.Dll = $dll
}

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
    foreach ($test in 'core_tests', 'patch_tests', 'disc_tests', 'library_tests', 'save_tests', 'settings_tests', 'input_tests', 'shader_tests') {
        & (Join-Path $build "$test.exe") | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "$test failed" }
    }
    Remove-Item Env:QT_QPA_PLATFORM
}

$name = "Xfusion-$version-win64"
$stage = Join-Path $build $name
New-Item -ItemType Directory $stage | Out-Null
Copy-Item (Join-Path $build 'xfusion.exe') $stage
Copy-Item (Join-Path $build 'xfusion-shaderc.exe') $stage # compiles the shaders of presets of the user
foreach ($file in 'README.md', 'CHANGELOG.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md') { Copy-Item (Join-Path $repo $file) $stage }
Copy-Item (Join-Path $repo 'docs') (Join-Path $stage 'docs') -Recurse

$coresDir = Join-Path $stage 'cores'
New-Item -ItemType Directory $coresDir | Out-Null
$sources = @()
foreach ($core in $cores) {
    Copy-Item $core.Dll $coresDir
    $licenseDir = Join-Path $stage "licenses\cores\$($core.Id)"
    foreach ($license in $core.Licenses) {
        $target = Join-Path $licenseDir $license
        New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
        Copy-Item (Join-Path (Join-Path $repo $core.Dir) $license) $target
    }
    $hash = (Get-FileHash $core.Dll -Algorithm SHA256).Hash.ToLower()
    $sources += "$($core.Name): $($core.Url), commit $($core.Commit), without changes. $(Split-Path -Leaf $core.Dll) SHA-256 $hash"
}
@"
The cores in this folder are built, without changes, from the source code at these commits (the code is public; Xfusion does not
change it). Their licenses are in licenses\cores\. They may not be sold or used in a commercial product or activity (ADR-0006).

$($sources -join "`r`n")
"@ | Set-Content (Join-Path $stage 'cores\SOURCES.txt') -Encoding utf8

@"
Xfusion $version ($commit)

Abre xfusion.exe: no hace falta instalar nada ni ser administrador. Los cores (Genesis Plus GX y PicoDrive) ya estan en cores\.
Solo faltan tus juegos: File > Open File..., o Options > Files > Library Folders para tener una biblioteca.
Para Sega CD necesitas ademas tus BIOS (bios_CD_J.bin, bios_CD_E.bin, bios_CD_U.bin) en system\: Options > Files > Find BIOS Files... los reconoce.

Todo lo que Xfusion escribe (ajustes, saves, estados, capturas) queda en esta carpeta, o en %LOCALAPPDATA%\Xfusion si esta
carpeta no se puede escribir (por ejemplo Program Files). Para llevarte tus datos a otro equipo, copia la carpeta; para quitar Xfusion,
borrala. Windows 10 u 11 de 64 bits. El ejecutable no esta firmado: SmartScreen puede avisar la primera vez (Mas informacion > Ejecutar de todos modos).

xfusion-shaderc.exe tiene que estar junto a xfusion.exe: compila los shaders de los presets de RetroArch que pongas en shaders\.
Licencias: LICENSE (Xfusion), THIRD_PARTY_NOTICES.md y licenses\ (los cores).
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
Write-Host "Package: $zip ($([math]::Round((Get-Item $zip).Length / 1MB, 1)) MB)"
Write-Host "SHA-256 of the zip: $hash"
Write-Host "SHA-256 of xfusion.exe: $exeHash"
if ($dirty) { Write-Warning 'The working tree has changes that are not committed: the package is not reproducible from the commit.' }
