[CmdletBinding()]
param(
    [string]$Path = (Join-Path $PSScriptRoot '..\build\test-rom.md')
)

# Writes a tiny Genesis ROM of our own (no copyrighted code) for checking the frontend without game files.
#
# Behaviour, all visible as the backdrop colour:
#   * player 1 Up held      -> red
#   * player 2 Up held      -> green
#   * player 1 B (press)    -> toggles blue; the toggle is the game state, kept in work RAM and in battery SRAM
# At start the toggle is read back from SRAM, so closing and reopening the game keeps it (save file check), and
# a save state restores it (state check).

$ErrorActionPreference = 'Stop'

$items = @(
    0x13FC, 0x0001, 0x00A1, 0x30F1,        # move.b #1,$A130F1      enable the cartridge SRAM
    0x13FC, 0x0040, 0x00A1, 0x0009,        # move.b #$40,$A10009    port 1 control: TH is an output
    0x13FC, 0x0040, 0x00A1, 0x000B,        # move.b #$40,$A1000B    port 2 control
    0x13FC, 0x0040, 0x00A1, 0x0003,        # move.b #$40,$A10003    TH high
    0x13FC, 0x0040, 0x00A1, 0x0005,
    0x23FC, 0x5345, 0x4741, 0x00A1, 0x4000, # move.l #'SEGA',$A14000 TMSS
    0x33FC, 0x8004, 0x00C0, 0x0004,        # VDP register 0
    0x33FC, 0x8144, 0x00C0, 0x0004,        # VDP register 1: display on
    0x33FC, 0x8700, 0x00C0, 0x0004,        # VDP register 7: backdrop colour 0
    0x1839, 0x0020, 0x0001,                # move.b $200001,d4      toggle saved in SRAM
    0x0244, 0x0001,                        # andi.w #1,d4
    0x33C4, 0x00FF, 0x0000,                # move.w d4,$FF0000
    0x4279, 0x00FF, 0x0002,                # clr.w $FF0002          previous B state
    'label:loop',
    0x7000,                                # moveq #0,d0
    0x1239, 0x00A1, 0x0003,                # move.b $A10003,d1      player 1
    0x0801, 0x0000,                        # btst #0,d1             Up (0 = pressed)
    @('bne.s', 'p1noup'),
    0x0040, 0x000E,                        # ori.w #$000E,d0        red
    'label:p1noup',
    0x7600,                                # moveq #0,d3
    0x0801, 0x0004,                        # btst #4,d1             B
    @('bne.s', 'notb'),
    0x7601,                                # moveq #1,d3
    'label:notb',
    0x3839, 0x00FF, 0x0002,                # move.w $FF0002,d4
    0x33C3, 0x00FF, 0x0002,                # move.w d3,$FF0002
    0x4A44,                                # tst.w d4
    @('bne.s', 'skip'),
    0x4A43,                                # tst.w d3
    @('beq.s', 'skip'),
    0x0A79, 0x0001, 0x00FF, 0x0000,        # eori.w #1,$FF0000      toggle on the press edge
    'label:skip',
    0x3839, 0x00FF, 0x0000,                # move.w $FF0000,d4
    0x13C4, 0x0020, 0x0001,                # move.b d4,$200001      keep it in SRAM
    0x4A44,                                # tst.w d4
    @('beq.s', 'nostate'),
    0x0040, 0x0E00,                        # ori.w #$0E00,d0        blue
    'label:nostate',
    0x1239, 0x00A1, 0x0005,                # move.b $A10005,d1      player 2
    0x0801, 0x0000,                        # btst #0,d1
    @('bne.s', 'p2noup'),
    0x0040, 0x00E0,                        # ori.w #$00E0,d0        green
    'label:p2noup',
    0x23FC, 0xC000, 0x0000, 0x00C0, 0x0004, # move.l #$C0000000,$C00004   CRAM address 0
    0x33C0, 0x00C0, 0x0000,                # move.w d0,$C00000
    @('bra', 'loop')
)

$codeStart = 0x200
$labels = @{}
$pc = $codeStart
foreach ($item in $items) {
    if ($item -is [string]) { $labels[$item.Substring(6)] = $pc }
    elseif ($item -is [array]) { $pc += $(if ($item[0] -eq 'bra') { 4 } else { 2 }) }
    else { $pc += 2 }
}

$code = New-Object System.Collections.Generic.List[byte]
function Add-Word([int]$word) { $code.Add([byte](($word -shr 8) -band 0xFF)); $code.Add([byte]($word -band 0xFF)) }
$pc = $codeStart
foreach ($item in $items) {
    if ($item -is [string]) { continue }
    if ($item -is [array]) {
        $target = $labels[$item[1]]
        if ($item[0] -eq 'bra') {
            Add-Word 0x6000
            Add-Word (($target - ($pc + 2)) -band 0xFFFF)
            $pc += 4
        } else {
            $displacement = $target - ($pc + 2)
            if ($displacement -lt 0 -or $displacement -gt 127) { throw "branch to $($item[1]) out of range" }
            Add-Word ((@{ 'bne.s' = 0x6600; 'beq.s' = 0x6700 }[$item[0]]) -bor $displacement)
            $pc += 2
        }
    } else {
        Add-Word $item
        $pc += 2
    }
}

$rom = New-Object byte[] 0x8000
function Put-Long([int]$at, [uint32]$value) { for ($i = 0; $i -lt 4; $i++) { $rom[$at + $i] = [byte](($value -shr (24 - 8 * $i)) -band 0xFF) } }
function Put-Text([int]$at, [string]$text, [int]$length) {
    $bytes = [Text.Encoding]::ASCII.GetBytes($text.PadRight($length))
    [Array]::Copy($bytes, 0, $rom, $at, $length)
}

Put-Long 0 0x00FFFE00                                  # initial stack pointer
for ($vector = 1; $vector -lt 64; $vector++) { Put-Long (4 * $vector) $codeStart }
Put-Text 0x100 'SEGA MEGA DRIVE ' 16
Put-Text 0x110 '(C)XFUS 2026.NOV' 16
Put-Text 0x120 'XFUSION TEST ROM' 48
Put-Text 0x150 'XFUSION TEST ROM' 48
Put-Text 0x180 'GM 00000000-00' 14
Put-Long 0x1A0 0
Put-Long 0x1A4 0x7FFF                                  # ROM end
Put-Long 0x1A8 0x00FF0000
Put-Long 0x1AC 0x00FFFFFF
Put-Text 0x1B0 'RA' 2
$rom[0x1B2] = 0xF8; $rom[0x1B3] = 0x20                 # SRAM on odd addresses
Put-Long 0x1B4 0x00200001
Put-Long 0x1B8 0x00203FFF
Put-Text 0x1F0 'JUE' 16
[Array]::Copy($code.ToArray(), 0, $rom, $codeStart, $code.Count)

$Path = [IO.Path]::GetFullPath($Path)
[IO.File]::WriteAllBytes($Path, $rom)
"Wrote $Path ($($code.Count) bytes of code)"
