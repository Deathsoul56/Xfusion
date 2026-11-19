[CmdletBinding()]
param(
    [string]$Repo = (Join-Path $PSScriptRoot '..\..\snes9xrd'),
    [string]$Since,
    [string]$Path,
    [string]$Author,
    [int]$Max = 50,
    [string]$Blame
)

$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path $Repo).Path
if (-not (Test-Path (Join-Path $Repo '.git'))) { throw "Not a git repository: $Repo" }

function Invoke-Git {
    $out = & git -C $Repo @args
    if ($LASTEXITCODE -ne 0) { throw "git $($args -join ' ') failed" }
    $out
}

if ($Blame) {
    $lines = Invoke-Git blame --line-porcelain -w -M -- $Blame | Where-Object { $_ -like 'author *' }
    $total = ($lines | Measure-Object).Count
    if ($total -eq 0) { throw "No blame data for $Blame" }
    "$Blame ($total lines)"
    $lines | ForEach-Object { $_.Substring(7) } | Group-Object | Sort-Object Count -Descending |
        ForEach-Object { '{0,6:N1}%  {1,5}  {2}' -f (100 * $_.Count / $total), $_.Count, $_.Name }
    $origin = Invoke-Git log --follow --reverse --format='%h|%ad|%an|%s' --date=short -- $Blame | Select-Object -First 1
    $originHash = ($origin -split '\|')[0]
    $originFiles = @(Invoke-Git diff-tree --no-commit-id --name-only -r $originHash).Count
    "origin: $origin ($originFiles files in that commit)"
    if ($originFiles -gt 15) {
        'WARNING: the file entered in a bulk commit; blame shows who committed it, not who wrote it.'
    }
    return
}

$range = if ($Since) { "$Since..HEAD" } else { 'HEAD' }
$logArgs = @('log', $range, "--max-count=$Max", '--date=short', '--format=%h|%ad|%an|%s')
if ($Author) { $logArgs += "--author=$Author" }
if ($Path) { $logArgs += '--'; $logArgs += $Path }

$commits = @(Invoke-Git @logArgs)
"$($commits.Count) commit(s) in $Repo ($range)"
foreach ($line in $commits) {
    $hash, $date, $name, $subject = $line -split '\|', 4
    "{0} {1} {2}: {3}" -f $hash, $date, $name, $subject
    $files = @(Invoke-Git diff-tree --no-commit-id --name-only -r $hash)
    if ($Path) { $files = @($files | Where-Object { $_ -like "$Path*" }) }
    $files | Select-Object -First 5 | ForEach-Object { "    $_" }
    if ($files.Count -gt 5) { "    (+$($files.Count - 5) more)" }
}
