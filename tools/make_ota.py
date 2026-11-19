#!/usr/bin/env python3
"""
Telink .bin → Zigbee OTA(.zigbee) 변환 (ZCL OTA 표준 헤더 + Upgrade Image 서브요소)

사용:
  python make_ota.py juntekBMS_8258.bin
  python make_ota.py juntekBMS_8258.bin --image-type 0x034A --version 0x10023001

기본값은 juntekBMS version_cfg.h 기준:
  MANUFACTURER_CODE_TELINK 0x1141
  IMAGE_TYPE  = (CHIP_TYPE << 8) | 0x4A   (CHIP_TYPE: TLSR_8258_1M)
  FILE_VERSION = 0x10023001 (APP_RELEASE 0x10, APP_BUILD 0x02, STACK 0x30 0x01)
※ IMAGE_TYPE 은 펌웨어와 반드시 같아야 함 — Z2M debug 로그의 장치 imageType 으로 확인 후 --image-type 지정
"""
import argparse
import struct
import sys
from pathlib import Path


def build(bin_data: bytes, manuf: int, image_type: int, version: int, text: str) -> bytes:
    hdr_len = 56
    sub = struct.pack('<HI', 0x0000, len(bin_data)) + bin_data   # tag 0x0000 = Upgrade Image
    total = hdr_len + len(sub)
    name = text.encode('ascii', 'replace')[:32].ljust(32, b'\x00')
    header = struct.pack(
        '<IHHHHHIH32sI',
        0x0BEEF11E,      # OTA Upgrade File Identifier
        0x0100,          # Header Version
        hdr_len,         # Header Length
        0x0000,          # Field Control (옵션 필드 없음)
        manuf,           # Manufacturer Code
        image_type,      # Image Type
        version,         # File Version
        0x0002,          # Zigbee Stack Version (Zigbee PRO)
        name,            # Header String
        total,           # Total Image Size
    )
    assert len(header) == hdr_len
    return header + sub


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('bin')
    ap.add_argument('--manuf', default='0x1141')
    ap.add_argument('--image-type', default='0x034A')
    ap.add_argument('--version', default='0x10023001')
    ap.add_argument('--text', default='DODO juntekBMS')
    ap.add_argument('-o', '--out')
    a = ap.parse_args()

    src = Path(a.bin)
    data = src.read_bytes()
    if len(data) < 16 or data[8:12] != b'KNLT':
        print('경고: Telink 펌웨어 표식(KNLT, offset 8)이 없습니다. .bin 파일이 맞는지 확인하세요.', file=sys.stderr)

    manuf, itype, ver = int(a.manuf, 0), int(a.image_type, 0), int(a.version, 0)
    out = Path(a.out) if a.out else src.with_name(f'{manuf:04X}-{itype:04X}-{ver:08X}-{src.stem}.zigbee')
    out.write_bytes(build(data, manuf, itype, ver, a.text))
    print(f'생성: {out}  ({out.stat().st_size} bytes)')
    print(f'  manufacturerCode=0x{manuf:04X}  imageType=0x{itype:04X}  fileVersion=0x{ver:08X} ({ver})')


if __name__ == '__main__':
    main()
