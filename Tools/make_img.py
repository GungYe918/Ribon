#!/usr/bin/env python3
import os
import platform
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ESP_DIR = ROOT / "esp"
IMG_DIR = ROOT / "img"
IMG_FILE = IMG_DIR / "ribon.img"


def ensure_dirs():
    IMG_DIR.mkdir(exist_ok=True)


def run(cmd):
    print(f"[RUN] {cmd}")
    subprocess.check_call(cmd, shell=True)


# ---------------------------------------------------------
#  mtools helper
# ---------------------------------------------------------

created_dirs = set()


def safe_mkdir_in_img(img_spec: str, path: str):
    """
    img_spec = 'ribon.img@@OFFSET' 같은 mtools용 이미지 지정 문자열
    path     = 'EFI/BOOT' 같은 FAT 내부 경로
    """
    parts = path.strip('/').split('/')
    cur = ""
    for p in parts:
        cur = f"{cur}/{p}" if cur else p
        if cur not in created_dirs:
            # 디렉토리 없으면 만들고, 이미 있으면 에러를 무시하도록 처리
            run(f"mmd -i {img_spec} ::/{cur} 2>/dev/null || true")
            created_dirs.add(cur)


def copy_tree_to_img(img_spec: str, src_root: Path):
    """
    esp/ 내부 전체 구조를 FAT 이미지로 복사.
    빈 디렉토리는 건너뜀.
    """
    for root, dirs, files in os.walk(src_root):
        rel = Path(root).relative_to(src_root)

        # FAT 내부 경로
        fat_dir = "/".join(rel.parts)

        # 빈 디렉토리 skip
        if not dirs and not files:
            print(f"[INFO] skip empty dir: {rel}")
            continue

        # 디렉토리 생성
        if fat_dir:
            safe_mkdir_in_img(img_spec, fat_dir)

        # 파일 복사
        for f in files:
            src = Path(root) / f
            if fat_dir:
                fat_target = f"::/{fat_dir}/{f}"
            else:
                fat_target = f"::/{f}"
            run(f"mcopy -i {img_spec} {src} {fat_target}")


# ---------------------------------------------------------
#  Linux: GPT + ESP 파티션이 있는 이미지 생성
# ---------------------------------------------------------

def get_esp_offset_bytes() -> int:
    """
    parted -m 를 사용하여 1번 파티션(ESP)의 시작 오프셋(B)을 구한다.
    """
    cmd = f"parted -m {IMG_FILE} unit B print"
    print(f"[RUN] {cmd}")
    out = subprocess.check_output(cmd, shell=True, text=True)

    for line in out.splitlines():
        # 파티션 라인은 '1:start:end:size:fs:name:flags' 형식
        if line.startswith("1:"):
            fields = line.split(':')
            start = fields[1]  # 예: '1048576B'
            if not start.endswith('B'):
                raise RuntimeError(f"Unexpected start field: {start}")
            return int(start[:-1])

    raise RuntimeError("ESP 파티션(1번)을 찾지 못했습니다.")


def make_img_linux():
    # 이전 이미지 있으면 삭제
    if IMG_FILE.exists():
        IMG_FILE.unlink()

    # 64MB 빈 디스크 파일 생성
    run(f"dd if=/dev/zero of={IMG_FILE} bs=1M count=64")

    # GPT 파티션 테이블 + ESP 파티션 생성
    # 1MiB offset부터 끝까지 fat32 ESP
    run(f"parted -s {IMG_FILE} mklabel gpt")
    run(f"parted -s {IMG_FILE} mkpart ESP fat32 1MiB 100%")
    run(f"parted -s {IMG_FILE} set 1 esp on")

    # 1번 파티션 시작 오프셋 (bytes) 구하기
    esp_offset = get_esp_offset_bytes()
    print(f"[INFO] ESP offset = {esp_offset} bytes")

    # mtools에서 사용할 이미지 지정 문자열: file@@offset
    img_spec = f"{IMG_FILE}@@{esp_offset}"

    # 파티션 영역을 FAT32로 포맷
    # -F : FAT32 강제, -v : 볼륨 라벨
    run(f"mformat -i {img_spec} -F -v RIBON ::")
    # esp/ 트리 전체를 FAT에 복사
    copy_tree_to_img(img_spec, ESP_DIR)


# ---------------------------------------------------------
#  macOS: 여전히 hdiutil 기반 (superfloppy 스타일)
#  → OVMF 자동부팅 보장은 Linux보다 약함
# ---------------------------------------------------------

def make_img_macos():
    if IMG_FILE.exists():
        IMG_FILE.unlink()

    # hdiutil이 자체적으로 파티션/볼륨을 구성해줌
    run(f"hdiutil create -size 64m -fs MS-DOS -volname RIBON {IMG_FILE}")

    mount_point = ROOT / "mnt_ribon"
    if mount_point.exists():
        shutil.rmtree(mount_point)
    mount_point.mkdir()

    run(f"hdiutil attach {IMG_FILE} -mountpoint {mount_point}")
    shutil.copytree(ESP_DIR, mount_point, dirs_exist_ok=True)
    run(f"hdiutil detach {mount_point}")


# ---------------------------------------------------------
#  Windows (WSL 필요) – 기본 superfloppy 구조 유지
# ---------------------------------------------------------

def make_img_windows():
    if shutil.which("wsl") is None:
        raise RuntimeError("Windows에서는 WSL이 필요합니다.")

    if IMG_FILE.exists():
        IMG_FILE.unlink()

    def win_to_wsl(path: Path):
        drive = path.drive[0].lower()
        rest = path.as_posix()[2:]
        return f"/mnt/{drive}/{rest}"

    esp_wsl = win_to_wsl(ESP_DIR)
    img_wsl = win_to_wsl(IMG_FILE)

    cmd = f"""
        wsl bash -c '
        dd if=/dev/zero of="{img_wsl}" bs=1M count=64
        mkfs.vfat "{img_wsl}"

        cp -r "{esp_wsl}" /tmp/ribon_esp
        cd /tmp/ribon_esp

        for f in $(find . -type f); do
            mkdir -p "$(dirname "$f")"
            mcopy -i "{img_wsl}" "$f" "::/$f"
        done
        '
    """
    run(cmd)


# ---------------------------------------------------------
#  Main
# ---------------------------------------------------------

def main():
    print("=== Ribon Boot Image Builder ===")
    ensure_dirs()

    system = platform.system()

    if system == "Linux":
        make_img_linux()
    elif system == "Darwin":
        make_img_macos()
    elif system == "Windows":
        make_img_windows()
    else:
        raise RuntimeError("지원되지 않는 OS")

    print(f"\n[OK] 이미지 생성 완료 -> {IMG_FILE}")


if __name__ == "__main__":
    main()
