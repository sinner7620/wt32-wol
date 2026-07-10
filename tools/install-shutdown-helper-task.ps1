param(
  [string]$TaskName = "WT32 Shutdown Helper",
  [int]$Port = 8765,
  [switch]$NoShutdown
)

$principal = [Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
  throw "Please run this script from an elevated PowerShell window."
}

$helperPath = Join-Path $PSScriptRoot "windows-shutdown-server.ps1"
if (-not (Test-Path -LiteralPath $helperPath)) {
  throw "Helper script not found: $helperPath"
}

$argumentList = @(
  "-NoProfile",
  "-ExecutionPolicy", "Bypass",
  "-File", "`"$helperPath`"",
  "-Port", $Port
)

if ($NoShutdown) {
  $argumentList += "-NoShutdown"
}

$action = New-ScheduledTaskAction `
  -Execute "powershell.exe" `
  -Argument ($argumentList -join " ")

$trigger = New-ScheduledTaskTrigger -AtStartup
$settings = New-ScheduledTaskSettingsSet `
  -AllowStartIfOnBatteries `
  -DontStopIfGoingOnBatteries `
  -ExecutionTimeLimit ([TimeSpan]::Zero) `
  -RestartCount 3 `
  -RestartInterval (New-TimeSpan -Minutes 1)

$taskPrincipal = New-ScheduledTaskPrincipal `
  -UserId "SYSTEM" `
  -LogonType ServiceAccount `
  -RunLevel Highest

Register-ScheduledTask `
  -TaskName $TaskName `
  -Action $action `
  -Trigger $trigger `
  -Settings $settings `
  -Principal $taskPrincipal `
  -Force | Out-Null

Start-ScheduledTask -TaskName $TaskName

Write-Host "Installed and started scheduled task: $TaskName"
Write-Host "Status URL: http://<pc-ip>:$Port/status"
