#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
极简 Intel HEX -> 裸 bin 转换器(纯标准库, 不依赖 intelhex 之类的第三方包)。
用来把 Keil 编译出的 .hex 转成 M6 OTA 自升级测试要用的裸二进制负载
(Bootloader 写flash是按地址连续写字节流的, 不认HEX里的地址记录, 必须转成裸bin)。

用法: python hex2bin.py <输入.hex> <输出.bin>
"""
import sys


def hex_to_bin(hex_path, bin_path):
    data = {}
    ext_addr = 0

    with open(hex_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line.startswith(':'):
                continue
            byte_count = int(line[1:3], 16)
            addr = int(line[3:7], 16)
            rec_type = int(line[7:9], 16)
            payload_hex = line[9:9 + byte_count * 2]
            payload = bytes.fromhex(payload_hex)

            if rec_type == 0x00:  # data record
                base = ext_addr + addr
                for i, b in enumerate(payload):
                    data[base + i] = b
            elif rec_type == 0x04:  # extended linear address
                ext_addr = int(payload_hex, 16) << 16
            elif rec_type == 0x01:  # EOF
                break

    if not data:
        raise ValueError("hex 文件里没解析到任何数据记录")

    lo = min(data.keys())
    hi = max(data.keys())
    size = hi - lo + 1

    buf = bytearray([0xFF]) * size
    for addr, b in data.items():
        buf[addr - lo] = b

    with open(bin_path, 'wb') as f:
        f.write(buf)

    return lo, hi, size


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"用法: {sys.argv[0]} <输入.hex> <输出.bin>")
        sys.exit(1)

    lo, hi, size = hex_to_bin(sys.argv[1], sys.argv[2])
    print(f"起始地址: 0x{lo:08X}")
    print(f"结束地址: 0x{hi:08X}")
    print(f"裸 bin 大小: {size} 字节 ({size/1024:.1f} KB)")
