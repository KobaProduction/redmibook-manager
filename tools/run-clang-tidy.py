"""Run the complete configured source list with bounded parallelism."""

import concurrent.futures
from pathlib import Path
import subprocess
import sys
from tidy_cache import AnalysisCache


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
    cache = AnalysisCache(tool, Path(build), arguments)
    keys = {source: cache.key(source) for source in sources}
    pending = [source for source in sources if not cache.hit(source, keys[source])]
    print(f"clang-tidy: {len(sources) - len(pending)} cached, {len(pending)} to analyze", flush=True)
    failures = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        futures = [
            pool.submit(check_source, tool, build, source, arguments)
            for source in pending
        ]
        for future in concurrent.futures.as_completed(futures):
            source, code, output = future.result()
            cache.record(source, keys[source], code == 0)
            print(f"clang-tidy: {source}: {'FAIL' if code else 'OK'}", flush=True)
            if code:
                failures += 1
                print(output, flush=True)
    print(f"clang-tidy: {len(sources)} sources, {len(pending)} analyzed, {failures} failures", flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
