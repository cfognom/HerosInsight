import subprocess
import os
import time
import argparse
from pathlib import Path

if (__name__ == "__main__"):
    parser = argparse.ArgumentParser()
    parser.add_argument("mod_path")
    args = parser.parse_args()

    gw_path = Path(os.environ["ProgramFiles(x86)"]) / "Guild Wars/Gw.exe"
    mod_path = Path(args.mod_path)
    subprocess.Popen([gw_path])
    time.sleep(1)
    subprocess.Popen([mod_path])
