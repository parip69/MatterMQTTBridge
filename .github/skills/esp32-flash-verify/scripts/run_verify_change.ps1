# Projektinterner Wrapper fuer den deterministischen Aufruf von tools/verify_change.py.
# Bevorzugt die Workspace-Python-Umgebung und reicht alle weiteren Argumente unveraendert weiter.

param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ForwardArgs
)

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\\..\\..\\..")
$verifyScript = Join-Path $repoRoot "tools\\verify_change.py"
$venvPython = Join-Path $repoRoot ".venv\\Scripts\\python.exe"

if (-not (Test-Path $verifyScript)) {
    Write-Error "tools\\verify_change.py wurde nicht gefunden."
    exit 1
}

if (Test-Path $venvPython) {
    & $venvPython $verifyScript @ForwardArgs
    exit $LASTEXITCODE
}

if (Get-Command python -ErrorAction SilentlyContinue) {
    & python $verifyScript @ForwardArgs
    exit $LASTEXITCODE
}

if (Get-Command py -ErrorAction SilentlyContinue) {
    & py -3 $verifyScript @ForwardArgs
    exit $LASTEXITCODE
}

Write-Error "Es wurde weder .venv\\Scripts\\python.exe noch python/py im PATH gefunden."
exit 1
