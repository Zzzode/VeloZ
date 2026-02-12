#!/usr/bin/env python3
"""
Check for Chinese content in the repository
Usage: ./scripts/check_chinese.py [optional path to check]
"""

import os
import re
import argparse
from pathlib import Path

# Define file types to check
INCLUDE_EXTENSIONS = {
    ".cpp", ".h", ".hpp", ".c", ".cc",
    ".js", ".ts", ".tsx", ".jsx",
    ".py",
    ".html", ".htm",
    ".md", ".txt",
    ".json", ".yaml", ".yml"
}

# Define directories to exclude
EXCLUDE_DIRS = {
    ".git", "build", "node_modules", ".cache", "dist", "__pycache__"
}

# Define files to exclude
EXCLUDE_FILES = {
    ".swp", ".swo", ".DS_Store"
}

# Chinese character pattern, matches common Chinese characters
CHINESE_PATTERN = re.compile(r"[\u4e00-\u9fff]+")


def is_excluded_dir(dir_name: str) -> bool:
    """Check if directory should be excluded"""
    return dir_name in EXCLUDE_DIRS


def is_excluded_file(file_name: str) -> bool:
    """Check if file should be excluded"""
    return any(file_name.endswith(ext) for ext in EXCLUDE_FILES)


def should_check_file(file_path: Path) -> bool:
    """Determine if file should be checked"""
    if file_path.is_dir():
        return False

    # Exclude the script itself
    if file_path.resolve() == Path(__file__).resolve():
        return False

    # Check if file should be excluded by name
    if is_excluded_file(file_path.name):
        return False

    # Check if file extension is included in the allowed list
    if file_path.suffix not in INCLUDE_EXTENSIONS:
        return False

    return True


def check_file_for_chinese(file_path: Path) -> list[tuple[int, str]]:
    """Check if file contains Chinese content"""
    chinese_lines = []

    try:
        with open(file_path, "r", encoding="utf-8") as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        try:
            with open(file_path, "r", encoding="gbk") as f:
                lines = f.readlines()
        except Exception as e:
            print(f"Warning: Cannot read {file_path}: {e}")
            return []
    except Exception as e:
        print(f"Warning: Cannot read {file_path}: {e}")
        return []

    for line_num, line in enumerate(lines, 1):
        if CHINESE_PATTERN.search(line):
            chinese_lines.append((line_num, line.rstrip()))

    return chinese_lines


def check_directory(path: str) -> dict[Path, list[tuple[int, str]]]:
    """Check all files in directory"""
    results = {}

    for root, dirs, files in os.walk(path):
        # Remove excluded directories from dirs so os.walk won't enter them
        dirs[:] = [d for d in dirs if not is_excluded_dir(d)]

        for file_name in files:
            file_path = Path(root) / file_name

            if should_check_file(file_path):
                chinese_lines = check_file_for_chinese(file_path)
                if chinese_lines:
                    results[file_path] = chinese_lines

    return results


def main():
    parser = argparse.ArgumentParser(
        description="Check for Chinese content in repository"
    )
    parser.add_argument(
        "path",
        nargs="?",
        default=".",
        help="Path to check (default: current directory)"
    )

    args = parser.parse_args()
    check_path = Path(args.path)

    if not check_path.exists():
        print(f"Error: Path '{check_path}' does not exist")
        return 1

    print("=== Checking for Chinese content in repository ===")
    print(f"Check path: {check_path.resolve()}")
    print(f"Included file types: {', '.join(sorted(INCLUDE_EXTENSIONS))}")
    print(f"Excluded directories: {', '.join(sorted(EXCLUDE_DIRS))}")
    print("==================================")
    print()

    results = check_directory(check_path)

    if results:
        print("Found Chinese content:")
        print("----------------------------------")

        for file_path, chinese_lines in sorted(results.items()):
            print(f"\n{file_path}:")
            for line_num, line in chinese_lines:
                print(f"  Line {line_num}: {line}")

        print()
        print("==================================")
        print(f"Check completed, found Chinese content in {len(results)} files")
        return 1  # Indicate Chinese content was found
    else:
        print("No Chinese content found")
        print("==================================")
        print("Check completed, all files comply with English-only requirement")
        return 0  # Indicate no Chinese content was found


if __name__ == "__main__":
    exit_code = main()
    exit(exit_code)
