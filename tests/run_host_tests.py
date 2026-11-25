"""Exercise production Referee methods with host UART/RTOS substitutes.

Only dependency includes and member visibility are adapted in a temporary header.
The protocol methods and shared types are copied unchanged. Real LibXR CRC code is
used, with an independent bitwise implementation in the test as the frame oracle.
"""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--libxr", type=Path, required=True)
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    module = tests.parent
    with tempfile.TemporaryDirectory(prefix="referee-2027-") as directory:
        build = Path(directory)
        source = (module / "Referee.hpp").read_text(encoding="utf-8")
        source = re.sub(r'^#include "(?!RefereeTypes.hpp")[^"\n]+"\n', "", source,
                        flags=re.MULTILINE)
        source = source.replace(" private:", " public:")
        (build / "referee_host.hpp").write_text(
            '#include "host_runtime.hpp"\n#include "crc_host.hpp"\n' + source,
            encoding="utf-8")
        crc = (args.libxr / "src/utils/crc.hpp").read_text(encoding="utf-8")
        crc = crc.replace('#include "libxr_def.hpp"', "")
        (build / "crc_host.hpp").write_text(crc, encoding="utf-8")
        crc_source = (args.libxr / "src/utils/crc_o3.cpp").read_text(encoding="utf-8")
        (build / "crc_host.cpp").write_text(
            crc_source.replace('#include "crc.hpp"', '#include "crc_host.hpp"'),
            encoding="utf-8")
        target = subprocess.check_output([args.cxx, "-dumpmachine"], text=True)
        flags = ["-std=c++20", "-Wall", "-Wextra", "-Werror"]
        if "mingw" in target:
            flags += ["-mno-ms-bitfields"]
        for name in ("types_test", "protocol_test", "review_test"):
            exe = build / (name + ".exe")
            subprocess.run([args.cxx, *flags, "-I", str(module), "-I", str(tests),
                            "-I", str(build), str(tests / (name + ".cpp")),
                            str(build / "crc_host.cpp"),
                            "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True, timeout=30)
        print("Referee 2027 host tests passed")


if __name__ == "__main__":
    main()
