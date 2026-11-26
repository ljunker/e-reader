# PlatformIO extra script for env:sim: builds the firmware with Emscripten
# into firmware.js + firmware.wasm and puts the simulator page next to them.
#
#   pio run -e sim              -> .pio/build/sim/web/
#   pio run -e sim -t serve     -> build, then http://localhost:8000/
import os
import shutil
import subprocess
import sys

Import("env")  # noqa: F821

PROJECT = env.subst("$PROJECT_DIR")
BUILD = env.subst("$BUILD_DIR")
SIM = os.path.join(PROJECT, "sim")
WEB_OUT = os.path.join(BUILD, "web")


def fail(msg):
    sys.stderr.write("\n*** sim: %s\n\n" % msg)
    env.Exit(1)


if not shutil.which("emcc"):
    fail("Emscripten (emcc) not found. Install it with `brew install emscripten` "
         "or from https://emscripten.org/docs/getting_started/ and run again.")

# ---- toolchain: the native platform scans for gcc after this script, so
# swap the compilers in right after it does.
EMSCRIPTEN = dict(CC="emcc", CXX="em++", AR="emar", RANLIB="emranlib",
                  PROGNAME="firmware", PROGSUFFIX=".js")
_tool = env.Tool


def _tool_then_emscripten(name, *args, **kwargs):
    result = _tool(name, *args, **kwargs)
    env.Replace(**EMSCRIPTEN)
    return result


env.Tool = _tool_then_emscripten
env.Replace(**EMSCRIPTEN)

# ---- parts of the arduino-esp32 core that run unchanged in the browser:
# String, Print, Stream and the VFS file system. Copied into the build so
# their `#include "Arduino.h"` finds the simulator's Arduino.h.
core_pkg = os.path.join(env.subst("$PROJECT_PACKAGES_DIR"), "framework-arduinoespressif32")
if not os.path.isdir(core_pkg):
    fail("framework-arduinoespressif32 is missing; run `pio pkg install -e feather_s3` once.")
CORE_FILES = {
    "cores/esp32": ["WString.h", "WString.cpp", "Print.h", "Print.cpp", "Printable.h", "Stream.h",
                    "Stream.cpp", "stdlib_noniso.h", "stdlib_noniso.c", "pgmspace.h"],
    "libraries/FS/src": ["FS.h", "FS.cpp", "FSImpl.h", "vfs_api.h", "vfs_api.cpp"],
}
core_dir = os.path.join(BUILD, "arduino_core")
os.makedirs(core_dir, exist_ok=True)
for sub, names in CORE_FILES.items():
    for name in names:
        src = os.path.join(core_pkg, sub, name)
        dst = os.path.join(core_dir, name)
        with open(src, encoding="utf-8") as f:
            text = f.read()
        # newlib's C strchr() returns char *; libc++ keeps it const in C++
        text = text.replace("= strchr(", "= (char *)strchr(")
        old = None
        if os.path.exists(dst):
            with open(dst, encoding="utf-8") as f:
                old = f.read()
        if old != text:
            with open(dst, "w", encoding="utf-8") as f:
                f.write(text)

env.Prepend(CPPPATH=[
    os.path.join(SIM, "include"),
    core_dir,
    # for sim/src, which the library finder does not scan
    os.path.join(PROJECT, "include"),
    os.path.join(PROJECT, "lib", "epdsim", "src"),
    os.path.join(PROJECT, "lib", "fuelgauge", "src"),
])
env.Append(
    CCFLAGS=["-O2", "-DSIM_BUILD=1", "-DARDUINO=10819", "-Wno-unused-function", "-Wno-deprecated-declarations"],
    CXXFLAGS=["-std=gnu++17"],
    LINKFLAGS=[
        "-O2",
        "-sMODULARIZE=1",
        "-sEXPORT_NAME=createFirmware",
        "-sENVIRONMENT=web",
        "-sASYNCIFY=1",
        "-sASYNCIFY_STACK_SIZE=262144",
        "-sSTACK_SIZE=1048576",
        "-sINITIAL_MEMORY=33554432",
        "-sALLOW_MEMORY_GROWTH=1",
        "-sFORCE_FILESYSTEM=1",
        "-lidbfs.js",
        "-sEXPORTED_FUNCTIONS=_main,_malloc,_free",
        "-sEXPORTED_RUNTIME_METHODS=FS,IDBFS,HEAPU8,stringToNewUTF8,UTF8ToString,addRunDependency,removeRunDependency",
    ],
)

env.BuildSources(os.path.join(BUILD, "arduino_core_obj"), core_dir)
env.BuildSources(os.path.join(BUILD, "sim_hal"), os.path.join(SIM, "src"))


# ---- skip library sources the reader never uses (they need real hardware
# headers): Adafruit's TFT/OLED drivers and GxEPD2's other panels.
def skip_unused(node):
    path = node.get_abspath().replace("\\", "/")
    name = os.path.basename(path)
    if name in ("Adafruit_SPITFT.cpp", "Adafruit_GrayOLED.cpp"):
        return None
    if "/GxEPD2/" in path:
        rel = path.rsplit("/GxEPD2/", 1)[1]
        if rel.startswith("src/"):
            rel = rel[4:]
        if "/" in rel and rel != "gdey/GxEPD2_420_GDEY042T81.cpp":
            return None
    return node


env.AddBuildMiddleware(skip_unused)


# ---- the page
def copy_web(target=None, source=None, env=env):
    os.makedirs(WEB_OUT, exist_ok=True)
    for name in os.listdir(os.path.join(SIM, "web")):
        shutil.copy2(os.path.join(SIM, "web", name), os.path.join(WEB_OUT, name))
    for name in ("firmware.js", "firmware.wasm"):
        shutil.copy2(os.path.join(BUILD, name), os.path.join(WEB_OUT, name))
    samples = os.path.join(WEB_OUT, "samples")
    os.makedirs(samples, exist_ok=True)
    for name in ("sample.epub", "sample2.epub", "drm.epub"):
        shutil.copy2(os.path.join(PROJECT, "test", "fixtures", name), os.path.join(samples, name))
    print("Simulator page: %s" % os.path.join(WEB_OUT, "index.html"))


program = os.path.join(BUILD, "firmware.js")
env.AddPostAction(program, copy_web)


def serve(target, source, env):
    copy_web()
    port = os.environ.get("SIM_PORT", "8000")
    print("Simulator on http://localhost:%s/  (Ctrl+C stops)" % port)
    subprocess.call([sys.executable, os.path.join(SIM, "serve.py"), port, WEB_OUT])


env.AddCustomTarget(
    name="serve",
    dependencies=program,
    actions=serve,
    title="Serve simulator",
    description="Build the simulator and serve it on http://localhost:8000/",
)
