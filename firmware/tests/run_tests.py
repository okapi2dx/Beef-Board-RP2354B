"""Run production C logic with host GCC; output stays in a temporary directory."""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
src = root / "src"
tests = root / "tests"
compiler = os.environ.get("CC", "gcc")
main = (src / "main.c").read_text(encoding="utf-8")

def function(name):
    match = re.search(r"^static [^\n]*\b" + name + r"\([^;]*?\)\s*\{", main, re.M)
    if not match:
        raise RuntimeError(f"Production function missing: {name}")
    end, depth = match.end(), 1
    while depth:
        depth += (main[end] == "{") - (main[end] == "}")
        end += 1
    return main[match.start():end]

# Test the actual hardware adapter functions too, using deterministic USB/DMA
# stand-ins. Functions are extracted verbatim; algorithms are not copied here.
integration = (tests / "integration_stubs.h").read_text(encoding="utf-8")
integration += "\n" + "\n".join(re.findall(r"^#define (?:CFG_\w+|INPUT_MODE_\w+|BUTTON_COUNT|B8_\w+)\s+[^\n]+", main, re.M))
for name in ["read_le16", "rgb_word", "update_rgb_leds", "remap_buttons", "controller_buttons", "update_button_leds", "update_diagnostics", "send_controller_report"]:
    integration += "\n" + function(name)
integration += "\n" + (tests / "integration_cases.c").read_text(encoding="utf-8")
with tempfile.TemporaryDirectory(prefix="beef-tests-") as directory:
    temp = Path(directory)
    adapter = temp / "integration.c"
    adapter.write_text(integration, encoding="utf-8")
    protocol_source = (src / "beef_protocol.c").read_text(encoding="utf-8")
    protocol_source = re.sub(r'^#include "(?:pico/[^\"]+|hardware/[^\"]+|tusb.h)"\s*$', '', protocol_source, flags=re.M)
    (temp / "protocol_under_test.c").write_text(protocol_source, encoding="utf-8")
    for name, sources in [
        ("button_debounce", [tests / "button_debounce_test.c"]),
        ("button_chatter", [tests / "button_chatter_test.c"]),
        ("protocol_stream", [tests / "protocol_stream_test.c"]),
        ("runtime", [tests / "runtime_test.c", src / "beef_runtime.c", src / "beef_lighting.c"]),
        ("storage", [tests / "storage_test.c", src / "beef_runtime.c"]),
        ("integration", [adapter, src / "beef_runtime.c", src / "beef_lighting.c"]),
    ]:
        executable = temp / (name + (".exe" if os.name == "nt" else ""))
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(src),
                        "-I" + str(tests / "stubs"), "-I" + str(temp), *map(str, sources), "-lm", "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)

subprocess.run([sys.executable, str(tests / "at90_lighting_test.py")], check=True)
