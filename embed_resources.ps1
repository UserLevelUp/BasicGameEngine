# Re-embed BGE_GAME_SCRIPT and BGE_FEATURE_LEDGER resources into space-rocks.exe
# using the same Win32 UpdateResource path as BGE's own EmbedUtf8TextResourceInExecutable.
param(
    [string]$ExePath = "C:\src\BasicGameEngine\exports\space-rocks\space-rocks.exe",
    [string]$CommandsPath = "C:\src\BasicGameEngine\exports\space-rocks\space-rocks.commands",
    [string]$FeaturesPath = "C:\src\BasicGameEngine\exports\space-rocks\space-rocks.features.txt"
)

Add-Type -Namespace Win32 -Name ResUpdate -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
public static extern IntPtr BeginUpdateResourceW(string pFileName, bool bDeleteExistingResources);

[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
public static extern bool UpdateResourceW(IntPtr hUpdate, IntPtr lpType, string lpName, ushort wLanguage, byte[] lpData, uint cbData);

[DllImport("kernel32.dll", SetLastError=true)]
public static extern bool EndUpdateResourceW(IntPtr hUpdate, bool fDiscard);
'@

function Embed-Resource {
    param([string]$Exe, [string]$Name, [byte[]]$Bytes)

    $hUpdate = [Win32.ResUpdate]::BeginUpdateResourceW($Exe, $false)
    if ($hUpdate -eq [IntPtr]::Zero) {
        throw "BeginUpdateResourceW failed (err=$([System.Runtime.InteropServices.Marshal]::GetLastWin32Error())) for $Exe"
    }
    $RT_RCDATA = [IntPtr]10
    $ok = [Win32.ResUpdate]::UpdateResourceW($hUpdate, $RT_RCDATA, $Name, 0, $Bytes, [uint32]$Bytes.Length)
    if (-not $ok) {
        [void][Win32.ResUpdate]::EndUpdateResourceW($hUpdate, $true)
        throw "UpdateResourceW failed (err=$([System.Runtime.InteropServices.Marshal]::GetLastWin32Error())) for $Name"
    }
    $ok = [Win32.ResUpdate]::EndUpdateResourceW($hUpdate, $false)
    if (-not $ok) {
        throw "EndUpdateResourceW failed (err=$([System.Runtime.InteropServices.Marshal]::GetLastWin32Error()))"
    }
}

if (-not (Test-Path $ExePath))      { throw "Missing exe: $ExePath" }
if (-not (Test-Path $CommandsPath)) { throw "Missing commands: $CommandsPath" }
if (-not (Test-Path $FeaturesPath)) { throw "Missing features: $FeaturesPath" }

# BGE writes UTF-8 (Narrow(text)) without BOM. Read as raw bytes to avoid surprises.
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$commandsBytes = $utf8NoBom.GetBytes([System.IO.File]::ReadAllText($CommandsPath))
$featuresBytes = $utf8NoBom.GetBytes([System.IO.File]::ReadAllText($FeaturesPath))

Write-Host "Embedding BGE_GAME_SCRIPT ($($commandsBytes.Length) bytes)..."
Embed-Resource -Exe $ExePath -Name "BGE_GAME_SCRIPT" -Bytes $commandsBytes
Write-Host "Embedding BGE_FEATURE_LEDGER ($($featuresBytes.Length) bytes)..."
Embed-Resource -Exe $ExePath -Name "BGE_FEATURE_LEDGER" -Bytes $featuresBytes

$info = Get-Item $ExePath
Write-Host "Done: $($info.FullName) (size=$($info.Length))"
