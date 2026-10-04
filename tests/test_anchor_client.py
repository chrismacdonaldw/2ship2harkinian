"""Exercise actual native Anchor/Profile code with stubbed game, transport and host services.
Supply the native dependency paths: --json-include DIR --lus-dir DIR. No server or game is launched.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--json-include", required=True)
    parser.add_argument("--lus-dir", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = root / "mm/2s2h/Network/Anchor"
    strip = lambda p: re.sub(r'^#include.*$', '', p.read_text(), flags=re.M)
    fixture = (root / "tests/AnchorClientFixture.inc").read_text()
    fixture = fixture.replace("PROFILE_BODY", strip(source / "Profile.cpp"))
    fixture = fixture.replace("CLIENT_HEADER", strip(source / "Anchor.h"))
    fixture = fixture.replace("CLIENT_BODY", strip(source / "Anchor.cpp"))
    with tempfile.TemporaryDirectory(prefix="mm-anchor-client-") as temp:
        path = Path(temp) / "test.cpp"
        path.write_text(fixture)
        for mode in ["standalone", "module"]:
            output = Path(temp) / mode
            command = [args.cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-DENABLE_ANCHOR",
                       "-I" + args.json_include, "-I" + str(Path(args.lus_dir) / "include"),
                       str(path), str(Path(args.lus_dir) / "src/ship/utils/StrHash64.cpp"), "-o", str(output)]
            if mode == "module":
                command.append("-DDIPTYCH_GAME_MODULE")
            subprocess.run(command, check=True)
            subprocess.run([str(output)], check=True)


if __name__ == "__main__":
    main()
