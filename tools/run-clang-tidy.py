"""Run the complete configured source list with bounded parallelism."""

import concurrent.futures
from pathlib import Path
import subprocess
import sys


def check_source(tool, build, source, arguments):
    result = subprocess.run(
        [tool, "-p", build, source, *arguments],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    return source, result.returncode, result.stdout + result.stderr


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    tool, build, source_list, *arguments = sys.argv[1:]
    sources = [source for source in Path(source_list).read_text(encoding="utf-8").splitlines() if source]
    if not sources:
        raise ValueError("No configured sources for clang-tidy")
    failures = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        futures = [
            pool.submit(check_source, tool, build, source, arguments)
            for source in sources
        ]
        for future in concurrent.futures.as_completed(futures):
            source, code, output = future.result()
            print(f"clang-tidy: {source}: {'FAIL' if code else 'OK'}", flush=True)
            if code:
                failures += 1
                print(output, flush=True)
    print(f"clang-tidy: {len(futures)} sources, {failures} failures", flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
