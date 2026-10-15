# Installa "PiPico Display Config" come un normale programma di Windows: collegamenti nel menu Start
# e sul Desktop che avviano la GUI senza finestra del terminale, con la sua icona.
#
#   powershell -ExecutionPolicy Bypass -File tools\install_app.ps1            installa
#   powershell -ExecutionPolicy Bypass -File tools\install_app.ps1 -Remove    rimuove i collegamenti
#   powershell -ExecutionPolicy Bypass -File tools\install_app.ps1 -NoDesktop  solo menu Start
param([switch]$Remove, [switch]$NoDesktop)

$name = 'PiPico Display Config.lnk'
$links = @((Join-Path ([Environment]::GetFolderPath('Programs')) $name))
if (-not $NoDesktop) { $links += (Join-Path ([Environment]::GetFolderPath('Desktop')) $name) }

if ($Remove) {
  foreach ($l in @((Join-Path ([Environment]::GetFolderPath('Programs')) $name), (Join-Path ([Environment]::GetFolderPath('Desktop')) $name))) {
    if (Test-Path $l) { Remove-Item $l -Confirm:$false; "Rimosso: $l" }
  }
  return
}

# pythonw.exe = Python senza finestra, accanto al python.exe vero (non allo shim)
$py = (python -c "import sys; print(sys.executable)").Trim()
$pyw = Join-Path (Split-Path $py) 'pythonw.exe'
if (-not (Test-Path $pyw)) { $pyw = $py }
$gui = Join-Path $PSScriptRoot 'display_gui.py'
$ico = Join-Path $PSScriptRoot 'app.ico'
if (-not (Test-Path $ico)) { python (Join-Path $PSScriptRoot 'make_icon.py') | Out-Null }

$sh = New-Object -ComObject WScript.Shell
foreach ($l in $links) {
  $s = $sh.CreateShortcut($l)
  $s.TargetPath = $pyw
  $s.Arguments = "`"$gui`""
  $s.WorkingDirectory = Split-Path $PSScriptRoot
  $s.IconLocation = $ico
  $s.Description = 'Configura il display PiPico (tema, elementi, allarmi, anteprima)'
  $s.Save()
  "Creato: $l"
}
"Avvia 'PiPico Display Config' dal menu Start o dal Desktop."
