#!/usr/bin/env python3
"""ADC_DMA 回归测试：读串口 N 秒，检查帧完整性、SEQ 连续性、上报间隔与数值一致性。

用法:
    python tools/regress.py                  # COM6，读 10 秒
    python tools/regress.py 10               # COM6，读 10 秒
    python tools/regress.py COM7 10          # 指定端口
    python tools/regress.py COM7 10 --baud 115200

退出码: 0=全部通过, 1=有异常
"""
import argparse
import sys
import time

try:
    import serial
except ImportError:
    print("缺少 pyserial: pip install pyserial")
    sys.exit(2)

HEAD  = b"\xAA\x55"
CMD_DATA = 0x20
DEFAULT_PORT = "COM6"          # 改成你自己的串口号


def crc16(data, crc=0xFFFF):
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def pop_frame(buf):
    """从 buf 里切出一个完整帧，返回 (frame, consumed)；数据不够返回 (None, 0)。"""
    while len(buf) >= 2:
        i = buf.find(HEAD)
        if i < 0:
            return None, len(buf) - 1          # 全是垃圾，留最后 1 字节
        if i > 0:
            return None, i                     # 先丢掉帧头前的垃圾
        if len(buf) < 4:
            return None, 0
        total = buf[2] + 6
        if total < 6 or total > 70:
            return None, 1                     # LEN 非法，丢 1 字节重找
        if len(buf) < total:
            return None, 0                     # 半帧，等下一批
        return bytes(buf[:total]), total
    return None, 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", nargs="?", default=None,
                    help="串口号，如 COM6；省略时用默认值")
    ap.add_argument("seconds", type=float, nargs="?", default=10.0)
    ap.add_argument("--baud", type=int, default=115200)
    a = ap.parse_args()

    port = a.port
    if port is None:
        port = DEFAULT_PORT
    elif port.replace(".", "", 1).isdigit():
        # 只给了秒数（没给端口），端口用默认值
        a.seconds = float(port)
        port = DEFAULT_PORT

    try:
        ser = serial.Serial(port, a.baud, timeout=0.05)
    except serial.SerialException as e:
        print("打开串口 %s 失败: %s" % (port, e))
        return 1

    ser.reset_input_buffer()

    buf = bytearray()
    crc_bad = 0
    junk = 0
    frames = 0
    last_seq = None
    seq_gap = 0
    intervals = []
    last_t = None
    bad_range = 0
    mv_min = 65535
    mv_max = 0
    t0 = time.perf_counter()

    while time.perf_counter() - t0 < a.seconds:
        buf += ser.read(4096)
        while True:
            fr, used = pop_frame(buf)
            if used:
                del buf[:used]
            if not fr:
                break

            if crc16(fr[:-2]) != (fr[-2] | (fr[-1] << 8)):
                crc_bad += 1
                continue

            if fr[4] != CMD_DATA:
                continue                        # 应答帧不计入统计

            frames += 1
            now = time.perf_counter()
            if last_t is not None:
                intervals.append(now - last_t)
            last_t = now

            seq = fr[3]
            if last_seq is not None and seq != ((last_seq + 1) & 0xFF):
                seq_gap += 1
            last_seq = seq

            mv   = fr[5] | (fr[6] << 8)
            vmin = fr[7] | (fr[8] << 8)
            vmax = fr[9] | (fr[10] << 8)

            mv_min = min(mv_min, mv)
            mv_max = max(mv_max, mv)
            if not (vmin <= mv <= vmax):
                bad_range += 1

    ser.close()

    print("=" * 46)
    print("帧数        : %d" % frames)
    print("CRC 错      : %d" % crc_bad)
    print("SEQ 跳变    : %d" % seq_gap)
    print("数值越界    : %d   (要求 vmin <= mv <= vmax)" % bad_range)
    if frames:
        iv = sorted(intervals)
        print("平均间隔    : %.1f ms" % (1000.0 * sum(iv) / len(iv)))
        print("间隔抖动    : %.1f ~ %.1f ms" % (1000.0 * iv[0], 1000.0 * iv[-1]))
        print("mv 范围     : %d ~ %d mV" % (mv_min, mv_max))
        print("实测帧率    : %.1f Hz" % (frames / a.seconds))
    print("=" * 46)

    ok = frames > 0 and crc_bad == 0 and seq_gap == 0 and bad_range == 0
    print("结果: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
