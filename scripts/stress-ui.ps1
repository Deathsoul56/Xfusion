[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Rom,
    [string]$Exe = (Join-Path $PSScriptRoot '..\build\xfusion.exe'),
    [int]$Seconds = 20,
    [int]$WarmupSeconds = 5
)

$ErrorActionPreference = 'Stop'
$Exe = (Resolve-Path $Exe).Path
if (Test-Path 'C:\msys64\clang64\bin') { $env:PATH = "C:\msys64\clang64\bin;$env:PATH" }

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class XfStress {
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    public struct RECT { public int Left, Top, Right, Bottom; }
}
'@

function Send-Key([byte]$vk, [bool]$down) { [XfStress]::keybd_event($vk, 0, $(if ($down) { 0 } else { 2 }), [UIntPtr]::Zero) }

function Drag-Title($process) {
    $rect = New-Object XfStress+RECT
    [void][XfStress]::GetWindowRect($process.MainWindowHandle, [ref]$rect)
    $x = $rect.Left + 300
    $y = $rect.Top + 12
    [void][XfStress]::SetCursorPos($x, $y)
    [XfStress]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    foreach ($direction in 1, -1) {
        for ($i = 0; $i -lt 20; $i++) {
            $x += 5 * $direction
            $y += 3 * $direction
            [void][XfStress]::SetCursorPos($x, $y)
            Start-Sleep -Milliseconds 30
        }
    }
    [XfStress]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
}

function Open-Menu {
    Send-Key 0x12 $true; Send-Key 0x46 $true; Send-Key 0x46 $false; Send-Key 0x12 $false
    Start-Sleep -Milliseconds 800
    Send-Key 0x1B $true; Send-Key 0x1B $false
    Start-Sleep -Milliseconds 200
}

$log = Join-Path $env:TEMP 'xfusion-stress.log'
Remove-Item $log -ErrorAction SilentlyContinue
$process = Start-Process $Exe -ArgumentList '--stats', "`"$Rom`"" -RedirectStandardError $log -PassThru
try {
    Start-Sleep -Seconds 3
    $process.Refresh()
    [void][XfStress]::SetForegroundWindow($process.MainWindowHandle)
    Start-Sleep -Seconds 1

    $stopAt = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $stopAt) {
        Drag-Title $process
        Open-Menu
    }
}
finally {
    Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
}

$samples = @(Get-Content $log | Where-Object { $_ -match '^\[stats\] frames=(\d+) underruns=(\d+) audioQueue=(\d+)\.\.(\d+)' } |
    ForEach-Object { [pscustomobject]@{ Frames = [int]$Matches[1]; Underruns = [int]$Matches[2]; MinQueue = [int]$Matches[3] } })
$samples = @($samples | Select-Object -Skip $WarmupSeconds)
if ($samples.Count -eq 0) { throw "No stats collected; see $log" }

$frames = $samples | Measure-Object Frames -Minimum -Average
$stalled = @($samples | Where-Object { $_.Frames -lt 55 }).Count
"seconds sampled : $($samples.Count)"
"frames/second   : min $($frames.Minimum), average $([math]::Round($frames.Average, 1)) (nominal 60)"
"slow seconds    : $stalled (fewer than 55 frames)"
"audio underruns : $(($samples | Measure-Object Underruns -Sum).Sum)"
"lowest audio queue (bytes): $(($samples | Measure-Object MinQueue -Minimum).Minimum)"
