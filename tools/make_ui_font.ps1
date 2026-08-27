param(
    [Parameter(Mandatory = $true)]
    [string]$Font,

    [string]$Chars = "tools/ui_font_chars.txt",
    [string[]]$Scan = @("app/app_ui_text.h"),
    [string]$Out = "tools/ui_font_16.bin",
    [string]$ImageOut = "tools/ui_font_w25q128.img",
    [int]$Size = 16,
    [int]$Bpp = 4
)

$ErrorActionPreference = "Stop"

$HeaderAddr = 0x00050000
$DataAddr = 0x00051000
$HeaderSize = 52
$FontMagic = 0x46545731
$FontVersion = 1

function Get-Crc32 {
    param([byte[]]$Bytes)

    $crc = [uint32]::MaxValue
    $poly = [uint32]::Parse("EDB88320", [System.Globalization.NumberStyles]::HexNumber)
    foreach ($byte in $Bytes) {
        $crc = $crc -bxor [uint32]$byte
        for ($i = 0; $i -lt 8; $i++) {
            if (($crc -band 1) -ne 0) {
                $crc = ($crc -shr 1) -bxor $poly
            } else {
                $crc = $crc -shr 1
            }
        }
    }

    return ($crc -bxor [uint32]::MaxValue)
}

function Write-U16LE {
    param([System.IO.BinaryWriter]$Writer, [uint16]$Value)
    $Writer.Write([byte]($Value -band 0xFF))
    $Writer.Write([byte](($Value -shr 8) -band 0xFF))
}

function Write-U32LE {
    param([System.IO.BinaryWriter]$Writer, [uint32]$Value)
    $Writer.Write([byte]($Value -band 0xFF))
    $Writer.Write([byte](($Value -shr 8) -band 0xFF))
    $Writer.Write([byte](($Value -shr 16) -band 0xFF))
    $Writer.Write([byte](($Value -shr 24) -band 0xFF))
}

if (-not (Test-Path -LiteralPath $Font)) {
    throw "Font file not found: $Font"
}

if (-not (Test-Path -LiteralPath $Chars)) {
    throw "Character list not found: $Chars"
}

$outDir = Split-Path -Parent $Out
if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir | Out-Null
}

$imageDir = Split-Path -Parent $ImageOut
if ($imageDir -and -not (Test-Path -LiteralPath $imageDir)) {
    New-Item -ItemType Directory -Path $imageDir | Out-Null
}

$charsText = Get-Content -LiteralPath $Chars -Raw -Encoding UTF8
foreach ($scanPath in $Scan) {
    if (Test-Path -LiteralPath $scanPath) {
        $charsText += Get-Content -LiteralPath $scanPath -Raw -Encoding UTF8
    }
}
$charsText = $charsText -replace "\s", ""
if ([string]::IsNullOrWhiteSpace($charsText)) {
    throw "Character list is empty."
}

$ranges = @("0x20-0x7E")
foreach ($ch in $charsText.ToCharArray() | Sort-Object -Unique) {
    $code = [int][char]$ch
    if ($code -gt 0x7F) {
        $ranges += ("0x{0:X}" -f $code)
    }
}

$convArgs = @(
    "lv_font_conv",
    "--font", $Font,
    "--size", "$Size",
    "--bpp", "$Bpp",
    "--format", "bin",
    "--no-compress",
    "--no-prefilter",
    "--force-fast-kern-format",
    "-o", $Out
)

foreach ($range in $ranges) {
    $convArgs += @("-r", $range)
}

npx @convArgs
if ($LASTEXITCODE -ne 0) {
    throw "lv_font_conv failed with exit code $LASTEXITCODE"
}

$fontBytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Out))
$crc = Get-Crc32 $fontBytes
$nameBytes = [System.Text.Encoding]::ASCII.GetBytes("ui_font_16.bin")
$imageSize = ($DataAddr - $HeaderAddr) + $fontBytes.Length
$image = New-Object byte[] $imageSize
for ($i = 0; $i -lt $image.Length; $i++) {
    $image[$i] = 0xFF
}

$stream = New-Object System.IO.MemoryStream(,$image)
$writer = New-Object System.IO.BinaryWriter($stream)
Write-U32LE $writer ([uint32]$FontMagic)
Write-U16LE $writer ([uint16]$FontVersion)
Write-U16LE $writer ([uint16]$HeaderSize)
Write-U32LE $writer ([uint32]$DataAddr)
Write-U32LE $writer ([uint32]$fontBytes.Length)
Write-U32LE $writer ([uint32]$crc)
$writer.Write($nameBytes)
for ($i = $nameBytes.Length; $i -lt 32; $i++) {
    $writer.Write([byte]0)
}
$writer.Flush()

[Array]::Copy($fontBytes, 0, $image, ($DataAddr - $HeaderAddr), $fontBytes.Length)
[System.IO.File]::WriteAllBytes((Join-Path (Get-Location) $ImageOut), $image)

"Font:       $Out"
"Image:      $ImageOut"
"Chars:      $($ranges.Count - 1) CJK chars + ASCII"
"Font size:  $($fontBytes.Length) bytes"
"CRC32:      0x{0:X8}" -f $crc
"W25Q range: header 0x{0:X6}, data 0x{1:X6}" -f $HeaderAddr, $DataAddr
