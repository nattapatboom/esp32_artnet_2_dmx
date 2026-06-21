#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(script_dir)
    manual_dir = os.path.join(root_dir, 'docs', 'user_manual')
    input_file = 'main.typ'
    output_pdf_name = 'user_manual.pdf'
    dest_path = os.path.join(root_dir, 'docs', output_pdf_name)

    print("=======================================")
    print("      Typst User Manual Compiler       ")
    print("=======================================")

    if not os.path.exists(manual_dir):
        print(f"[-] Error: Manual directory '{manual_dir}' does not exist.")
        sys.exit(1)

    # 1. Determine where typst.exe is located
    typst_bin = None

    # Check system PATH
    if shutil.which('typst'):
        typst_bin = 'typst'
    # Check WinGet standard directory on Windows
    elif os.name == 'nt':
        local_appdata = os.environ.get('LOCALAPPDATA', '')
        username = os.environ.get('USERNAME', 'natta')
        
        common_paths = [
            # WinGet links path
            os.path.join(local_appdata, 'Microsoft', 'WinGet', 'Links', 'typst.exe'),
            # Specific WinGet package path
            os.path.join(local_appdata, 'Microsoft', 'WinGet', 'Packages', 
                         'Typst.Typst_Microsoft.Winget.Source_8wekyb3d8bbwe', 
                         'typst-x86_64-pc-windows-msvc', 'typst.exe'),
            # General User profile fallback path
            f"C:\\Users\\{username}\\AppData\\Local\\Microsoft\\WinGet\\Links\\typst.exe"
        ]
        for path in common_paths:
            if os.path.exists(path):
                typst_bin = path
                break

    if not typst_bin:
        print("[-] Error: Typst compiler not found.")
        print("    Please install Typst via WinGet using command:")
        print("      winget install Typst.Typst")
        sys.exit(1)

    print(f"[+] Found Typst compiler: {typst_bin}")
    print("[+] Compiling document...")

    os.chdir(manual_dir)

    # Compile command: typst compile main.typ output.pdf
    cmd_args = [typst_bin, 'compile', input_file, dest_path]
    
    result = subprocess.run(cmd_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    if result.returncode != 0:
        print("[-] Error compiling manual. Typst compile logs/errors:")
        print(result.stdout)
        print(result.stderr)
        sys.exit(1)

    print(f"[+] Success! Generated: {dest_path}")
    print("[+] Typst compiles in a single pass with no temporary auxiliary files.")

if __name__ == '__main__':
    main()
