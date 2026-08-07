# 给 bin 文件末尾补上 ipmudtool 要求的 CRC32 校验尾巴 (纯 PowerShell/.NET 实现,
# 用 [System.IO.File]::ReadAllBytes 读文件, 避免 Python/Bash 在这种深层特殊字符
# 路径下偶发读取不完整的问题)
#
# 算法(从 ipmudtool 反汇编还原, 函数名 CalculateCRC32):
#   1. 整个文件按4字节一组, 组内字节反转
#   2. 对反转后的数据算 CRC-32/MPEG-2 (多项式0x04C11DB7, 初值0xFFFFFFFF, 不反转,
#      无最终异或) —— 已用二进制里提取出的256项表核对过完全一致
#   3. ipmudtool 要求这个 CRC 结果必须等于 0 才算 "valid"
#   4. 在文件末尾按小端序追加4字节 = 原始数据算出来的 CRC 值本身, 能让重算后结果为0
#
# 注意: 全程用 [int64] 做移位/异或运算再 -band 0xFFFFFFFFL 掩回32位, 不直接用
# [uint32]做shl——PowerShell的 -shl 在操作数是默认[int]时会按有符号32位溢出,
# 比如 128 -shl 24 = -2147483648, 直接转 [uint32] 会报错, 所以全程用 int64 兜底。
#
# 坑2(曾导致这个脚本一直算不对, 从没成功产出过带正确尾巴的文件): 裸写
# 0xFFFFFFFF / 0x80000000 这种 >= 0x80000000 的十六进制字面量, PowerShell
# 默认按有符号 Int32 解析(分别等于 -1 和 -2147483648), 跟 [int64] 变量做
# -band/-bxor 时数值就是错的(掩码形同虚设)。必须在字面量后面加 L 后缀强制
# 按 Int64 解析(0xFFFFFFFFL 才是 4294967295), 本文件里所有这类字面量都已改。
#
# 用法: powershell -ExecutionPolicy Bypass -File Append-Crc.ps1 <输入.bin> <输出.bin>

param(
    [Parameter(Mandatory=$true)][string]$InPath,
    [Parameter(Mandatory=$true)][string]$OutPath
)

$ErrorActionPreference = "Stop"

function New-Crc32Table {
    $poly = 0x04C11DB7L
    $table = New-Object 'int64[]' 256
    for ($i = 0; $i -lt 256; $i++) {
        $c = [int64]$i -shl 24
        for ($j = 0; $j -lt 8; $j++) {
            if (($c -band 0x80000000L) -ne 0) {
                $c = (($c -shl 1) -bxor $poly) -band 0xFFFFFFFFL
            } else {
                $c = ($c -shl 1) -band 0xFFFFFFFFL
            }
        }
        $table[$i] = $c
    }
    return $table
}

function Get-Crc32Mpeg2 {
    param([byte[]]$Data, [int64[]]$Table)
    $crc = 0xFFFFFFFFL
    foreach ($b in $Data) {
        $idx = (($crc -shr 24) -bxor [int64]$b) -band 0xFFL
        $crc = (($crc -shl 8) -band 0xFFFFFFFFL) -bxor $Table[$idx]
    }
    return $crc -band 0xFFFFFFFFL
}

function Get-WordByteSwap {
    param([byte[]]$Data)
    if ($Data.Length % 4 -ne 0) {
        throw "文件大小必须是4字节的整数倍, 现在是 $($Data.Length)"
    }
    $out = New-Object byte[] $Data.Length
    for ($i = 0; $i -lt $Data.Length; $i += 4) {
        $out[$i]   = $Data[$i+3]
        $out[$i+1] = $Data[$i+2]
        $out[$i+2] = $Data[$i+1]
        $out[$i+3] = $Data[$i]
    }
    return $out
}

$table = New-Crc32Table

$raw = [System.IO.File]::ReadAllBytes($InPath)
Write-Output "输入: $InPath ($($raw.Length) 字节)"

if ($raw.Length % 4 -ne 0) {
    $pad = 4 - ($raw.Length % 4)
    Write-Output "警告: 文件大小不是4字节整数倍, 末尾补 $pad 个 0x00 字节对齐"
    $raw += [byte[]](, 0) * $pad
}

$swapped = Get-WordByteSwap -Data $raw
$crc1 = Get-Crc32Mpeg2 -Data $swapped -Table $table

$trailer = [byte[]](
    [byte]($crc1 -band 0xFF),
    [byte](($crc1 -shr 8) -band 0xFF),
    [byte](($crc1 -shr 16) -band 0xFF),
    [byte](($crc1 -shr 24) -band 0xFF)
)

$outData = $raw + $trailer

$finalCrc = Get-Crc32Mpeg2 -Data (Get-WordByteSwap -Data $outData) -Table $table

[System.IO.File]::WriteAllBytes($OutPath, $outData)

Write-Output ("追加校验尾巴: {0:X2} {1:X2} {2:X2} {3:X2} (小端序, 值=0x{4:X8})" -f $trailer[0],$trailer[1],$trailer[2],$trailer[3],$crc1)
Write-Output "输出: $OutPath ($($outData.Length) 字节)"
if ($finalCrc -eq 0) {
    Write-Output "自检最终CRC: 0x00000000  PASS"
} else {
    Write-Output ("自检最终CRC: 0x{0:X8}  FAIL——不要用这个文件!" -f $finalCrc)
}
