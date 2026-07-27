# GameAnimationSample

Unreal Engine **5.8** fork of Epic's Game Animation Sample, with additional editor plugins and C++ experiments (Cinematic QTE, Property History, Electronic Nodes, etc.).

## Important: Content is not in this repository

Binary Unreal assets under `Content/` are **gitignored** (large `.uasset` / `.umap` files). Cloning this repo alone gives you source, config, and project plugins — **not** a playable sample.

You must obtain Content from Epic's official sample, then place it into this project.

### Get Content from Fab / Epic

1. Open the [Epic Games Launcher](https://store.epicgames.com/en-US/download) → **Unreal Engine** → **Fab** / Library (or open [Fab](https://www.fab.com/) in a browser).
2. Search for **Game Animation Sample** and add / download the package for **Unreal Engine 5.8** (or the closest matching 5.x sample you will retarget).
3. Create or open the official sample once so Epic finishes downloading assets (typically under something like `Documents/Unreal Projects/GameAnimationSample 5.8`).

Official sample on Fab (UE 5.4–5.8):

- [Game Animation Sample on Fab](https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016)
- Or search `Game Animation Sample` in Epic Games Launcher → Fab / Library

### Merge Content into this project

From the official sample folder, copy the entire `Content` directory into this repository root:

```text
<OfficialSample>/Content  →  <ThisRepo>/Content
```

Recommended (Windows PowerShell / cmd):

```powershell
robocopy "C:\Users\<You>\Documents\Unreal Projects\GameAnimationSample 5.8\Content" ".\Content" /E
```

Optional but recommended: also sync engine-facing config from the official sample if your maps or plugins fail to load (especially `Config/DefaultEngine.ini`, device profiles, Network Prediction settings). Do **not** overwrite this repo's custom plugin enablement in `GameAnimationSample.uproject` without reviewing the diff.

After copying, you should have:

```text
GameAnimationSample/
  Content/          # from Fab / official sample (local only)
  Config/
  Source/
  Plugins/
  GameAnimationSample.uproject
```

## Requirements

- Unreal Engine **5.8** (binary from Epic Games Launcher)
- Visual Studio 2022 or later with C++ desktop workload (VS 2026 also works)
- Windows 10/11
- Git (and ideally Git LFS awareness for other tools; Content itself is not stored here)

## Getting started

1. Clone this repository.
2. Install / download **Game Animation Sample** Content via Fab / Epic Launcher (see above).
3. Copy official `Content/` into this project root.
4. Right-click `GameAnimationSample.uproject` → **Generate Visual Studio project files**, or open the `.uproject` with UE 5.8 and allow module compile.
5. Build target `GameAnimationSampleEditor` (Development | Win64) if prompted.
6. Open the editor and Play (`DefaultLevel` when Content is present).

Command-line build example:

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  GameAnimationSampleEditor Win64 Development `
  -Project="$PWD\GameAnimationSample.uproject" -WaitMutex
```

## Project structure

| Path | Description |
|------|-------------|
| `Source/` | Game module C++ (character movement helpers, etc.) |
| `Content/` | **Not in git** — maps, animations, Blueprints from Epic sample |
| `Config/` | Project `.ini` settings |
| `Plugins/CinematicQTE/` | Cinematic QTE system (Level Sequence integration) |
| `Plugins/PropertyHistory/` | Inline property history in Details (editor) |
| `Plugins/ElectronicNodes/` | Blueprint / material wire style (editor) |

## Plugins

### Engine / sample plugins (enabled in `.uproject`)

AnimationWarping, PoseSearch, AnimationLocomotionLibrary, MotionWarping, HairStrands, Chooser, RigLogic, LiveLink, LiveLinkControlRig, Mover, NetworkPrediction, SmartObjects, Locomotor, and related sample plugins.

### Project plugins (this fork)

- **CinematicQTE** — QTE tracks/sections for Level Sequences
- **PropertyHistory** — right-click a property → **See history** (requires Source Control / Git)
- **ElectronicNodes** — circuit-style graph wires in Blueprint / Material editors

See each plugin's own `README.md` under `Plugins/` for details.

## Features (with Content installed)

- Character locomotion / motion matching sample maps
- Animation warping and Pose Search
- MetaHuman-related sample content (from Epic package)
- Extra tooling from project plugins above

## Troubleshooting

| Issue | What to check |
|-------|----------------|
| Empty / missing maps | `Content/` not copied from the official sample |
| Engine version mismatch dialog | Project targets **5.8**; install UE 5.8 or retarget carefully |
| Compile errors after pull | Generate project files, rebuild Editor target |
| Property History empty | Enable Git / Source Control in the editor and commit assets that live outside git carefully |
| PoseSearch / PoseHistory log spam | Usually Content / AnimBP mismatch; ensure you copied a matching 5.8 sample Content pack |

## License

- Epic Game Animation Sample Content remains subject to Epic's sample / Fab license terms.
- Code and plugins in this repository are for educational / development use unless otherwise noted in individual plugin licenses (`Plugins/*/LICENSE`).
