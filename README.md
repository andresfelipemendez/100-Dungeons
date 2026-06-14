# 100 Dungeons

Monorepo for a hot-reloading C game and the engine that runs it. Two halves:

- **[meikyu](meikyu/)** (迷宮) — the engine. A small hot-reloading C game engine:
  a stable exe that gathers a game's data + C sources and forges them into a
  reloadable dll. Save a file, the running process picks it up — no restart,
  game state survives.
- **[dungeon1](dungeon1/)** — the game. A project: a folder with a
  `project.meikyu` marker and a `src/` tree. No build scripts of its own — the
  engine generates every build input.

The engine is reusable; a game is data. Starting another game is a sibling
folder with a `project.meikyu` and a `src/`.

## Quickstart

```sh
cd meikyu && ./bootstrap.sh     # macOS/Linux (Windows: bootstrap.bat)
./build-mac/meikyu --path ../dungeon1
```

`bootstrap` is the only cmake step (first run also fetches + builds SDL3, a few
minutes). Everything after — game dlls, shaders, ship bundles, even rebuilding
the engine exe — is a generated [kaji](meikyu/lib/kaji) target the running
engine drives. Full build/run/ship instructions: **[meikyu/README.md](meikyu/README.md)**.

## Architecture

The engine is split across a frozen exe↔dll seam and a set of first-party
libraries; the game lives entirely on the reloadable side.

```
exe (stable host)                         dll (reloadable)
  window / GPU / input  ── PlatformApi ──▶   engine systems: render, asset, ui
  dll reload loop                            game src/ (gathered .c files)
  seni migration driver                      hot state: src/game_state.h
  project_gen (marker -> build inputs)       editor tooling (dev only, never shipped)
        │                                            ▲
        └──── kansi sees a save ─▶ kaji forges the dll ─▶ host swaps it in
```

- **exe / dll split** — `src/abi/` is the frozen contract (PlatformMemory/Api,
  GpuContext). Changing it means *restart*, not *reload*. The exe never reads
  game memory; the dll never holds platform pointers across frames.
- **hot reload** — `kansi` reports a debounced save edge; `kaji` (the forge)
  builds the dll as non-blocking child processes; the host atomically swaps it.
  On a `game_state.h` layout change, `seni` diffs the old layout (embedded in
  the running dll) against the new header, codegens + runs a memory migration,
  *then* swaps — new fields zeroed, removed dropped, arrays `min(old,new)`. Any
  failure is logged and the old dll keeps running; state untouched.
- **engine owns builds** — a project ships zero build scripts. On open the
  engine writes `<project>/build-<os>/gen/` (unity .c, kaji + kansi cfgs,
  inspectable), regenerated on every open so engine upgrades propagate. All
  compilation goes through `kaji`.

### First-party libraries — `meikyu/lib/`

Each is standalone with its own `test.sh` suite, named for its function:

| lib | | role |
|-----|-----|------|
| [ito](meikyu/lib/ito) | 糸 | string views + bounded builder — the monorepo's string type |
| [michi](meikyu/lib/michi) | 道 | path views + builder, layered on ito |
| [dodai](meikyu/lib/dodai) | 土台 | the single OS layer (process/files/dl/watch/lock/time); SDL-free |
| [kansi](meikyu/lib/kansi) | 監視 | source watcher: reports a debounced change edge |
| [kaji](meikyu/lib/kaji) | 鍛冶 | the forge: builds targets from a generated description |
| [seni](meikyu/lib/seni) | 遷移 | persistent-state layout differ + migration codegen |
| [horu](meikyu/lib/horu) | 彫 | constructive solid geometry — carve solids from primitives |

### Platform support

- **macOS, Linux:** built and run.
- **Windows:** compiles (CI mingw syntax-check) and path buffers are audited,
  but the build has never been executed on Windows.

## Repo map

```
meikyu/      the engine (exe + libs). see meikyu/README.md
dungeon1/    the game project (project.meikyu marker + src/)
assets/      shared models, bundled into ship builds
vendor/      third-party only: SDL3, cgltf, stb, clay
docs/        design specs + plans (docs/superpowers/)
dev/         scratch / local dev artifacts
postponed/   earlier engine/forge experiments, parked
```
