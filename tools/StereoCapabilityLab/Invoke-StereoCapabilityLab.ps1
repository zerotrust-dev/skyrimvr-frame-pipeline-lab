[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [string]$OutputRoot = (Join-Path $PSScriptRoot 'results'),

    [ValidateSet('Smoke', 'Validation', 'Benchmark')]
    [string]$Mode = 'Smoke',

    [ValidateSet('S0', 'S1', 'S2', 'S3')]
    [string]$Scene = 'S1',

    [int]$Width = 1280,
    [int]$Height = 1280,
    [int]$Draws = 1000,
    [int]$Warmup = 300,
    [int]$Frames = 2000,
    [string]$Backends = 'B0,B1,B2,B3',
    [switch]$DebugLayer,
    [switch]$Warp
)

$ErrorActionPreference = 'Stop'
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputRoot)
[System.IO.Directory]::CreateDirectory($resolvedOutput) | Out-Null
$wrapperLog = Join-Path $resolvedOutput 'launcher-events.jsonl'
$requestedExecutable = [System.IO.Path]::GetFullPath($Executable)
if (-not (Test-Path -LiteralPath $requestedExecutable -PathType Leaf)) {
    $missingEvent = [ordered]@{
        utc = [DateTime]::UtcNow.ToString('o')
        event = 'launcher_preflight_failure'
        executable = $requestedExecutable
        message = 'Executable does not exist or is not a file.'
        exit_code = 9002
    }
    Add-Content -LiteralPath $wrapperLog -Value ($missingEvent | ConvertTo-Json -Compress) -Encoding utf8
    [Console]::Error.WriteLine("Executable not found: $requestedExecutable")
    exit 9002
}
$resolvedExecutable = (Resolve-Path -LiteralPath $requestedExecutable).Path

$started = [DateTime]::UtcNow
$arguments = @('--output', $resolvedOutput, '--shaders', (Join-Path (Split-Path -Parent $resolvedExecutable) 'shaders'))

switch ($Mode) {
    'Smoke' {
        $arguments += '--smoke'
    }
    'Validation' {
        $arguments += @('--validation-only', '--scene', 'S0', '--width', $Width, '--height', $Height,
            '--validation-draws', [Math]::Min($Draws, 256), '--backends', $Backends)
    }
    'Benchmark' {
        $arguments += @('--scene', $Scene, '--width', $Width, '--height', $Height, '--draws', $Draws,
            '--warmup', $Warmup, '--frames', $Frames, '--backends', $Backends)
    }
}
if ($DebugLayer) { $arguments += '--debug' }
if ($Warp) { $arguments += '--warp' }

$startEvent = [ordered]@{
    utc = $started.ToString('o')
    event = 'process_start'
    executable = $resolvedExecutable
    executable_sha256 = (Get-FileHash -LiteralPath $resolvedExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
    mode = $Mode
    arguments = $arguments
    pid = $PID
}
Add-Content -LiteralPath $wrapperLog -Value ($startEvent | ConvertTo-Json -Compress -Depth 4) -Encoding utf8

try {
    & $resolvedExecutable @arguments
    $exitCode = $LASTEXITCODE
} catch {
    $exitCode = 9001
    $failure = [ordered]@{
        utc = [DateTime]::UtcNow.ToString('o')
        event = 'launcher_exception'
        message = $_.Exception.Message
        category = $_.CategoryInfo.Category.ToString()
    }
    Add-Content -LiteralPath $wrapperLog -Value ($failure | ConvertTo-Json -Compress) -Encoding utf8
}

$finishEvent = [ordered]@{
    utc = [DateTime]::UtcNow.ToString('o')
    event = 'process_finish'
    exit_code = $exitCode
    elapsed_seconds = [Math]::Round(([DateTime]::UtcNow - $started).TotalSeconds, 3)
}
Add-Content -LiteralPath $wrapperLog -Value ($finishEvent | ConvertTo-Json -Compress) -Encoding utf8
exit $exitCode
