# Avvia pc_stats.py insieme a Windows (nascosto, senza finestra) creando un collegamento
# nella cartella Esecuzione automatica dell'utente.
#
#   powershell -ExecutionPolicy Bypass -File tools\autostart.ps1            installa
#   powershell -ExecutionPolicy Bypass -File tools\autostart.ps1 -Remove    rimuove
#   powershell -ExecutionPolicy Bypass -File tools\autostart.ps1 -Status    mostra se e' installato
param([switch]$Remove, [switch]$Status)

$lnk = Join-Path ([Environment]::GetFolderPath('Startup')) 'PiPico Display Stats.lnk'

if ($Status) {
  if (Test-Path $lnk) { "Installato: $lnk" } else { "Non installato" }
  return
}
if ($Remove) {
  if (Test-Path $lnk) { Remove-Item $lnk -Confirm:$false; "Rimosso: $lnk" } else { "Non era installato" }
  return
}

# pythonw.exe = Python senza finestra, accanto al python.exe vero (non allo shim)
$py = (python -c "import sys; print(sys.executable)").Trim()
$pyw = Join-Path (Split-Path $py) 'pythonw.exe'
if (-not (Test-Path $pyw)) { $pyw = $py }
$script = Join-Path $PSScriptRoot 'pc_stats.py'

$sh = New-Object -ComObject WScript.Shell
$s = $sh.CreateShortcut($lnk)
$s.TargetPath = $pyw
$s.Arguments = "`"$script`""
$s.WorkingDirectory = Split-Path $PSScriptRoot
$s.WindowStyle = 7   # minimizzata
$s.Description = 'Invia le statistiche del PC al Pico'
$s.Save()
"Installato: $lnk"
"  esegue: $pyw `"$script`""
"Per rimuoverlo: powershell -ExecutionPolicy Bypass -File tools\autostart.ps1 -Remove"
