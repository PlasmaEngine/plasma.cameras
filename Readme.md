# Gameplay Cameras (`plasma.cameras`)

Gameplay cameras for Plasma Engine: views, a camera director that blends between them by priority, and a modifier
stack for framing and effects.

## What is in it

| Kind | Types |
| --- | --- |
| Director | `plCameraDirectorComponent` |
| Views | `plThirdPersonViewComponent`, `plFirstPersonViewComponent`, `plFixedViewComponent`, `plRailViewComponent`, `plTopDownViewComponent` |
| Framing modifiers | Composition (soft center, balanced framing of several subjects), Look At, Look Hint, Focus, Lock-On |
| Effect modifiers | Shake, FOV Kick, Zoom, Recoil, Head Bob, Lean, Landing, Damping |
| Level markup | `plCameraVolumeComponent`, `plCameraPointOfInterestComponent`, camera shake volumes |
| Legacy | `plHeadBoneComponent`, `plCameraShakeComponent` (moved unchanged from Game Components 1.x) |

The user manual is in `Docs/` (start at `Docs/index.html`). Ready-made setups for common kinds of games are in
`Docs/Recipes.html`.

## Layout

```
Source/CamerasPlugin/          runtime plugin (plCamerasPlugin)
  Core/                        pose, views base, director, world module, modifier base, input, debug
  Views/                       the five views
  Modifiers/                   effect and framing modifiers, the framing solver
  Volumes/                     camera volumes, points of interest, shake volumes
  Legacy/                      head bone, camera shake
Source/EditorPluginCameras/    editor plugin: icon folder, hiding unused properties
Docs/                          user manual
EditorIcons/TypeIcons/         component and modifier icons for the editor UI
Assets/Editor/ShapeIcons/      viewport icons
Tests/CamerasTest/             headless checks (console program)
```

## Building

```bash
PLASMA_SDK_ROOT=<engine checkout> plasmabuild build CamerasPlugin
PLASMA_SDK_ROOT=<engine checkout> plasmabuild build EditorPluginCameras
PLASMA_SDK_ROOT=<engine checkout> plasmabuild build CamerasTest
```

## Testing

`CamerasTest.exe` opens no window. Run it with the SDK's binaries on `PATH`:

```bash
PATH="<engine>/Binaries/Windows_Development:$PATH" Bin/Windows_x64_Development/CamerasTest.exe
```

It checks blend curves, projection, aiming, the two-subject framing solver (random cases per layout), the zone
maths, effect modifiers, and the director's request rules (priorities, ties, interruption, expiry, yielding to
sequence cameras) in a real world. It prints one line per check and returns the number of failures.

## Moving from Game Components 1.x

Game Components 2.0.0 no longer contains the third-person view, head bone and camera shake components; they live
here with the same type names and saved data. Game Components 2.0.0 declares `Requires plasma.cameras`, and the
editor offers to add required packages when a project opens. The old include paths still compile through
forwarding headers, but C++ that calls these components has to link against `plCamerasPlugin`.
