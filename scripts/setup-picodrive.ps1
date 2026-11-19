$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$msys = 'C:\msys64'
$picodriveCommit = '26ecb2b6358fefba24e3d68b9eb2efba7f10d5ee'
$dest = Join-Path $repo 'external\picodrive'

if (-not (Test-Path (Join-Path $dest '.git'))) {
    git clone --recurse-submodules https://github.com/notaz/picodrive.git $dest
}
git -C $dest checkout $picodriveCommit
git -C $dest submodule update --init --recursive

$env:MSYSTEM = 'UCRT64'
$env:CHERE_INVOKING = '1'
$msysDest = (& "$msys\usr\bin\cygpath.exe" -u $dest)
& "$msys\usr\bin\bash.exe" -lc "cd '$msysDest' && make -f Makefile.libretro platform=win -j8"
if ($LASTEXITCODE -ne 0) { throw 'PicoDrive build failed' }

$dataRoot = if ($env:XFUSION_DATA_DIR) { $env:XFUSION_DATA_DIR } else { Join-Path $repo 'build' }
$coresDir = Join-Path $dataRoot 'cores'
New-Item -ItemType Directory -Force $coresDir | Out-Null
Copy-Item (Join-Path $dest 'picodrive_libretro.dll') $coresDir -Force
Write-Host "Core installed in $coresDir"
