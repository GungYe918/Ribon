#!/usr/bin/zsh


cmake --build build
python3 Tools/make_img.py 
python3 Tools/run_qemu.py
