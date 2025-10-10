# Biospheres Distribution Packaging Script
# This script creates a distributable package of the Biospheres application

param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [switch]$IncrementVersion = $false,
    [ValidateSet("major", "minor", "patch")]
    [string]$VersionType = "patch"
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Biospheres Distribution Packager" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Increment version if requested
if ($IncrementVersion) {
    Write-Host "Incrementing version ($VersionType)..." -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot "increment_version.ps1") -IncrementType $VersionType
    Write-Host ""
}

# Read current version
$versionFile = Join-Path $PSScriptRoot "version.txt"
$version = "unknown"
$fullVersion = "unknown"
if (Test-Path $versionFile) {
    $version = (Get-Content $versionFile -Raw).Trim()
    
    # Check if this is an alpha version (0.x.x)
    if ($version -match '^0\.') {
        $fullVersion = "$version-alpha"
        Write-Host "Version: $fullVersion" -ForegroundColor Yellow
        Write-Host "Status:  Early Alpha" -ForegroundColor Yellow
    } else {
        $fullVersion = $version
        Write-Host "Version: $version" -ForegroundColor Green
    }
    Write-Host ""
}

# Define paths
$projectRoot = $PSScriptRoot
$buildDir = Join-Path $projectRoot "$Platform\$Configuration"
$distDir = Join-Path $projectRoot "Distribution"
$exeName = "Biospheres.exe"

# Check if build exists
if (-not (Test-Path (Join-Path $buildDir $exeName))) {
    Write-Host "ERROR: Build not found at $buildDir" -ForegroundColor Red
    Write-Host "Please build the project in $Configuration mode first." -ForegroundColor Yellow
    Write-Host "In Visual Studio: Set configuration to '$Configuration' and platform to '$Platform', then build." -ForegroundColor Yellow
    exit 1
}

Write-Host "Build directory: $buildDir" -ForegroundColor Green
Write-Host "Distribution directory: $distDir" -ForegroundColor Green
Write-Host ""

# Clean and create distribution directory
if (Test-Path $distDir) {
    Write-Host "Cleaning existing distribution folder..." -ForegroundColor Yellow
    Remove-Item $distDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

Write-Host "Copying files..." -ForegroundColor Cyan
Write-Host ""

# Copy executable
Write-Host "  [1/4] Copying executable..." -ForegroundColor White
Copy-Item (Join-Path $buildDir $exeName) -Destination $distDir
Write-Host "        ✓ $exeName" -ForegroundColor Green

# Copy shaders
Write-Host "  [2/4] Copying shaders..." -ForegroundColor White
$shadersSource = Join-Path $projectRoot "shaders"
$shadersDestination = Join-Path $distDir "shaders"
if (Test-Path $shadersSource) {
    Copy-Item $shadersSource -Destination $shadersDestination -Recurse -Force
    $shaderCount = (Get-ChildItem -Path $shadersDestination -Recurse -File).Count
    Write-Host "        ✓ Copied $shaderCount shader files" -ForegroundColor Green
} else {
    Write-Host "        ⚠ Shaders folder not found" -ForegroundColor Yellow
}

# Copy DLLs (if any exist in the build directory)
Write-Host "  [3/4] Checking for DLL dependencies..." -ForegroundColor White
$dlls = Get-ChildItem -Path $buildDir -Filter "*.dll" -ErrorAction SilentlyContinue
if ($dlls) {
    foreach ($dll in $dlls) {
        Copy-Item $dll.FullName -Destination $distDir
        Write-Host "        ✓ $($dll.Name)" -ForegroundColor Green
    }
} else {
    Write-Host "        ✓ No DLL dependencies found (statically linked)" -ForegroundColor Green
}

# Copy additional resources
Write-Host "  [4/4] Copying additional resources..." -ForegroundColor White

# Copy README if it exists
$readmePath = Join-Path $projectRoot "README.md"
if (Test-Path $readmePath) {
    Copy-Item $readmePath -Destination $distDir
    Write-Host "        ✓ README.md" -ForegroundColor Green
}

# Copy LICENSE if it exists
$licensePath = Join-Path $projectRoot "LICENSE"
if (Test-Path $licensePath) {
    Copy-Item $licensePath -Destination $distDir
    Write-Host "        ✓ LICENSE" -ForegroundColor Green
}

# Create genomes folder (empty, will be populated by user)
$genomesDir = Join-Path $distDir "genomes"
New-Item -ItemType Directory -Force -Path $genomesDir | Out-Null

# Copy genomes README if it exists
$genomesReadmePath = Join-Path $projectRoot "genomes\README.md"
if (Test-Path $genomesReadmePath) {
    Copy-Item $genomesReadmePath -Destination $genomesDir
    Write-Host "        ✓ genomes/README.md" -ForegroundColor Green
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Package created successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Distribution location: $distDir" -ForegroundColor White
Write-Host ""

# Get total size
$totalSize = (Get-ChildItem -Path $distDir -Recurse | Measure-Object -Property Length -Sum).Sum
$sizeMB = [math]::Round($totalSize / 1MB, 2)
Write-Host "Total package size: $sizeMB MB" -ForegroundColor White

# Count files
$fileCount = (Get-ChildItem -Path $distDir -Recurse -File).Count
Write-Host "Total files: $fileCount" -ForegroundColor White
Write-Host ""

# Create a ZIP archive
Write-Host "Creating ZIP archive..." -ForegroundColor Cyan
$zipFilename = "Biospheres_v${fullVersion}_$Configuration.zip"
$zipPath = Join-Path $projectRoot $zipFilename
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Compress-Archive -Path "$distDir\*" -DestinationPath $zipPath -CompressionLevel Optimal
Write-Host "✓ ZIP archive created: $zipFilename" -ForegroundColor Green
$zipSize = [math]::Round((Get-Item $zipPath).Length / 1MB, 2)
Write-Host "  Archive size: $zipSize MB" -ForegroundColor White
Write-Host ""

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Test the executable in the Distribution folder" -ForegroundColor White
Write-Host "  2. Share the ZIP file: $zipFilename" -ForegroundColor White
Write-Host "  3. Users should extract and run $exeName" -ForegroundColor White
Write-Host "========================================" -ForegroundColor Cyan
