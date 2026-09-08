param(
    [Parameter(Mandatory = $true)]
    [string]$PluginPath
)

$ErrorActionPreference = 'Stop'

function Read-U16([byte[]]$Bytes, [int]$Offset) {
    return [BitConverter]::ToUInt16($Bytes, $Offset)
}

function Read-U32([byte[]]$Bytes, [int]$Offset) {
    return [BitConverter]::ToUInt32($Bytes, $Offset)
}

function Read-Ascii([byte[]]$Bytes, [int]$Offset) {
    $end = $Offset
    while ($end -lt $Bytes.Length -and $Bytes[$end] -ne 0) {
        $end++
    }
    return [Text.Encoding]::ASCII.GetString($Bytes, $Offset, $end - $Offset)
}

function Resolve-Rva([byte[]]$Bytes, $Sections, [uint32]$Rva) {
    foreach ($section in $Sections) {
        $span = [Math]::Max([uint32]$section.VirtualSize, [uint32]$section.RawSize)
        if ($Rva -ge $section.VirtualAddress -and
            $Rva -lt ($section.VirtualAddress + $span)) {
            return [int]($section.RawPointer + ($Rva - $section.VirtualAddress))
        }
    }
    return -1
}

if (-not (Test-Path -LiteralPath $PluginPath -PathType Leaf)) {
    throw "Plugin not found: $PluginPath"
}

$bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $PluginPath))
if ($bytes.Length -lt 0x100 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
    throw 'Not a PE file'
}

$pe = [int](Read-U32 $bytes 0x3c)
if ($pe -lt 0 -or $pe + 0x100 -gt $bytes.Length -or
    $bytes[$pe] -ne 0x50 -or $bytes[$pe + 1] -ne 0x45) {
    throw 'Invalid PE header'
}

$machine = Read-U16 $bytes ($pe + 4)
$sectionCount = Read-U16 $bytes ($pe + 6)
$optionalSize = Read-U16 $bytes ($pe + 20)
$optional = $pe + 24
$magic = Read-U16 $bytes $optional
if ($magic -ne 0x010b) {
    throw ('Expected PE32 optional header, got 0x{0:X4}' -f $magic)
}

$dataDirectory = $optional + 96
$sectionTable = $optional + $optionalSize
$sections = @()
for ($index = 0; $index -lt $sectionCount; $index++) {
    $offset = $sectionTable + ($index * 40)
    $sections += [PSCustomObject]@{
        VirtualSize = Read-U32 $bytes ($offset + 8)
        VirtualAddress = Read-U32 $bytes ($offset + 12)
        RawSize = Read-U32 $bytes ($offset + 16)
        RawPointer = Read-U32 $bytes ($offset + 20)
    }
}

$exportRva = Read-U32 $bytes $dataDirectory
$exportOffset = Resolve-Rva $bytes $sections $exportRva
if ($exportOffset -lt 0) {
    throw 'Export directory missing'
}

$nameCount = Read-U32 $bytes ($exportOffset + 24)
$namesRva = Read-U32 $bytes ($exportOffset + 32)
$namesOffset = Resolve-Rva $bytes $sections $namesRva
$exports = @()
for ($index = 0; $index -lt $nameCount; $index++) {
    $nameRva = Read-U32 $bytes ($namesOffset + ($index * 4))
    $nameOffset = Resolve-Rva $bytes $sections $nameRva
    if ($nameOffset -ge 0) {
        $exports += Read-Ascii $bytes $nameOffset
    }
}

$importRva = Read-U32 $bytes ($dataDirectory + 8)
$importOffset = Resolve-Rva $bytes $sections $importRva
$imports = @()
while ($importOffset -ge 0 -and (Read-U32 $bytes $importOffset) -ne 0) {
    $nameRva = Read-U32 $bytes ($importOffset + 12)
    $nameOffset = Resolve-Rva $bytes $sections $nameRva
    if ($nameOffset -ge 0) {
        $imports += Read-Ascii $bytes $nameOffset
    }
    $importOffset += 20
}

$machineText = '0x{0:X4}' -f $machine
Write-Host "Machine: $machineText"
Write-Host ("Optional header: 0x{0:X4}" -f $magic)
Write-Host "Exports: $($exports -join ', ')"
Write-Host "Imports: $($imports -join ', ')"

if ($machine -ne 0x014c) {
    throw "Expected x86 PE machine 0x014C, got $machineText"
}
if ($exports -notcontains 'GetL4NPluginInstance') {
    throw 'GetL4NPluginInstance export missing'
}
if ($imports | Where-Object { $_ -match '(?i)AfxHook|HLAE' }) {
    throw 'Plugin has a static HLAE import; HLAE must be loaded dynamically'
}
$openExrImports = @(
    'Iex-3_3.dll',
    'IlmThread-3_3.dll',
    'Imath-3_1.dll',
    'OpenEXR-3_3.dll',
    'OpenEXRCore-3_3.dll'
)
$externalOpenExr = $imports | Where-Object {
    $openExrImports -contains $_
}
if ($externalOpenExr) {
    throw "Plugin has external OpenEXR imports: $($externalOpenExr -join ', ')"
}

Write-Host 'Plugin ABI checks passed.' -ForegroundColor Green
