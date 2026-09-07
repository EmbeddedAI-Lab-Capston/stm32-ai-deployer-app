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
      .\tools\uiprobe.ps1 log -Lines 30
      .\tools\uiprobe.ps1 quit
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("ping", "shot", "dump", "props", "navigate", "click", "quit", "log")]
    [string]$Command,

    [string]$Path,
    [int]$Tab = -1,
    [string]$Object = "appState",
    [string]$Name,
    [string]$Filter,
    [int]$MaxDepth = 12,
    [int]$Lines = 40,
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
}

$response = Send-BridgeCommand ($request | ConvertTo-Json -Compress)
if ($null -eq $response) { exit 1 }
Write-Output $response
