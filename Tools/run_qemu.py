#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent.parent
IMG = ROOT / "img" / "ribon.img"
OVMF_CODE = ROOT / "OVMF" / "OVMF_CODE.fd"
OVMF_VARS = ROOT / "OVMF" / "OVMF_VARS.fd"

def run(cmd):
    print(f"[RUN] {cmd}")
    subprocess.check_call(cmd, shell=True)

def main():
    run(f"qemu-system-x86_64 "
        f"-drive if=pflash,format=raw,readonly=on,file={OVMF_CODE} "
        f"-drive format=raw,file={IMG} "
        f"-m 512M")

if __name__ == "__main__":
    main()
