# 极简 Intel HEX -> 裸 bin 转换器(纯 PowerShell, 不依赖 python/fromelf)
# 用来把 Keil 编译出的 .hex 转成 Bootloader OTA 测试要用的裸二进制负载
#
# 用法: powershell -ExecutionPolicy Bypass -File hex2bin.ps1 <输入.hex> <输出.bin>

param(
    [Parameter(Mandatory=$true)][string]$HexPath,
    [Parameter(Mandatory=$true)][string]$BinPath
)

$ErrorActionPreference = "Stop"

$data = @{}
$extAddr = 0

foreach ($line in Get-Content $HexPath) {
    $line = $line.Trim()
    if (-not $line.StartsWith(":")) { continue }

    $byteCount = [Convert]::ToInt32($line.Substring(1, 2), 16)
    $addr      = [Convert]::ToInt32($line.Substring(3, 4), 16)
    $recType   = [Convert]::ToInt32($line.Substring(7, 2), 16)
    $payloadHex = $line.Substring(9, $byteCount * 2)

    if ($recType -eq 0) {
        $base = $extAddr + $addr
        for ($i = 0; $i -lt $byteCount; $i++) {
            $b = [Convert]::ToInt32($payloadHex.Substring($i * 2, 2), 16)
            $data[$base + $i] = [byte]$b
        }
    }
    elseif ($recType -eq 4) {
        $extAddr = [Convert]::ToInt32($payloadHex, 16) -shl 16
    }
    elseif ($recType -eq 1) {
        break
    }
}

if ($data.Count -eq 0) {
    throw "hex 文件里没解析到任何数据记录"
}

$lo = ($data.Keys | Measure-Object -Minimum).Minimum
$hi = ($data.Keys | Measure-Object -Maximum).Maximum
$size = $hi - $lo + 1

$buf = New-Object byte[] $size
for ($i = 0; $i -lt $size; $i++) { $buf[$i] = 0xFF }
foreach ($addr in $data.Keys) {
    $buf[$addr - $lo] = $data[$addr]
}

[System.IO.File]::WriteAllBytes($BinPath, $buf)

Write-Output ("起始地址: 0x{0:X8}" -f $lo)
Write-Output ("结束地址: 0x{0:X8}" -f $hi)
Write-Output ("裸 bin 大小: $size 字节 ({0:N1} KB)" -f ($size / 1024))
