[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
$archivePath = Join-Path $outputDir 'SkyrimHighContrast-Shader-Prototype-0.2.zip'
Add-Type -AssemblyName System.IO.Compression
$stream = [System.IO.File]::Open($archivePath, [System.IO.FileMode]::Create)
try {
    $archive = [System.IO.Compression.ZipArchive]::new($stream, [System.IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        $files = @{
            'reshade-shaders/Shaders/SkyrimHighContrast.fx' = 'reshade-shaders/Shaders/SkyrimHighContrast.fx'
            'vortex_override_instructions.json' = 'packaging/vortex_override_instructions.json'
        }
        foreach ($entryName in $files.Keys) {
            $entry = $archive.CreateEntry($entryName)
            $entryStream = $entry.Open()
            try {
                $bytes = [System.IO.File]::ReadAllBytes((Join-Path $projectRoot $files[$entryName]))
                $entryStream.Write($bytes, 0, $bytes.Length)
            } finally { $entryStream.Dispose() }
        }
    } finally { $archive.Dispose() }
} finally { $stream.Dispose() }
Write-Output $archivePath
