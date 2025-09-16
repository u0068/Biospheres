<#
.SYNOPSIS
  Replace struct definitions in shader files with canonical versions from shared_structs.glsl.

.DESCRIPTION
  Recursively walks a shader directory and replaces struct blocks in each shader file
  with matching structs from the shared file. Useful to keep GLSL struct layouts in sync.

.PARAMETER ShaderDir
  Root folder to scan for shader files. Default: ".\shaders"

.PARAMETER SharedFile
  Path to the shared structs file. Default: "$ShaderDir\shared_structs.glsl"

.PARAMETER Backup
  If specified, creates a timestamped backup of each modified shader file.

.EXAMPLE
  .\sync_structs.ps1 -ShaderDir ".\shaders" -Backup
#>

[CmdletBinding()]
param(
    [string]$ShaderDir = ".\shaders",
    [string]$SharedFile,
    [switch]$Backup
)

# Resolve default shared file if not provided
if (-not $SharedFile) {
    $SharedFile = Join-Path -Path $ShaderDir -ChildPath "shared_structs.glsl"
}

function Write-Log {
    param([string]$msg)
    Write-Host $msg
}

# Validate inputs
if (-not (Test-Path -Path $ShaderDir -PathType Container)) {
    Write-Error "Shader directory not found: $ShaderDir"
    exit 1
}
if (-not (Test-Path -Path $SharedFile -PathType Leaf)) {
    Write-Error "Shared structs file not found: $SharedFile"
    exit 1
}

# Read shared file and extract structs
$sharedText = Get-Content -Path $SharedFile -Raw -ErrorAction Stop

# Regex to capture structs: struct Name { ... };
# Singleline to allow multiline match and non-greedy (?'body' matches inner text)
$structPattern = 'struct\s+([A-Za-z_]\w*)\s*\{[\s\S]*?\};'

$sharedStructs = @{}  # name -> struct text

[System.Text.RegularExpressions.Regex]$rx = New-Object System.Text.RegularExpressions.Regex($structPattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)

$sharedMatches = $rx.Matches($sharedText)
foreach ($m in $sharedMatches) {
    $name = $m.Groups[1].Value
    $body = $m.Value
    # Normalize whitespace a bit (optional) - we'll store raw text
    $sharedStructs[$name] = $body
}

if ($sharedStructs.Count -eq 0) {
    Write-Error "No structs found in shared file: $SharedFile"
    exit 1
}

Write-Log "Found $($sharedStructs.Count) struct(s) in shared file."

# File extensions to process (add/remove as desired)
$extensions = @("*.glsl","*.vert","*.frag","*.comp","*.geom","*.tesc","*.tese")

# Collect files
$files = @()
foreach ($ext in $extensions) {
    $files += Get-ChildItem -Path $ShaderDir -Recurse -Include $ext -File -ErrorAction SilentlyContinue
}

# Exclude the shared file itself (by full path, case-insensitive)
$sharedFull = (Get-Item -Path $SharedFile).FullName
$files = $files | Where-Object { $_.FullName -ne $sharedFull }

Write-Log ("Found {0} shader files to process." -f $files.Count)

# Process each file
$summary = [System.Collections.Generic.List[string]]::new()
foreach ($file in $files) {
    try {
        $fullPath = $file.FullName
        $origText = Get-Content -Path $fullPath -Raw -ErrorAction Stop

        $changed = $false

        # replacement delegate
        $evaluator = {
            param($m)
            $structName = $m.Groups[1].Value
            if ($sharedStructs.ContainsKey($structName)) {
                $newStruct = $sharedStructs[$structName]
                if ($newStruct -ne $m.Value) {
                    $script:changed = $true
                    Write-Host ("  Replacing struct '{0}' in {1}" -f $structName, $fullPath)
                }
                return $newStruct
            } else {
                return $m.Value
            }
        }

        # Perform replacement using .NET Regex.Replace with MatchEvaluator
        $regex = New-Object System.Text.RegularExpressions.Regex($structPattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
        $script:changed = $false
        $newText = $regex.Replace($origText, $evaluator)

        if ($script:changed) {
            # Backup if requested
            if ($Backup) {
                $time = Get-Date -Format "yyyyMMdd_HHmmss"
                $bakPath = "$fullPath.bak.$time"
                Copy-Item -Path $fullPath -Destination $bakPath -ErrorAction Stop
                Write-Host ("  Backup created: {0}" -f $bakPath)
            }

            # Write out new file (preserve UTF8 encoding)
            Set-Content -Path $fullPath -Value $newText -Encoding UTF8
            $summary.Add(("Updated: {0}" -f $fullPath))
        } else {
            # no change
            # Write-Host ("  No struct replacements needed in {0}" -f $fullPath)
        }
    }
    catch {
        Write-Warning ("Failed to process {0}: {1}" -f $fullPath, $_.Exception.Message)
    }
}

Write-Host "----- Done -----"
Write-Host ("Files updated: {0}" -f $summary.Count)
if ($summary.Count -gt 0) {
    foreach ($s in $summary) { Write-Host $s }
}
