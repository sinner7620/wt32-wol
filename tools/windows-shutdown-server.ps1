param(
  [int]$Port = 8765,
  [switch]$NoShutdown
)

$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Any, $Port)
$listener.Start()

Write-Host "WT32 shutdown helper listening on 0.0.0.0:$Port"
Write-Host "Status:   http://<pc-ip>:$Port/status"
Write-Host "Shutdown: http://<pc-ip>:$Port/shutdown"

try {
  while ($true) {
    $client = $listener.AcceptTcpClient()
    $stream = $client.GetStream()
    $reader = [System.IO.StreamReader]::new($stream, [System.Text.Encoding]::ASCII)

    $requestLine = $reader.ReadLine()
    while (($line = $reader.ReadLine()) -ne $null -and $line.Length -gt 0) {
      # Drain request headers.
    }

    $path = "/"
    if ($requestLine -match '^\S+\s+([^\s?]+)') {
      $path = $matches[1].ToLowerInvariant()
    }

    $responseText = "not found"
    $statusCode = 404

    if ($path -eq "/status") {
      $responseText = "online"
      $statusCode = 200
    } elseif ($path -eq "/shutdown") {
      $responseText = "shutdown accepted"
      $statusCode = 200

      if (-not $NoShutdown) {
        Start-Process -FilePath "shutdown.exe" -ArgumentList "/s /t 3"
      }
    }

    $statusText = if ($statusCode -eq 200) { "OK" } else { "Not Found" }
    $body = [System.Text.Encoding]::UTF8.GetBytes($responseText)
    $headerText = "HTTP/1.1 $statusCode $statusText`r`nContent-Type: text/plain; charset=utf-8`r`nContent-Length: $($body.Length)`r`nConnection: close`r`n`r`n"
    $headers = [System.Text.Encoding]::ASCII.GetBytes($headerText)

    $stream.Write($headers, 0, $headers.Length)
    $stream.Write($body, 0, $body.Length)
    $stream.Close()
    $client.Close()
  }
} finally {
  $listener.Stop()
}
