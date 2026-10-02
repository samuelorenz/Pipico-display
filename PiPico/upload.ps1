param([string]$Uf2, [string]$Port = "COM6")

# Se il Pico non e' gia' in BOOTSEL, lo riavvia aprendo/chiudendo la seriale a 1200 baud
if (-not (Get-Volume | Where-Object FileSystemLabel -eq 'RPI-RP2')) {
  try {
    $p = New-Object System.IO.Ports.SerialPort $Port,1200
    $p.Open(); Start-Sleep -Milliseconds 200; $p.Close()
  } catch { Write-Host "ATTENZIONE: $Port e' occupata (Serial Monitor aperto?). Chiudilo e rilancia l'upload." }
}

# Aspetta l'unita' RPI-RP2 (max 10 secondi)
$v = $null
for ($i = 0; $i -lt 50 -and -not $v; $i++) {
  Start-Sleep -Milliseconds 200
  $v = Get-Volume | Where-Object FileSystemLabel -eq 'RPI-RP2'
}
if (-not $v) { Write-Error "Unita' RPI-RP2 non trovata. Tieni premuto BOOTSEL e ricollega il cavo."; exit 1 }

Copy-Item $Uf2 "$($v.DriveLetter):\"
Write-Host "Caricato su $($v.DriveLetter):"
