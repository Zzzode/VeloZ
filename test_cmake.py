#!/usr/bin/env python3

import os
import subprocess
import sys

def run_command(cmd):
    try:
        result = subprocess.run(cmd, shell=True, check=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              text=True, timeout=60)
        print(f"Command '{cmd}' succeeded:")
        print(result.stdout.strip())
        return result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        print(f"Command '{cmd}' timed out")
        return -1, "", "Timeout"
    except subprocess.CalledProcessError as e:
        print(f"Command '{cmd}' failed with return code {e.returncode}")
        print("STDOUT:")
        print(e.stdout.strip())
        print("STDERR:")
        print(e.stderr.strip())
        return e.returncode, e.stdout, e.stderr
    except Exception as e:
        print(f"Command '{cmd}' failed with exception: {e}")
        return -1, "", str(e)

def main():
    print("Current working directory:", os.getcwd())
    print()

    print("Checking CMake installation:")
    run_command("which cmake")
    run_command("cmake --version")
    print()

    print("Checking project directory contents:")
    run_command("ls -la /home/zzzode/Develop/VeloZ/")
    print()

    print("Checking CMake files:")
    run_command("ls -la /home/zzzode/Develop/VeloZ/CMake*")
    print()

    print("Checking build directory:")
    run_command("ls -la /home/zzzode/Develop/VeloZ/build/")
    print()

    print("Attempting to create build directory:")
    run_command("mkdir -p /home/zzzode/Develop/VeloZ/build/dev")
    print()

    print("Attempting to configure CMake:")
    run_command("cd /home/zzzode/Develop/VeloZ && cmake -B build/dev -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON")

if __name__ == "__main__":
    main()