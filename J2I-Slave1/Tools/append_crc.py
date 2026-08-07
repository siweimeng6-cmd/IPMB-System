#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
给 bin 文件末尾补上 ipmudtool 要求的 CRC32 校验尾巴, 让它通过 ipmudtool 里
"Check BMC image valid/invalid" 这一步本地校验。

算法从 ipmudtool 反汇编还原(函数名就叫 CalculateCRC32, 地址0x408724):
  1. 整个文件按4字节一组, 每组内部字节反转(word byte-swap)
  2. 对反转后的数据算 CRC-32/MPEG-2 (多项式0x04C11DB7, 初值0xFFFFFFFF,
     不反转输入输出, 无最终异或) —— 这是标准变体, 表已经跟提取出来的
     256项表核对过完全一致, 不是猜的
  3. ipmudtool 要求这个 CRC 结果必须等于 0 才算 "valid"
  4. 让它等于0的办法: 在文件末尾按小端序追加4字节 = 原始数据(未加尾巴时)
     算出来的 CRC 值本身(已经用真实 app_latest.bin 验证过, 补上之后
     重新按同样算法算一遍确实等于0)

用法:
    python append_crc.py <输入.bin> <输出.bin>
"""
import sys


def gen_table_msb(poly: int):
    tbl = []
    for i in range(256):
        c = i << 24
        for _ in range(8):
            if c & 0x80000000:
                c = ((c << 1) ^ poly) & 0xFFFFFFFF
            else:
                c = (c << 1) & 0xFFFFFFFF
        tbl.append(c)
    return tbl


TABLE = gen_table_msb(0x04C11DB7)


def crc_step(crc: int, byte: int) -> int:
    idx = ((crc >> 24) ^ byte) & 0xFF
    return ((crc << 8) & 0xFFFFFFFF) ^ TABLE[idx]


def crc32_mpeg2(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for b in data:
        crc = crc_step(crc, b)
    return crc


def word_bswap(data: bytes) -> bytes:
    assert len(data) % 4 == 0, "文件大小必须是4字节的整数倍"
    out = bytearray(len(data))
    for i in range(0, len(data), 4):
        out[i:i + 4] = data[i:i + 4][::-1]
    return bytes(out)


def append_crc(in_path: str, out_path: str):
    with open(in_path, "rb") as f:
        raw = f.read()

    if len(raw) % 4 != 0:
        pad = 4 - (len(raw) % 4)
        print(f"警告: 文件大小 {len(raw)} 不是4字节整数倍, 末尾补 {pad} 个 0x00 字节对齐")
        raw = raw + b"\x00" * pad

    swapped = word_bswap(raw)
    crc1 = crc32_mpeg2(swapped)
    trailer = crc1.to_bytes(4, "little")

    out_data = raw + trailer

    # 自检: 用同样算法验证补完之后确实能通过 "valid" 判定 (结果应为0)
    final_crc = crc32_mpeg2(word_bswap(out_data))

    with open(out_path, "wb") as f:
        f.write(out_data)

    print(f"输入: {in_path} ({len(raw)} 字节)")
    print(f"追加校验尾巴: {trailer.hex(' ')} (小端序, 值=0x{crc1:08X})")
    print(f"输出: {out_path} ({len(out_data)} 字节)")
    print(f"自检最终CRC: 0x{final_crc:08X}  {'PASS(应为0)' if final_crc == 0 else 'FAIL——算法有问题, 不要用这个文件!'}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"用法: {sys.argv[0]} <输入.bin> <输出.bin>")
        sys.exit(1)
    append_crc(sys.argv[1], sys.argv[2])
