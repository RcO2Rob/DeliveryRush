# Delivery Rush

Deliver parcels by bicycle before time runs out. Follow the green beacon and dodge pursuing cars and pedestrians. A time coin adds 10 seconds. A snowflake freezes the countdown and traffic for 3 seconds while you keep riding.

## Campaign

| Stage | Objective |
| --- | --- |
| 1. Riding Lesson | Make one delivery and practice using pickups while dodging a slower car. |
| 2. Oncoming Traffic | Make three deliveries while dodging groups of two or three cars from ahead. |
| 3. City Rush | Make three deliveries while dodging front and rear attacks. The last stop is the clinic. |
| 4. Park Crossing | Cross both bridges, pass each green checkpoint on land, and deliver to the picnic area. |

Each order starts with 60 seconds. Orders in stages 2 and 3 pay 100 each, and retries preserve completed deliveries. The next stage starts automatically after the results screen. The final results screen has a **Main Menu** button.

| Input | Action |
| --- | --- |
| W / Up | Pedal |
| A, D / Left, Right | Steer |
| S / Down | Brake, then reverse |
| Space | Brake |
| Hold right mouse button and move | Look around |
| Esc or P | Pause |
| Enter | Start, resume, or retry after timeout |
| R | Retry the current order during play |

Choose **Quit Game** from the main menu or pause menu to exit.

## Run from source

Install Unreal Engine 5.8 and Python 3. On Windows, add Python to `PATH` and install the Visual Studio **Game development with C++** workload with MSVC x64/x86 tools and a Windows SDK. Visual Studio Installer can import [`.vsconfig`](.vsconfig). On macOS, install and activate the full Xcode toolchain.

Run terminal commands from the repository root.

| Task | Windows | macOS |
| --- | --- | --- |
| Check setup | `python -X utf8 tools/ue.py doctor` | `python3 tools/ue.py doctor` |
| Open Unreal Editor | `python -X utf8 tools/ue.py editor` | `Open Demo.command` |
| Play in a standalone window | `python -X utf8 tools/ue.py play` | `Play Demo.command` |

For a custom engine location, run `python tools/ue.py editor --engine "<engine-directory>"` on Windows or use `python3` on macOS. To run the standalone C++ simulation checks, use `python tools/check_demo.py`. To package for the current platform, run `python tools/ue.py package`. Packages are written to `Builds/`.

## Edit the maps

The campaign starts on `DeliveryEditable` and switches to `DeliveryPark` for stage 4. Find both levels in `Content/Maps`. In the editor's Outliner, expand `Delivery` to find buildings, roads, hills, and level settings. Edit road shapes with their spline points. Use collision on props that should block riders and cars, and use `NoCollision` for decoration. Save the level and start a fresh Play session to update collision, navigation, and the minimap.

## Asset credits

- Quaternius's [Casual Hoodie character](https://quaternius.com/packs/ultimatemodularcharacters.html) (CC0), adapted for cycling.
- Two vehicles from Quaternius's [Cars Pack](https://quaternius.com/packs/cars.html) (CC0).
