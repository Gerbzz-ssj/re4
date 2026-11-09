# Desktop port plan

Milestone 1 is a native Windows build of the decompiled game. The end goal is a mixed-reality PC VR
"diorama" build: the game world as a miniature in the player's real room, with Leon about 2.5 ft
(0.76 m) tall. Every decision below is made so that step is an extension, not a rewrite.

The matching GameCube build stays untouched. All port code is behind `#ifdef TARGET_PC` or lives in
new directories (`port/`), and `python3 configure.py && ninja` must still produce 100% matching bytes.

## What the game code needs from the platform

Measured over `src/` excluding `src/lib/` (465k lines):

| Area | Distinct functions | Call sites | Plan |
|---|---|---|---|
| PS math (`PSMTX*`, `PSVEC*`) | 30 | 5,553 | Map to the SDK's portable `C_MTX`/`C_VEC` versions |
| GX graphics | 110 | 3,922 | Aurora (GX on Vulkan / D3D12) |
| OS (heaps, threads, time, interrupts) | 61 | 498 | Aurora's OS layer + port shims |
| AX / AI / DSP audio | 46 | ~100 | Port Capcom's `snd*` driver onto an AX shim over SDL audio |
| ARAM (`AR*`, `ARQ*`) | 9 | ~20 | Emulate as a plain heap block with memcpy |
| DVD | 18 | 32 | Read from an extracted disc folder |
| PAD / SI | 11 | 74 | Aurora (SDL gamepad) |
| CARD | 17 | 35 | Save files in the user folder |
| VI | 12 | 32 | Aurora window / present |
| CRI Sofdec / ADX movies | (src/lib) | — | Stub at first (skip movies), later decode with FFmpeg |

## Compatibility layer: Aurora

[Aurora](https://github.com/encounter/aurora) (MIT) is a source-level GameCube/Wii compatibility layer
made for decompilation ports (Metaforce, Dusk). It implements GX, VI, PAD, SI and parts of OS on SDL with
D3D12 / Vulkan / Metal. Writing our own GX-to-GPU layer would cost months; Aurora covers it.

For VR the Vulkan backend matters: OpenXR hands out Vulkan swapchain images, so the diorama build
renders the GX scene twice (one view/projection per eye) into those images. That needs one hook in the
renderer: the GX projection and view matrices the game sets must be overridable per eye. Plan for it from
the start by routing `GXSetProjection` and the game camera (`pG->Cam`, written by `CameraMove`) through a
port-owned `RenderView`.

## Code problems to solve

1. **Compiler-matching hacks.** 562 `asm` blocks/statements and `register T x asm("rN")` pins, plus
   526 `// COMPILER-DIFF:` spots. A PC header (`port/include/pc_compat.h`, force-included) turns the
   pins and empty `asm("")` into nothing; the remaining real assembly (paired-single math, GQR setup,
   cache/SPR code, ~231 instructions in game code) gets C replacements under `#ifdef TARGET_PC`.
2. **Endianness.** All disc data is big-endian. Every loader that casts a file buffer to a struct
   (models, motions `FCV`, room archive blocks `SMD`/`SAR`/`CAM`/..., TPL textures, EST/ESL tables)
   needs a byte-swap pass on load. This is the largest single job. Do it per format, with a swap table
   generated from the reconstructed structs in `include/`.
3. **Pointer size.** 1,617 casts between pointers and 32-bit ints, and file formats that store 32-bit
   offsets that get relocated into pointers in place. Milestone 1 builds as **32-bit x86 (Win32)** so
   these work unchanged; moving to x64 is its own milestone before VR (OpenXR runtimes are 64-bit).
4. **REL overlays.** The 114 RELs (rooms, enemies, weapons, Sscrn, tools) are linked statically into
   one executable; `OSLink`/module-load calls resolve to a table of each module's prolog/epilog.
5. **Case collisions.** `src/Tools/{t_prim,t_util,tools}.cpp` collide with `src/tools/` on Windows.
   Debug tool modules are left out of milestone 1.

## Status

Build with `.\port\build.ps1` (32-bit, clang 23 targeting `i686-pc-windows-msvc`, Ninja).

- **`src/game`: 285 of 295 C++ units compile** into `re4_game.lib`. The other 10 contain real PowerPC
  asm and are listed in `PC_ASM_UNITS` in `port/CMakeLists.txt` until they have C versions. The 61
  newlib `.c` units are replaced by the host C runtime.
- PC fixes so far: `port/include/pc_compat.h` (asm removal, C-style `strstr` & co.), a `TARGET_PC`
  branch for `fabsf` in `include/math_sub.h`, and an explicit function-pointer cast in `esp0a.cpp`
  (the same cast `esp.cpp` uses; emits no code).
- **508 symbols are still undefined.** 137 are game C++ defined in units not yet built (the 10 asm
  units, the em/wep/pl/st/Sscrn modules). The rest is the platform: GX 89, AX 49, OS 39, CARD 17,
  DVD 15, PSMTX/PSVEC/C_MTX 30, VI 8, PPC 8, PAD 6, CRI 16, compiler runtime 25, other C 56. The full
  list is written to `port/build/undefined.txt`.

## Milestones

1. **Boot to title.** CMake build (clang-cl, Win32) of `src/game` + one room, Aurora for GX/VI/PAD,
   stubs for audio/movies/CARD, DVD reading from an extracted disc folder. Success = title screen renders.
2. **Playable village.** Endian swaps for every format the `st1_*` rooms use, all em/wep modules, ARAM
   emulation, input mapping.
3. **Sound and saves.** AX shim on SDL audio, CARD to files, ADX/Sofdec movies via FFmpeg.
4. **x64.** Remove the 32-bit pointer assumptions.
5. **Diorama VR.** OpenXR on the Vulkan backend, passthrough (`XR_FB_passthrough` / runtime
   equivalents), a free tabletop camera decoupled from `CameraQuasiFPS`, and one world scale factor.
   Leon's model height in game units divided into 0.76 m sets that factor. Controls: world grab/rotate,
   controller or gamepad for Leon.

## Game data

The repository never contains game files. Everyone supplies their own discs:

1. Build the extractor (`port/tools/gcextract.cpp`, reads `.iso`/`.gcm`/`.ciso`).
2. Extract each disc **outside the repository**: `gcextract disc1.ciso <data>/disc1`, same for disc 2.
3. Point the port at `<data>` at run time (command line or config file). Nothing is copied into the build.

`.gitignore` blocks disc images, archives and extracted data, and `tools/check_no_game_data.py` runs in CI
(`.github/workflows/no-game-data.yml`) and fails on any game file, extension or signature, even if force-added.

### Retail vs debug discs

The decompiled code is the Nov 2004 debug prototype. The USA retail release (also `G4BE08`) is different:
none of its `main.dol`/REL hashes match `config/G4BE08/build.sha1`, enemies and weapons are linked into
`main.dol` (19 code files vs 115), and some data was renamed or repacked (`em/emNN.das` became `em/emNN.drs`).
Of 183 data paths written literally in the source, 37 exist on the retail discs.

The port links all code statically, so the REL layout doesn't matter. The data does: with retail discs, every
format that changed between prototype and retail needs a loader adapter. The debug discs need none.

## What you need

- Your own disc images, extracted with `gcextract` to a folder outside the repo (see Game data).
- On Windows: Visual Studio 2022 Build Tools (C++ workload and Windows SDK), CMake, Ninja, LLVM
  (clang-cl), and the Vulkan SDK.
