# CheckGameplayDeps.ps1 — dependency-direction guard for the game layers.
# Fails (exit 1) if any source includes a forbidden subsystem:
#   Gameplay/  — Rendering / GUI / Platform / OpenGL / glad / glfw /
#                ImGui / raw GL headers / Campaign/ / Game/
#   Campaign/  — the same, except Gameplay/ includes are allowed;
#                Game/ is still forbidden.
# Dependency direction: Engine <- Gameplay <- Campaign <- Game.
#
# Usage: powershell -NoProfile -File scripts/CheckGameplayDeps.ps1
# Works from any cwd; resolves the repo root from the script location.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$simForbidden = 'Rendering/', 'GUI/', 'Platform/', 'OpenGL', 'glad', 'glfw', 'ImGui', 'GL/', 'GLES', 'KHR/'
$violations = @()

function Scan-Dir([string]$dir, [string[]]$forbidden) {
    $files = Get-ChildItem $dir -Recurse -Include *.h,*.hpp,*.cpp
    foreach ($file in $files) {
        # @(...) forces array even for single-line files
        $lines = @(Get-Content $file.FullName)
        for ($i = 0; $i -lt $lines.Count; ++$i) {
            # normalize Windows-style include separators before token match
            $line = ($lines[$i] -replace '\\', '/')
            if ($line -notmatch '^\s*#\s*include') { continue }
            foreach ($token in $forbidden) {
                if ($line -like "*$token*") {
                    $script:violations += "{0}:{1}: {2}" -f $file.FullName, ($i + 1), $lines[$i].Trim()
                }
            }
        }
    }
    return $files.Count
}

$scanned = 0
$gameplayDir = Join-Path $root 'Gameplay'
if (Test-Path $gameplayDir) {
    # Gameplay is the bottom layer — no upward or presentation deps.
    $scanned += Scan-Dir $gameplayDir ($simForbidden + 'Campaign/', 'Game/')
}
$campaignDir = Join-Path $root 'Campaign'
if (Test-Path $campaignDir) {
    # Campaign may consume Gameplay but never Game/ or presentation.
    $scanned += Scan-Dir $campaignDir ($simForbidden + 'Game/')
}
if ($scanned -eq 0) {
    Write-Host "SKIP: Gameplay/ and Campaign/ not present"
    exit 0
}

if ($violations.Count -gt 0) {
    Write-Host "FAIL: forbidden includes in game layers:"
    $violations | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host ("PASS: {0} files clean" -f $scanned)
exit 0
