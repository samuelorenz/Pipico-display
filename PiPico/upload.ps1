param([string]$Uf2, [string]$Port = "")

# Trova da solo la COM del Pico (VID 2E8A): il numero cambia a ogni ricollegamento
if (-not $Port) {
  $dev = Get-CimInstance Win32_PnPEntity | Where-Object { $_.PNPDeviceID -like 'USB\VID_2E8A*' -and $_.Name -match '\((COM\d+)\)' } | Select-Object -First 1
  if ($dev) { $Port = $Matches[1] }
}

# pc_stats.py e la GUI tengono aperta la seriale: li fermo per liberarla e li rilancio dopo l'upload
$relaunch = @{}
Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'pc_stats\.py|display_gui\.py' -and $_.Name -match 'python' } | ForEach-Object {
  $name = if ($_.CommandLine -match 'display_gui') { 'display_gui.py' } else { 'pc_stats.py' }
  if (-not $relaunch.ContainsKey($name)) { $relaunch[$name] = $_.ExecutablePath }
  Write-Host "Fermo $name (PID $($_.ProcessId)) per liberare la seriale: lo rilancio dopo l'upload."
  Stop-Process -Id $_.ProcessId -Force
}

# Se il Pico non e' gia' in BOOTSEL, lo riavvia aprendo/chiudendo la seriale a 1200 baud
if (-not (Get-Volume | Where-Object FileSystemLabel -eq 'RPI-RP2')) {
  if (-not $Port) {
    Write-Host "Nessuna porta seriale del Pico trovata: provo ad aspettare il bootloader."
  } else {
    try {
      $p = New-Object System.IO.Ports.SerialPort $Port,1200
      $p.Open(); Start-Sleep -Milliseconds 200; $p.Close()
    } catch {
      # Normale: il Pico si riavvia in BOOTSEL appena la porta si apre a 1200 baud, quindi .NET
      # non riesce a completare l'apertura ("dispositivo inesistente"). Se invece la porta e'
      # davvero occupata, il volume RPI-RP2 non compare e l'errore e' segnalato piu' sotto.
      if ($_.Exception.Message -notmatch 'inesistente|does not exist') {
        Write-Host "ATTENZIONE: impossibile aprire $Port ($($_.Exception.Message)). Se e' aperto un Serial Monitor chiudilo."
      }
    }
  }
}

# Aspetta l'unita' RPI-RP2 (max 10 secondi)
$v = $null
for ($i = 0; $i -lt 50 -and -not $v; $i++) {
  Start-Sleep -Milliseconds 200
  $v = Get-Volume | Where-Object FileSystemLabel -eq 'RPI-RP2'
}
if (-not $v) { Write-Error "Unita' RPI-RP2 non trovata. Chiudi il Serial Monitor oppure tieni premuto BOOTSEL e ricollega il cavo."; exit 1 }

Copy-Item $Uf2 "$($v.DriveLetter):\"
Write-Host "Caricato su $($v.DriveLetter):"

# Rilancia quello che avevo fermato, quando il Pico e' ripartito (la GUI chiude pc_stats e lo ripristina quando esce)
if ($relaunch.Count -gt 0) {
  Start-Sleep -Seconds 6
  foreach ($name in $relaunch.Keys) {
    $style = if ($name -eq 'pc_stats.py') { 'Hidden' } else { 'Normal' }
    Start-Process $relaunch[$name] -ArgumentList "tools\$name" -WorkingDirectory $PSScriptRoot -WindowStyle $style
    Write-Host "Rilanciato $name"
  }
}
