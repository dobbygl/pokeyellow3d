#!/usr/bin/env python3
"""Rebuild private historical QA helpers from Git, without rebuilding the cartridge.

Linux/Makefiles/MinSizeRel only. Start from a completed current CMake build.
Source commits, source archives, commands and binary hashes accompany the output.
The 7955aaa renderer receives only a six-decimal mesh timing diagnostic.
"""
import argparse
import hashlib
import json
import platform
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

REFERENCE = '7955aaa0ddbba3d9e2fd78085902be167faccf23'
RUNTIME = '00cc26dafb9a41ea9d935508e9fbe9e25b5f5a6e'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('build', 'runtime', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--parent', required=True, help='Commit immediately before this phase')
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    build, runtime = args.build.resolve(), args.runtime.resolve()
    if platform.system() != 'Linux':
        parser.error('This historical helper recipe requires Linux')
    flags = (build / '_gbrt_build/CMakeFiles/gbrt.dir/flags.make').read_text()
    expected = {'-Os', '-DNDEBUG', '-std=gnu++17', '-ffunction-sections',
                '-fdata-sections', '-DWITH_GZFILEOP'}
    actual = next(line.split('=', 1)[1].split() for line in flags.splitlines()
                  if line.startswith('CXX_FLAGS ='))
    if set(actual) != expected:
        parser.error('Expected the standard MinSizeRel flags; do not mix optimization settings')

    def git(*command, cwd=project):
        return subprocess.check_output(['git', *command], cwd=cwd, text=True).strip()

    if git('rev-parse', 'HEAD', cwd=runtime) != RUNTIME:
        parser.error('Runtime does not match the pinned historical revision')
    subprocess.run(['git', 'diff', '--quiet', 'HEAD'], cwd=runtime, check=True)
    parent = git('rev-parse', '--verify', '--end-of-options', args.parent + '^{commit}')
    for revision in (REFERENCE, parent):
        # Reuse is justified only when generated cartridge sources agree.
        subprocess.run(['git', 'diff', '--quiet', revision, '--',
                        ':(top,glob)*.c', ':(top,glob)*.h'], cwd=project, check=True)
    args.output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='art-references-', dir=args.output.resolve()))
    print(root, flush=True)
    compiler = next(line.split('=', 1)[1] for line in (build / 'CMakeCache.txt').read_text().splitlines()
                    if line.startswith('CMAKE_CXX_COMPILER:FILEPATH='))
    rt = runtime / 'runtime'
    for label, revision in (('reference', REFERENCE), ('parent', parent)):
        directory = root / label
        directory.mkdir()
        commands = []

        def run(command):
            command = list(map(str, command))
            commands.append(command)
            with (directory / 'build.log').open('a') as log:
                subprocess.run(command, cwd=directory, check=True, stdout=log,
                               stderr=subprocess.STDOUT)

        archive = directory / 'source.tar'
        subprocess.run(['git', 'archive', '--format=tar', '--output=' + str(archive),
                        revision, 'src', 'tests'], cwd=project, check=True)
        with tarfile.open(archive) as stream:
            stream.extractall(directory, filter='data')
        if label == 'reference':
            source = directory / 'src/pallet3d.cpp'
            text = source.read_text()
            if text.count('build=%.2fms') != 1:
                raise RuntimeError('Unexpected reference timing diagnostic')
            source.write_text(text.replace('build=%.2fms', 'build=%.6fms'))
        common = [compiler, *actual, '-DGB_HAS_SDL2', '-DGB_PLATFORM_LINUX',
                  '-I' + str(directory / 'src'), '-I' + str(project),
                  '-I' + str(rt / 'include'), '-I' + str(rt / 'vendor'),
                  '-I' + str(rt / 'vendor/imgui'), '-isystem', '/usr/include/SDL2']
        library = directory / 'libgbrt.a'
        shutil.copy2(build / '_gbrt_build/libgbrt.a', library)
        for filename in ('pallet3d.cpp', 'pallet_presentation.cpp'):
            run(common + ['-c', directory / 'src' / filename, '-o', filename + '.o'])
        run(['ar', 'r', library, 'pallet3d.cpp.o', 'pallet_presentation.cpp.o'])
        for target in ('art_benchmark', 'pallet_render_smoke'):
            source = project / 'tests/art_benchmark.cpp' if target == 'art_benchmark' else directory / 'tests/pallet_render_smoke.cpp'
            definitions = ['-DSDL_MAIN_HANDLED', '-DQA_DETERMINISTIC_SDL_CLOCK']
            if label == 'reference' and target == 'art_benchmark':
                definitions.append('-DART_REFERENCE_RENDERER')
            run(common + definitions + ['-c', source, '-o', target + '.o'])
            run(common + definitions + ['-c', project / 'tests/qa_sdl_clock.cpp', '-o', 'qa_sdl_clock.o'])
            run([compiler, '-Os', '-Wl,--gc-sections', '-s', '-Wl,--wrap=SDL_GetTicks',
                 '-Wl,--wrap=SDL_Delay', target + '.o', 'qa_sdl_clock.o', '-o', target,
                 build / 'libpokeyellow_cart.a', library, '-lcurl', '-lSDL2', '-lGLX', '-lOpenGL'])
        inputs = [archive, build / 'libpokeyellow_cart.a', build / '_gbrt_build/libgbrt.a',
                  *(project / 'tests' / filename for filename in ('art_benchmark.cpp',
                    'art_benchmark_clock.h', 'read_only_memory.h', 'qa_sdl_clock.cpp'))]
        provenance = {'source_commit': revision, 'runtime_commit': RUNTIME, 'commands': commands,
                      'instrumentation': 'build stamp %.6fms only' if label == 'reference' else 'none',
                      'inputs': {str(path): sha(path) for path in inputs},
                      'binaries': {name: sha(directory / name) for name in ('art_benchmark', 'pallet_render_smoke')}}
        (directory / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
        print(label + ': built', flush=True)


if __name__ == '__main__':
    main()
