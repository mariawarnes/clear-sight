[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Compiler,
    [Parameter(Mandatory)][string]$Imports,
    [string]$Flags
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceDir = Join-Path $projectRoot 'Data/Scripts/Source'
$outputDir = Join-Path $projectRoot 'build/Data/Scripts'
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) {
    throw "Papyrus compiler not found: $Compiler. Install the Skyrim SE Creation Kit first."
}
if (-not (Test-Path -LiteralPath $Imports -PathType Container)) {
    throw "Complete vanilla Papyrus import directory not found: $Imports"
}
foreach ($required in @('Actor.psc', 'EffectShader.psc', 'Quest.psc', 'ReferenceAlias.psc')) {
    if (-not (Test-Path -LiteralPath (Join-Path $Imports $required))) {
        throw "Missing vanilla dependency $required in $Imports. Extract the Creation Kit script sources."
    }
}
if (-not $Flags) { $Flags = Join-Path $Imports 'TESV_Papyrus_Flags.flg' }
if (-not (Test-Path -LiteralPath $Flags -PathType Leaf)) { throw "Flags file not found: $Flags" }
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
foreach ($script in Get-ChildItem -LiteralPath $sourceDir -Filter 'HC_*.psc') {
    & $Compiler $script.FullName "-i=$sourceDir;$Imports" "-o=$outputDir" "-f=$Flags"
    if ($LASTEXITCODE -ne 0) { throw "Papyrus compilation failed for $($script.Name)" }
}
Write-Output "Compiled Papyrus files: $outputDir"
