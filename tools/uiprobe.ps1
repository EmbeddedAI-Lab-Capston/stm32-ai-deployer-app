<#
  uiprobe.ps1 - drives the app's DebugBridge (dev-only named pipe).

  The app must already be running with:
      STM32AiDeployer.exe --debug-bridge --no-splash

  Examples:
      .\tools\uiprobe.ps1 ping
      .\tools\uiprobe.ps1 navigate -Tab 7
      .\tools\uiprobe.ps1 shot -Path C:\temp\watch.png
      .\tools\uiprobe.ps1 dump -Filter Button
      .\tools\uiprobe.ps1 props -Object appState
      .\tools\uiprobe.ps1 click -Name watch.startButton
      .\tools\uiprobe.ps1 invoke -Object backend -Method scanTools
      .\tools\uiprobe.ps1 invoke -Object backend -Method takeRegisterSnapshot -ArgsJson '[0, ["RCC","I2C1"]]'
      .\tools\uiprobe.ps1 log -Lines 30
      .\tools\uiprobe.ps1 quit
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("ping", "shot", "dump", "props", "navigate", "click", "invoke", "quit", "log")]
    [string]$Command,

    [string]$Path,
    [int]$Tab = -1,
    [string]$Object = "appState",
    [string]$Name,
    [string]$Filter,
    [int]$MaxDepth = 12,
    [int]$Lines = 40,
    [string]$Method,
    [string[]]$MethodArgs = @(),
    # Raw JSON array of invoke arguments, sent verbatim - the only way to pass
    # a list or map argument (-MethodArgs sends every argument as a string).
    #   -ArgsJson '[0, ["RCC","I2C1"]]'   -ArgsJson '["<id>", {"label":"x"}]'
    [string]$ArgsJson,
    [string]$Pipe = "stm32aid-debug",
    [string]$ExeDir = "C:\dev\stm32-ai-deployer-app\build"
)

function Send-BridgeCommand([string]$JsonBody) {
    $client = New-Object System.IO.Pipes.NamedPipeClientStream(".", $Pipe, [System.IO.Pipes.PipeDirection]::InOut)
    try {
        $client.Connect(3000)
    } catch {
        Write-Output "ERROR: cannot connect to pipe '$Pipe'. Is the app running with --debug-bridge?"
        return $null
    }
    $encoding = New-Object System.Text.UTF8Encoding($false)
    $writer = New-Object System.IO.StreamWriter($client, $encoding)
    $writer.AutoFlush = $true
    $reader = New-Object System.IO.StreamReader($client, $encoding)
    $writer.WriteLine($JsonBody)
    $response = $reader.ReadLine()
    $client.Dispose()
    return $response
}

# "log" never touches the pipe - it just tails the app's own trace file.
if ($Command -eq "log") {
    $logPath = Join-Path $ExeDir "app_trace.log"
    if (-not (Test-Path $logPath)) {
        Write-Output "no app_trace.log at $logPath"
        exit 1
    }
    Get-Content $logPath -Tail $Lines
    exit 0
}

$request = @{ cmd = $Command }
switch ($Command) {
    "shot" {
        if ($Path) { $request.path = $Path }
    }
    "dump" {
        $request.maxDepth = $MaxDepth
        if ($Filter) { $request.filter = $Filter }
    }
    "props" {
        $request.object = $Object
        if ($Filter) { $request.filter = $Filter }
    }
    "navigate" {
        if ($Tab -lt 0) { Write-Output "ERROR: -Tab required"; exit 1 }
        $request.tab = $Tab
    }
    "click" {
        if (-not $Name) { Write-Output "ERROR: -Name required"; exit 1 }
        $request.name = $Name
    }
    "invoke" {
        if (-not $Method) { Write-Output "ERROR: -Method required"; exit 1 }
        $request.object = $Object
        $request.method = $Method
        # Best-effort typing: numbers and true/false become JSON number/bool,
        # everything else stays a JSON string.
        $request.args = @($MethodArgs | ForEach-Object {
            if ($_ -match '^-?\d+(\.\d+)?$') { [double]$_ }
            elseif ($_ -eq 'true') { $true }
            elseif ($_ -eq 'false') { $false }
            else { $_ }
        })
    }
}

$body = $request | ConvertTo-Json -Compress -Depth 10
if ($Command -eq "invoke" -and $ArgsJson) {
    # Validate, but inject the caller's text as-is: a round trip through
    # PowerShell objects would unroll one-element arrays and flatten maps.
    try { $parsed = ConvertFrom-Json $ArgsJson } catch { Write-Output "ERROR: -ArgsJson is not valid JSON"; exit 1 }
    if (-not $ArgsJson.TrimStart().StartsWith("[")) { Write-Output "ERROR: -ArgsJson must be a JSON array"; exit 1 }
    $request.Remove("args")
    $body = ($request | ConvertTo-Json -Compress -Depth 10).TrimEnd("}") + ',"args":' + $ArgsJson.Trim() + "}"
}
$response = Send-BridgeCommand $body
if ($null -eq $response) { exit 1 }
Write-Output $response
