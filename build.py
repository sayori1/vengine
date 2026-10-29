import argparse
import platform
from pathlib import Path
import shlex
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent
BUILD = ROOT / "build"
SHADERS = {
    "kVertexSource": "geometry_pass/shaders/geometry.vert",
    "kFragmentSource": "geometry_pass/shaders/geometry.frag",
    "kPbrVertexSource": "geometry_pass/shaders/pbr.vert",
    "kPbrFragmentSource": "geometry_pass/shaders/pbr.frag",
    "kTonemapVertexSource": "geometry_pass/shaders/tonemap.vert",
    "kTonemapFragmentSource": "geometry_pass/shaders/tonemap.frag",
    "kDepthVertexSource": "depth_pass/shaders/depth.vert",
    "kDepthFragmentSource": "depth_pass/shaders/depth.frag",
    "kShadowVertexSource": "shadow_pass/shaders/shadow.vert",
    "kShadowFragmentSource": "shadow_pass/shaders/shadow.frag",
    "kLightCullingComputeSource": "light_culling_pass/shaders/light_culling.comp",
}


def ninja_escape(value):
    return str(value).replace("$", "$$").replace(" ", "$ ").replace(":", "$:")


def require_tool(name):
    path = shutil.which(name)
    if path is None:
        raise RuntimeError(f"Required tool not found: {name}")
    return path


def generate_shaders():
    generated = BUILD / "generated"
    generated.mkdir(parents=True, exist_ok=True)
    shaders = ROOT / "src/modules/renderer/passes"
    shader_header = "#pragma once\n\nnamespace geometry_shaders {\n"
    for name, path in SHADERS.items():
        source = (shaders / path).read_text()
        shader_header += f'inline constexpr char {name}[] = R"glsl({source})glsl";\n'
    shader_header += "}\n"
    shader_path = generated / "GeometryShaderSources.h"
    if not shader_path.exists() or shader_path.read_text() != shader_header:
        shader_path.write_text(shader_header)

def build(config):
    if sys.platform != "linux" or platform.machine() not in ("x86_64", "AMD64"):
        raise RuntimeError("Only Linux x86_64 is supported")
    compiler = require_tool("clang++")
    ninja = require_tool("ninja")
    if not (ROOT / "vendor/sokol/sokol_app.h").is_file():
        raise RuntimeError("Sokol is missing. Run: git submodule update --init --recursive")

    sources = sorted((ROOT / "src").rglob("*.cpp"))
    if not sources:
        raise RuntimeError("No C++ sources found in src/")

    directory = BUILD / config
    directory.mkdir(parents=True, exist_ok=True)
    manifest = directory / "build.ninja"
    binary = directory / "vengine"
    common = "-std=c++20 -m64 -Wall -Wextra -Wpedantic -DSOKOL_GLCORE -Ibuild/generated -Isrc -isystem vendor/sokol -isystem vendor/glm"
    if config == "debug":
        cflags = f"{common} -O0 -g3 -DDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer"
        ldflags = "-m64 -fsanitize=address,undefined"
    else:
        linker = require_tool("ld.lld")
        cflags = f"{common} -O3 -DNDEBUG -flto=thin -ffunction-sections -fdata-sections"
        ldflags = f"-m64 -O3 -flto=thin --ld-path={shlex.quote(linker)} -Wl,-O2,--gc-sections,--as-needed"

    lines = [
        "ninja_required_version = 1.3",
        f"builddir = {ninja_escape(directory.relative_to(ROOT).as_posix())}",
        f"cxx = {ninja_escape(shlex.quote(compiler))}",
        f"cflags = {ninja_escape(cflags)}",
        f"ldflags = {ninja_escape(ldflags)}",
        "libs = -lX11 -lXi -lXcursor -lGL -ldl -pthread -lm",
        "rule shaders",
        f"  command = {ninja_escape(shlex.quote(sys.executable))} build.py shaders",
        "  description = EMBED renderer shaders",
        "  restat = 1",
        "build build/generated/GeometryShaderSources.h: shaders " + " ".join(
            ninja_escape(f"src/modules/renderer/passes/{path}") for path in SHADERS.values()
        ) + " | build.py",
        "rule compile",
        "  command = $cxx $cflags -pthread -MD -MF $out.d -MT $out -c $in -o $out",
        "  depfile = $out.d",
        "  deps = gcc",
        "  description = CXX $in",
        "rule link",
        "  command = $cxx $ldflags @$out.rsp $libs -o $out",
        "  rspfile = $out.rsp",
        "  rspfile_content = $in",
        "  description = LINK $out",
    ]
    objects = []
    for source in sources:
        relative = source.relative_to(ROOT)
        obj = directory / "obj" / relative.with_suffix(".cpp.o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        output = ninja_escape(obj.relative_to(ROOT).as_posix())
        objects.append(output)
        lines.append(f"build {output}: compile {ninja_escape(relative.as_posix())} || build/generated/GeometryShaderSources.h")

    target = ninja_escape(binary.relative_to(ROOT).as_posix())
    manifest_path = ninja_escape(manifest.relative_to(ROOT).as_posix())
    lines.extend([
        f"build {target}: link {' '.join(objects)} | {manifest_path}",
        f"default {target}",
        "",
    ])
    content = "\n".join(lines)
    if not manifest.exists() or manifest.read_text() != content:
        manifest.write_text(content)
    database = subprocess.run(
        [ninja, "-f", str(manifest), "-t", "compdb", "compile"],
        cwd=ROOT, check=True, capture_output=True, text=True,
    ).stdout
    database_path = ROOT / "compile_commands.json"
    if not database_path.exists() or database_path.read_text() != database:
        database_path.write_text(database)
    subprocess.run([ninja, "-f", str(manifest)], cwd=ROOT, check=True)
    return binary


def main():
    parser = argparse.ArgumentParser(description="Build and run vengine with Clang and Ninja")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("debug")
    commands.add_parser("release")
    commands.add_parser("clean")
    commands.add_parser("shaders")
    run = commands.add_parser("run")
    run.add_argument("config", choices=("debug", "release"))
    args = parser.parse_args()
    try:
        if args.command == "shaders":
            generate_shaders()
            return 0
        if args.command == "clean":
            if BUILD.exists():
                shutil.rmtree(BUILD)
            database_path = ROOT / "compile_commands.json"
            if database_path.exists():
                database_path.unlink()
            return 0
        config = args.config if args.command == "run" else args.command
        binary = build(config)
        if args.command == "run":
            subprocess.run([str(binary)], cwd=ROOT, check=True)
        return 0
    except subprocess.CalledProcessError as error:
        return error.returncode if error.returncode > 0 else 128 - error.returncode
    except (OSError, RuntimeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())
