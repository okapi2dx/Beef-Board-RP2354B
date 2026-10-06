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
        ("phoenixwan_uart", [tests / "phoenixwan_uart_test.c", src / "phoenixwan_uart.c"]),
        ("runtime", [tests / "runtime_test.c", src / "beef_runtime.c", src / "beef_lighting.c"]),
        ("storage", [tests / "storage_test.c", src / "beef_runtime.c"]),
        ("integration", [adapter, src / "beef_runtime.c", src / "beef_lighting.c"]),
    ]:
        executable = temp / (name + (".exe" if os.name == "nt" else ""))
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(src),
                        "-I" + str(tests / "stubs"), "-I" + str(temp), *map(str, sources), "-lm", "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)

    at90_source = (root / "at90usb1286/devices/iidx/iidx_usb.cpp").read_text(encoding="utf-8")
    update_begin = at90_source.index("  void UsbHandler::update(const config &config)")
    update_end = at90_source.index("  void UsbHandler::config_update", update_begin)
    update_source = at90_source[update_begin:update_end].replace("void UsbHandler::update", "static void firmware_update", 1)
    (temp / "at90_update_under_test.h").write_text(update_source, encoding="utf-8")
    cpp_compiler = os.environ.get("CXX", "g++")
    at90_update = temp / ("at90_update.exe" if os.name == "nt" else "at90_update")
    subprocess.run([cpp_compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-I" + str(temp), str(tests / "at90_turntable_update_test.cpp"), "-o", str(at90_update)], check=True)
    subprocess.run([str(at90_update)], check=True)
    begin = at90_source.index("    if (turntable_multiplier(new_config)", at90_source.index("void UsbHandler::config_update"))
    opening = at90_source.index("{", begin)
    end, depth = opening + 1, 1
    while depth:
        depth += (at90_source[end] == "{") - (at90_source[end] == "}")
        end += 1
    (temp / "at90_reconfigure_under_test.h").write_text("static void at90_reconfigure(const config &new_config) {\n" + at90_source[begin:end] + "\n}\n", encoding="utf-8")
    cpp_compiler = os.environ.get("CXX", "g++")
    button_begin = at90_source.index("    update_tt_transitions(new_config.reverse_tt)", end)
    button_end = at90_source.index("\n  }", button_begin)
    (temp / "at90_button_reconfigure_under_test.h").write_text("static void reconfigure(const config &new_config) {\n" + at90_source[button_begin:button_end] + "\n}\n", encoding="utf-8")
    button_reconfigure = temp / ("button_reconfigure.exe" if os.name == "nt" else "button_reconfigure")
    subprocess.run([cpp_compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-I" + str(temp), str(tests / "button_reconfigure_test.cpp"), "-o", str(button_reconfigure)], check=True)
    subprocess.run([str(button_reconfigure)], check=True)
    runtime_object = temp / "runtime.o"
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(src), "-c", str(src / "beef_runtime.c"), "-o", str(runtime_object)], check=True)
    parity = temp / ("turntable_parity.exe" if os.name == "nt" else "turntable_parity")
    subprocess.run([cpp_compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", str(tests / "turntable_parity_test.cpp"), str(runtime_object), "-o", str(parity)], check=True)
    subprocess.run([str(parity)], check=True)
    smooth = temp / ("turntable_smooth.exe" if os.name == "nt" else "turntable_smooth")
    subprocess.run([cpp_compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", str(tests / "turntable_smooth_test.cpp"), str(runtime_object), "-o", str(smooth)], check=True)
    subprocess.run([str(smooth)], check=True)
    multiplier = temp / ("multiplier.exe" if os.name == "nt" else "multiplier")
    subprocess.run([cpp_compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", str(tests / "turntable_multiplier_test.cpp"), str(runtime_object), "-o", str(multiplier)], check=True)
    subprocess.run([str(multiplier)], check=True)
    reconfigure = temp / ("turntable_reconfigure.exe" if os.name == "nt" else "turntable_reconfigure")
    subprocess.run([cpp_compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-I" + str(temp), str(tests / "turntable_reconfigure_test.cpp"), str(runtime_object), "-o", str(reconfigure)], check=True)
    subprocess.run([str(reconfigure)], check=True)



    for name, source in [
        ("at90_turntable_gate", root / "at90usb1286" / "tests" / "turntable_gate_test.cpp"),
        ("at90_debounce", root / "at90usb1286" / "tests" / "debounce_test.cpp"),
        ("button_debounce_parity", tests / "button_debounce_parity_test.cpp"),
    ]:
        executable = temp / (name + (".exe" if os.name == "nt" else ""))
        subprocess.run([cpp_compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                        str(source), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)

subprocess.run([sys.executable, str(tests / "at90_lighting_test.py")], check=True)
subprocess.run([sys.executable, str(tests / "at90_rgb_diagnostics_test.py")], check=True)

subprocess.run([sys.executable, str(tests / "at90_reset_test.py")], check=True)
