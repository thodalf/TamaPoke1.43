#!/usr/bin/env python3
# Concatenates the proto.h include onto TamaPoke.ino to produce sketch.cpp.
# Kept as its own script (rather than shell `cat >`) because CMake's
# add_custom_command does not run through a shell, so redirection isn't
# available portably -- this is what the CMakeLists.txt build (for CLion)
# uses instead of the `cat` line in build.sh.
import pathlib
import sys

ino_path, out_path = sys.argv[1], sys.argv[2]
sketch = '#include "proto.h"\n' + pathlib.Path(ino_path).read_text()
pathlib.Path(out_path).write_text(sketch)
