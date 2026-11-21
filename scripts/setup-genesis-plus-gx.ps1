$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$msys = 'C:\msys64'
$gpgxCommit = '4fd32efe1fec3537d11f7aaa24cb961c6ff57fdd'
$dest = Join-Path $repo 'external\genesis_plus_gx'

if (-not (Test-Path (Join-Path $dest '.git'))) {
    git clone https://github.com/libretro/Genesis-Plus-GX.git $dest
}
git -C $dest fetch --quiet origin
git -C $dest checkout $gpgxCommit

$env:MSYSTEM = 'UCRT64'
$env:CHERE_INVOKING = '1'
$msysDest = (& "$msys\usr\bin\cygpath.exe" -u $dest)
& "$msys\usr\bin\bash.exe" -lc "cd '$msysDest' && make -f Makefile.libretro platform=win -j8"
if ($LASTEXITCODE -ne 0) { throw 'Genesis Plus GX build failed' }

$dataRoot = if ($env:XFUSION_DATA_DIR) { $env:XFUSION_DATA_DIR } else { Join-Path $repo 'build' }
$coresDir = Join-Path $dataRoot 'cores'
New-Item -ItemType Directory -Force $coresDir | Out-Null
Copy-Item (Join-Path $dest 'genesis_plus_gx_libretro.dll') $coresDir -Force
Write-Host "Core installed in $coresDir"