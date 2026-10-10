# Builds out\release\DBZ3Access-<version>.zip from a finished build (run build.sh first).
# The zip unpacks into the game folder, next to dbz3.exe.
param([string]$Version = "1.0")
$ErrorActionPreference = "Stop"
$src = Split-Path $PSScriptRoot
$game = Split-Path $src
$rel = Join-Path $src "out\release"
$stage = Join-Path $rel "stage"
if (Test-Path $rel) { Remove-Item -LiteralPath $rel -Recurse -Force }
New-Item -ItemType Directory -Force (Join-Path $stage "DBZ3Access\loader"), (Join-Path $stage "DBZ3Access\licenses") | Out-Null

Copy-Item -LiteralPath (Join-Path $src "out\Play DBZ3 Access.exe") $stage
foreach ($f in "out\DBZ3Access\DBZ3Access.dll", "data\text.tsv", "README.txt", "LICENSE.txt") {
    Copy-Item -LiteralPath (Join-Path $src $f) (Join-Path $stage "DBZ3Access")
}
Copy-Item -LiteralPath (Join-Path $game "DBZ3Access\prism.dll") (Join-Path $stage "DBZ3Access")
Copy-Item -LiteralPath (Join-Path $src "out\rexgpu-access.dll") (Join-Path $stage "DBZ3Access\loader\rexgpu-xenos.dll")
Copy-Item -LiteralPath (Join-Path $src "third_party\minhook\LICENSE.txt") (Join-Path $stage "DBZ3Access\licenses\MinHook-LICENSE.txt")
Copy-Item -LiteralPath (Join-Path $src "third_party\prism\LICENSE") (Join-Path $stage "DBZ3Access\licenses\Prism-LICENSE.txt")

$zip = Join-Path $rel "DBZ3Access-$Version.zip"
# Windows' tar writes standard "/" paths; Compress-Archive and ZipFile in
# PowerShell 5.1 write "\", which some unzip tools turn into odd file names.
& "$env:SystemRoot\System32\tar.exe" -a -c -f $zip -C $stage "Play DBZ3 Access.exe" DBZ3Access
if ($LASTEXITCODE -ne 0) { throw "tar failed" }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$z = [IO.Compression.ZipFile]::OpenRead($zip)
$z.Entries | ForEach-Object { "{0,-45} {1,9}" -f $_.FullName, $_.Length }
$z.Dispose()
"$zip  ($([math]::Round((Get-Item $zip).Length / 1KB)) KB)"
