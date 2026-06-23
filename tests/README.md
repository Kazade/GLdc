# GLdc test suite

The tests are plain C++ headers under `tests/`. Every class deriving from
`test::TestCase` (or `GLTestCase`) is auto-discovered (see
`tools/test_generator.py`) and each `void test_*()` method becomes a test case.
They build into the `gldc_tests` binary.

The same tests run on two backends:

  * **desktop / software** backend — fast iteration in CI;
  * **Dreamcast / kospvr** backend — the real target, run under an emulator.

Suites that drive the GL API derive from **`GLTestCase`** (`tools/gl_test.h`),
which initialises the GPU exactly once and resets GL state before each test
(see "Running once on the Dreamcast" below). Pure non-GL suites (e.g. the
standalone allocator tests) derive from `test::TestCase` directly.

## Desktop

```sh
# configure + build (32-bit desktop build)
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make gldc_tests

# run everything (headless)
SDL_VIDEODRIVER=dummy ./tests/gldc_tests

# run a single suite (prefix match on "Suite::test")
SDL_VIDEODRIVER=dummy ./tests/gldc_tests TextureFormatTests
```

## Dreamcast

Build with the KallistiOS toolchain inside the `kazade/dreamcast-sdk`
container, then run the resulting `.elf` under the
[nitrocast](https://gitlab.com/simulant/nitrocast) emulator:

```sh
# build (from the repo root; KOS_BASE is set inside the container)
podman run --rm -v "$PWD":"$PWD":Z -w "$PWD" localhost/kazade/dreamcast-sdk \
    /bin/sh -c "source /etc/bash.bashrc; \
        mkdir -p dcbuild && cd dcbuild && \
        cmake -DCMAKE_TOOLCHAIN_FILE=../toolchains/Dreamcast.cmake \
              -DCMAKE_BUILD_TYPE=Release -DBUILD_SAMPLES=OFF .. && \
        make gldc_tests"

# run on the emulator. -u enables dcload host-file syscalls and -C mounts the
# given directory as the dcload filesystem (KOS sees it as /pc), which is where
# the golden references are read from. The build copies goldens/ next to the
# .elf, so run from there:
cd dcbuild/tests
nitrocast -b -e gldc_tests.elf -u -C . -r 32
```

The allocator unit tests are desktop-only (they assert exact host heap
addresses); they are automatically excluded from the Dreamcast build.

### Running once on the Dreamcast

`InitGPU()`/`ShutdownGPU()` map to `pvr_init()`/`pvr_shutdown()`, which must not
be torn down and brought back up between tests. `GLTestCase` therefore opens the
PVR (and the display) lazily on the first test, registers an `atexit()` handler
to shut it down once at program exit, and only *resets* GL state between tests.
New GL-driving suites should derive from `GLTestCase`; if they need extra setup
they should override `set_up()` and call `GLTestCase::set_up()` first.

## What's covered

| File | Focus |
|------|-------|
| `test_glcolor.h`              | `glColor*` / `glColorPointer` state |
| `test_glteximage2d.h`         | basic internal-format selection |
| `test_pvr_vertex_submission.h`| TA poly-list structure & headers |
| `test_vertex_formats.h`       | `glVertexPointer` types/sizes/strides, immediate mode, `glDrawElements` |
| `test_texcoord_formats.h`     | `glTexCoordPointer` type scaling, immediate `glTexCoord` |
| `test_texture_formats.h`      | byte-exact texture conversion (RGB565 / ARGB4444 / ARGB1555 / RGBA8 / RED / ALPHA / paletted), `glTexSubImage2D`, errors |
| `test_golden_rendering.h`     | end-to-end rendered-output comparison |

The format/submission tests work by inspecting the internal state the driver
produces — the converted texture bytes (`TextureObject::data`) and the submitted
vertices in `OP_LIST` / `PT_LIST` / `TR_LIST` — so they are exact and
deterministic, and they pin down the per-type readers and pixel conversions
without needing a framebuffer.

## Golden-image tests

`tools/golden.h` contains a small, self-contained, deterministic CPU rasteriser.
It consumes the **same** TA poly-lists the real backend submits and reproduces
the backend's triangle-strip walk and perspective divide, then fills triangles
with Gouraud vertex colour (optionally modulated by a decoded texture). Output is
compared against committed PPM references in `tests/goldens/`.

Because the rasteriser lives in the test harness (not the library) it doesn't
move when the library changes, so a diff in the rendered output reliably points
at a regression in GLdc's transform / colour / clipping / submission pipeline.
The poly-lists are produced by the real (SH4-compiled) library, so the same
references are validated against actual Dreamcast output too — on the Dreamcast
the textured cases even sample the decoded texture straight out of PVR VRAM.
Comparison is tolerant (per-channel + mismatch-fraction thresholds) so sub-LSB
rounding never causes flakiness.

The references live in `tests/goldens/`. On the desktop they are read straight
from the source tree; on the Dreamcast they are read through the dcload mount at
`/pc/goldens` (the build copies them next to the `.elf`, so `-C .` from the
build directory exposes them).

### Adding or updating a golden

1. Write a test that draws something and calls
   `golden::rasterize_all_lists(img)` then
   `assert_true(golden::check(img, "my_scene"))`.
2. Generate (or refresh) the reference image (desktop):

   ```sh
   GLDC_UPDATE_GOLDENS=1 SDL_VIDEODRIVER=dummy ./tests/gldc_tests GoldenRenderingTests
   ```

3. **Eyeball the new `tests/goldens/my_scene.ppm`** before committing it
   (any image viewer / `convert ... png` works) and commit it alongside the test.

On a mismatch the harness writes `my_scene.actual.ppm` and `my_scene.diff.ppm`
(differing pixels highlighted red) next to the golden for inspection.

> Note: the video mode is 640×480 and the viewport y-flip is relative to that
> height, so golden scenes place the viewport at `(0, 480 - H)` to capture a
> W×H image at the top-left of the framebuffer.
