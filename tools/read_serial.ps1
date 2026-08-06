<#
Reads the board's serial output for a few seconds and prints it, then exits.

Two reasons this exists rather than `arduino-cli monitor`:

  1. The monitor is interactive and never returns, so it cannot be captured by a
     script or used in an automated check.
  2. .NET's SerialPort leaves DTR deasserted by default. The Nano 33 BLE's native
     USB CDC reads that as "no host attached", so `while (!Serial)` never
     completes and the port looks completely silent even though the board is
     running fine. Asserting DTR is what makes it talk.

Usage:
  powershell -ExecutionPolicy Bypass -File tools/read_serial.ps1
  powershell -ExecutionPolicy Bypass -File tools/read_serial.ps1 -Seconds 20
  powershell -ExecutionPolicy Bypass -File tools/read_serial.ps1 -Port COM5 -Seconds 5

Only one program may hold the port. Close the Arduino IDE's Serial Monitor first.
#>
param(
  [string]$Port    = 'COM4',
  [int]   $Baud    = 115200,
  [int]   $Seconds = 6,
  [int]   $WaitFor = 20      # seconds to wait for the port to appear after an upload
)

$deadline = (Get-Date).AddSeconds($WaitFor)
$sp = $null

while ((Get-Date) -lt $deadline) {
  try {
    $sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
    $sp.DtrEnable  = $true
    $sp.RtsEnable  = $true
    $sp.ReadTimeout = 500
    $sp.Open()
    break
  } catch {
    $sp = $null
    Start-Sleep -Milliseconds 150
  }
}

if ($null -eq $sp) {
  Write-Error "Could not open $Port within $WaitFor s. Is a serial monitor already holding it?"
  exit 1
}

Start-Sleep -Milliseconds 300

$stop = (Get-Date).AddSeconds($Seconds)
$sb   = New-Object System.Text.StringBuilder
while ((Get-Date) -lt $stop) {
  try { [void]$sb.Append($sp.ReadExisting()) } catch { }
  Start-Sleep -Milliseconds 100
}
$sp.Close()

Write-Output $sb.ToString()
