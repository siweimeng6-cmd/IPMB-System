#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
UART5(功能串口)固件升级测试脚本, 覆盖 M2(Pre/Erase/Reset握手)和 M3(Program分片)。

用法:
    pip install pyserial
    python uart5_fwupdate_test.py <串口设备名, 如 COM5 或 /dev/ttyUSB0> [pre|erase|reset|all]
    python uart5_fwupdate_test.py <串口设备名> program <bin文件路径>

字节表来自反汇编 H5K16M32004/ipmudtool 还原的协议(见开发计划文档), 跟板子上
task_fw_update.c 的 FwUpdate_HandleFrame 是同一套编解码逻辑的独立 Python 实现,
用来在没有真实 ipmudtool 环境时做端到端联调。
"""
import sys
import time

try:
    import serial
except ImportError:
    print("需要 pyserial: pip install pyserial")
    sys.exit(1)

BASICMODE_START = 0xA0
BASICMODE_STOP = 0xA5
BASICMODE_HANDSHAKE = 0xA6
BASICMODE_ESCAPE = 0xAA

ESCAPE_TABLE = {
    BASICMODE_START: 0xB0,
    BASICMODE_STOP: 0xB5,
    BASICMODE_HANDSHAKE: 0xB6,
    BASICMODE_ESCAPE: 0xBA,
    0x1B: 0x3B,
}
UNESCAPE_TABLE = {v: k for k, v in ESCAPE_TABLE.items()}


def ipmb_checksum(data):
    """2's complement checksum, 跟 IPMB_Calc_Checksum 一致"""
    return (0 - sum(data)) & 0xFF


def basicmode_encode(frame: bytes) -> bytes:
    out = bytearray([BASICMODE_START])
    for b in frame:
        if b in ESCAPE_TABLE:
            out.append(BASICMODE_ESCAPE)
            out.append(ESCAPE_TABLE[b])
        else:
            out.append(b)
    out.append(BASICMODE_STOP)
    return bytes(out)


def basicmode_decode(raw: bytes) -> bytes:
    out = bytearray()
    in_frame = False
    escape_pending = False
    for b in raw:
        if b == BASICMODE_START:
            in_frame = True
            escape_pending = False
            out = bytearray()
            continue
        if not in_frame:
            continue
        if escape_pending:
            if b not in UNESCAPE_TABLE:
                in_frame = False
                continue
            out.append(UNESCAPE_TABLE[b])
            escape_pending = False
        elif b == BASICMODE_ESCAPE:
            escape_pending = True
        elif b == BASICMODE_STOP:
            return bytes(out)
        elif b == BASICMODE_HANDSHAKE:
            continue
        else:
            out.append(b)
    return b""  # 没等到结束字节


def build_update_cmd(subcmd: int, data_extra: bytes = b"") -> bytes:
    """rsSA=0x20(BMC) netFn/rsLUN=0xD0(netFn=0x34 OEM) rqSA=0x81 rqSeq/rqLUN=0x04
    cmd=0xF0 data[0]=subcmd [+data_extra] """
    head = bytes([0x20, 0xD0])
    cs1 = ipmb_checksum(head)
    body = bytes([0x81, 0x04, 0xF0, subcmd]) + data_extra
    cs2 = ipmb_checksum(body)
    return head + bytes([cs1]) + body + bytes([cs2])


CMDS = {
    "pre":   build_update_cmd(0x01),
    "erase": build_update_cmd(0x02),
    "reset": build_update_cmd(0x05),
}

PROGRAM_PKT_LEN = 300
CHUNK_MAX = 256


def simple_checksum(data: bytes) -> int:
    """简单加法校验和(不取反), 跟板子上 task_fw_update.c 的 simple_checksum 一致"""
    return sum(data) & 0xFF


def build_program_packet(seq: int, chunk: bytes, is_last: bool) -> bytes:
    """按开发计划里的300字节 Program 分片包排布拼包, 跟 UARTR_Program(0x403adc)
    反汇编还原的字节序完全一致"""
    assert len(chunk) <= CHUNK_MAX
    pkt = bytearray(PROGRAM_PKT_LEN)
    pkt[0:7] = bytes([0x20, 0xD0, 0x10, 0x81, 0x04, 0xF0, 0x03])

    seq_bytes = bytearray(seq.to_bytes(4, "little"))
    if is_last:
        seq_bytes[3] = ((seq >> 24) & 0x0F) | 0x80
    pkt[7:11] = bytes(seq_bytes)

    pkt[11:13] = len(chunk).to_bytes(2, "little")
    pkt[13:13 + len(chunk)] = chunk
    # 13+len(chunk) 到 268 保持 0 (bytearray 初值就是0, 对应板子那边 memset 后的垃圾区)

    pkt[269] = simple_checksum(bytes(pkt[7:269]))
    # 270-298 保持 0 填充
    pkt[299] = ipmb_checksum(bytes(pkt[3:299]))
    return bytes(pkt)


def run_program(ser: serial.Serial, bin_path: str) -> bool:
    with open(bin_path, "rb") as f:
        data = f.read()
    print(f"固件文件: {bin_path}, 共 {len(data)} 字节, {(len(data) + CHUNK_MAX - 1)//CHUNK_MAX} 包")

    if not send_and_wait_ack(ser, "pre", CMDS["pre"]):
        return False
    if not send_and_wait_ack(ser, "erase", CMDS["erase"]):
        return False

    local_running_cs = 0
    seq = 0
    offset = 0
    while offset < len(data):
        chunk = data[offset:offset + CHUNK_MAX]
        is_last = (offset + len(chunk)) >= len(data)
        pkt = build_program_packet(seq, chunk, is_last)

        for b in chunk:
            local_running_cs = (local_running_cs + b) & 0xFF

        ok = send_and_wait_ack(ser, f"program seq={seq} len={len(chunk)} last={is_last}", pkt)
        if not ok:
            print(f"[FAIL] 第 {seq} 包(偏移 {offset}) 没收到合法 ack, 中止")
            return False

        seq += 1
        offset += len(chunk)

    print(f"\n本地算出的运行校验和(简单加法, 全部负载字节): 0x{local_running_cs:02X}")
    print("拿这个值跟板子 UART4 调试口最后打印的\"运行校验和\"对一下, 应该完全一致。")
    return True


BB_M0 = 0x55
BB_M1 = 0x20
BB_TAIL = 0x0D
BB_STAGE_HANDSHAKE = 0x10
BB_STAGE_ERASE = 0x11
BB_STAGE_PROGRAM = 0x12
BB_STAGE_RESTART = 0xAB


def bb_token(stage: int) -> bytes:
    return bytes([BB_M0, BB_M1, stage, BB_TAIL])


def bb_ack(stage: int) -> bytes:
    return bytes([0x80, BB_M0, BB_M1, stage, BB_TAIL])


def bb_wait_ack(ser: serial.Serial, stage: int, timeout: float) -> bool:
    """BB态裸协议没有成帧, 在 timeout 秒内只要收到的字节流里出现过期望的5字节
    ack 子串就算成功, 能容忍前后夹杂杂散字节"""
    expected = bb_ack(stage)
    deadline = time.time() + timeout
    buf = b""
    while time.time() < deadline:
        chunk = ser.read(64)
        if chunk:
            buf += chunk
            if expected in buf:
                print("BB RX: " + buf.hex(" "))
                return True
        if len(buf) > 4096:
            buf = buf[-16:]
    print("BB RX(超时前收到的全部): " + buf.hex(" "))
    return False


def run_bb_ota(ser: serial.Serial, bin_path: str) -> bool:
    with open(bin_path, "rb") as f:
        data = f.read()

    print("\n=== BB(Bootloader)态: 裸协议 ===")
    print(f"固件: {bin_path}, {len(data)} 字节")

    print("\n--- BB 握手(0x10) ---")
    ser.reset_input_buffer()
    ser.write(bb_token(BB_STAGE_HANDSHAKE))
    if not bb_wait_ack(ser, BB_STAGE_HANDSHAKE, 10):
        print("[FAIL] 没收到握手 ack, 板子是不是还没跳进 Bootloader?")
        return False
    print("[PASS] 握手")

    print("\n--- BB 擦除(0x11), 480KB App区, 可能要几秒 ---")
    ser.reset_input_buffer()
    ser.write(bb_token(BB_STAGE_ERASE))
    if not bb_wait_ack(ser, BB_STAGE_ERASE, 30):
        print("[FAIL] 没收到擦除 ack")
        return False
    print("[PASS] 擦除")

    print(f"\n--- BB 裸发送固件数据({len(data)}字节), 无成帧, 发完等 ack ---")
    ser.reset_input_buffer()
    t0 = time.time()
    ser.write(data)
    elapsed_send = time.time() - t0
    print(f"发送完成, 耗时 {elapsed_send:.1f}秒, 等待写flash完成的ack...")
    if not bb_wait_ack(ser, BB_STAGE_PROGRAM, 60):
        print("[FAIL] 没收到 program ack (可能还在写flash, 或者写失败了)")
        return False
    print("[PASS] Program(写flash完成)")

    print("\n--- BB 重启(0xAB), 手册里这一步没有应答, 发完直接等板子跳新固件 ---")
    ser.write(bb_token(BB_STAGE_RESTART))
    print("已发送重启令牌")
    return True


def run_full_ota(ser: serial.Serial, bin_path: str) -> bool:
    """完整OTA: App态 Pre/Erase/Program/Reset -> 等板子跳Bootloader -> BB态 握手/擦除/Program/重启"""
    if not run_program(ser, bin_path):
        return False
    if not send_and_wait_ack(ser, "reset", CMDS["reset"]):
        return False
    print("\n已发送 Reset, 等待板子复位跳进 Bootloader...")
    time.sleep(2)
    return run_bb_ota(ser, bin_path)


def send_and_wait_ack(ser: serial.Serial, name: str, frame: bytes):
    wire = basicmode_encode(frame)
    print(f"\n--- {name} ---")
    print("TX (raw IPMB): " + frame.hex(" "))
    print("TX (on wire ): " + wire.hex(" "))
    ser.reset_input_buffer()
    ser.write(wire)

    time.sleep(0.3)
    resp_raw = ser.read(256)
    if not resp_raw:
        print("[FAIL] 没收到任何应答(检查接线/波特率/板子是否跑到 M2 代码)")
        return False
    print("RX (raw bytes): " + resp_raw.hex(" "))

    decoded = basicmode_decode(resp_raw)
    if not decoded:
        print("[FAIL] 没能从收到的字节里解出合法 Basic Mode 帧")
        return False
    print("RX (decoded IPMB): " + decoded.hex(" "))

    if len(decoded) < 8:
        print(f"[FAIL] 应答帧太短: {len(decoded)} 字节")
        return False
    if ipmb_checksum(decoded[0:2]) != decoded[2]:
        print("[FAIL] 应答 checksum1 错误")
        return False
    if ipmb_checksum(decoded[3:-1]) != decoded[-1]:
        print("[FAIL] 应答 checksum2 错误")
        return False
    if decoded[0] != 0x81 or decoded[3] != 0x20:
        print(f"[FAIL] 地址字段不对: rsSA={decoded[0]:#x} rqSA={decoded[3]:#x}, 期望 0x81/0x20")
        return False
    if decoded[5] != 0xF0:
        print(f"[FAIL] cmd 不对: {decoded[5]:#x}, 期望 0xF0")
        return False
    cc = decoded[6]
    if cc != 0x00:
        print(f"[FAIL] completion code = {cc:#x}, 期望 0x00")
        return False

    print(f"[PASS] {name}: 收到合法 ack (cc=0x00)")
    return True


def main():
    if len(sys.argv) < 2:
        print(f"用法: {sys.argv[0]} <串口设备名> [pre|erase|reset|all]")
        print(f"      {sys.argv[0]} <串口设备名> program <bin文件路径>   (M3: 只发App态分片, 不写flash)")
        print(f"      {sys.argv[0]} <串口设备名> ota <bin文件路径>       (M6: 完整OTA, 真的会擦写flash)")
        sys.exit(1)

    port = sys.argv[1]
    which = sys.argv[2] if len(sys.argv) > 2 else "all"

    ser = serial.Serial(port, baudrate=115200, bytesize=8, parity='N',
                         stopbits=1, timeout=1)

    if which == "program":
        if len(sys.argv) < 4:
            print(f"用法: {sys.argv[0]} <串口设备名> program <bin文件路径>")
            ser.close()
            sys.exit(1)
        ok = run_program(ser, sys.argv[3])
        ser.close()
        print(f"\nprogram: {'PASS' if ok else 'FAIL'}")
        return

    if which == "ota":
        if len(sys.argv) < 4:
            print(f"用法: {sys.argv[0]} <串口设备名> ota <bin文件路径>")
            ser.close()
            sys.exit(1)
        bin_path = sys.argv[3]
        print("=" * 60)
        print("即将执行完整 OTA 升级流程: 会真的擦除并重写板子 App 区(0x08008000起,")
        print("480KB)的 flash, 过程中不能断电、不能断开这条 UART5 连接。")
        print(f"固件: {bin_path}")
        print("=" * 60)
        confirm = input("确认继续吗? 输入 yes 继续: ")
        if confirm.strip().lower() != "yes":
            print("已取消")
            ser.close()
            sys.exit(0)
        ok = run_full_ota(ser, bin_path)
        ser.close()
        print(f"\nota: {'PASS' if ok else 'FAIL'}")
        return

    names = ["pre", "erase", "reset"] if which == "all" else [which]
    results = {}
    for name in names:
        if name not in CMDS:
            print(f"未知命令: {name}")
            continue
        results[name] = send_and_wait_ack(ser, name, CMDS[name])

    ser.close()

    print("\n==== 汇总 ====")
    for name, ok in results.items():
        print(f"{name}: {'PASS' if ok else 'FAIL'}")


if __name__ == "__main__":
    main()
