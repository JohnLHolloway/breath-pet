param(
    [ValidateSet('build','upload','monitor','test')][string]$Action = 'build',
    [string]$Port = 'COM3',
    [switch]$ResetDemoData
)
$ErrorActionPreference = 'Stop'
$runtime = Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $runtime)) {
    python -m venv (Join-Path $PSScriptRoot '.venv')
    if ($LASTEXITCODE -ne 0) { throw 'Python environment setup failed' }
    & $runtime -m pip install -r (Join-Path $PSScriptRoot 'requirements-dev.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Development tools installation failed' }
}
Push-Location $PSScriptRoot
try {
    switch ($Action) {
        'build' { & $runtime -m platformio run }
        'upload' { & $runtime -m platformio run --target upload --upload-port $Port }
        'monitor' { & $runtime -m platformio device monitor --port $Port --baud 115200 }
        'test' {
            # Tests use their own NVS namespace. Always restore the normal firmware.
            try {
                & $runtime -m platformio run -e device-test --target upload --upload-port $Port
                if ($LASTEXITCODE -ne 0) { throw 'Test firmware upload failed' }
                & $runtime tools\test_device.py --port $Port --reset-demo-data
                if ($LASTEXITCODE -ne 0) { throw 'Device tests failed' }
            } finally {
                & $runtime -m platformio run -e breath-pet --target upload --upload-port $Port
                if ($LASTEXITCODE -ne 0) { throw 'Restore normal firmware failed; retry dev.ps1 upload' }
            }
        }
    }
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code $LASTEXITCODE" }
} finally { Pop-Location }
