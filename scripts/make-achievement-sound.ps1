# Generates src\resources\sounds\achievement.wav: a two-note chime made from sine waves (own work, no third-party audio).
param([string]$Out = "$PSScriptRoot\..\src\resources\sounds\achievement.wav")
$rate = 22050; $dur = 0.75; $n = [int]($rate * $dur); $buf = New-Object byte[] ($n * 2)
$notes = @(@{ f = 987.77; t0 = 0.0; len = 0.30 }, @{ f = 1318.51; t0 = 0.16; len = 0.59 })
for ($i = 0; $i -lt $n; $i++) {
    $t = $i / $rate; $s = 0.0
    foreach ($no in $notes) {
        $u = $t - $no.t0
        if ($u -ge 0 -and $u -lt $no.len) {
            $env = [Math]::Min(1.0, $u / 0.01) * [Math]::Exp(-4.5 * $u / $no.len)
            $s += $env * (0.7 * [Math]::Sin(2 * [Math]::PI * $no.f * $u) + 0.2 * [Math]::Sin(4 * [Math]::PI * $no.f * $u) + 0.1 * [Math]::Sin(6 * [Math]::PI * $no.f * $u))
        }
    }
    $v = [int][Math]::Max(-32767, [Math]::Min(32767, $s * 14000)); $bytes = [BitConverter]::GetBytes([int16]$v); $buf[2 * $i] = $bytes[0]; $buf[2 * $i + 1] = $bytes[1]
}
$ms = New-Object IO.MemoryStream; $w = New-Object IO.BinaryWriter $ms
$w.Write([Text.Encoding]::ASCII.GetBytes('RIFF')); $w.Write([int](36 + $buf.Length)); $w.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt ')); $w.Write([int]16); $w.Write([int16]1); $w.Write([int16]1)
$w.Write([int]$rate); $w.Write([int]($rate * 2)); $w.Write([int16]2); $w.Write([int16]16); $w.Write([Text.Encoding]::ASCII.GetBytes('data')); $w.Write([int]$buf.Length); $w.Write($buf); $w.Flush()
[IO.File]::WriteAllBytes($Out, $ms.ToArray())
