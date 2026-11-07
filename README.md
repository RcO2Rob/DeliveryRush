# Delivery Rush

Ride a bicycle across a rolling 3D city and park and deliver an urgent parcel before the
per-order deadline. The campaign has one teaching delivery followed by three
deliveries in each of stages two and three; the last stop is the clinic.
Each paid order earns 100 cash and retries preserve completed orders. Traffic
gets faster and more frequent with each delivery, with a higher simultaneous
vehicle limit. Stage two allows 90 seconds per order and only one-pass traffic
from ahead. Stage three allows 60 seconds per order and adds rear pursuit.
Each stage-two order has at most one clock (+10 seconds) and one snowflake;
stage-three orders have up to two separated alley coins (+10 seconds each)
and one snowflake. Snowflakes stop cars,
pedestrians and new traffic for five seconds while cycling and the deadline
continue. Pickups do not respawn during an order.
Choose streets, turn at intersections, take an alley or
turn back. Incoming cars try to ram the courier and keep pursuing after a miss.
After a miss, the same car brakes, recovers behind the rider and accelerates
for another charge. On an open route the next close approach is tuned for
3–4 seconds after the miss, including when the bicycle is at full speed.
Buildings and detours can extend this interval; cars still obey collision.

In stage three the first group has three cars. Further two-car groups enter ahead
after at least 8 / 6.5 / 5 seconds and 40 / 35 / 30 metres of riding for orders
one / two / three respectively. Existing
cars keep pursuing from behind, so front and rear attacks can overlap. At most
5 / 6 / 8 cars pursue simultaneously for the respective orders; a due group waits for room and two clear
positions ahead. New cars that miss also join the rear pursuit cycle.

A pursuer can overturn after a hard obstacle impact or a heavy collision with
another car. Steering and U-turns alone never overturn a car. Wrecks stay at the crash site and leave
pursuit permanently for the rest of the attempt. Later waves use distinct new
cars. The delivery and its original deadline continue between waves.

A solid hit knocks the bicycle down, then automatically restarts the delivery
from the current order's checkpoint. A light brush can push and wobble the bike without ending
the attempt. There are no attack warning overlays, warning sounds, parcel health
points.

## Run and edit on Windows

See [Windows开发指南](Windows开发指南.md) for setup. This project and its saved maps require UE 5.8; install the Windows C++ toolchain through Visual Studio Installer.
Double-click **Check Setup.cmd** to inspect prerequisites, **Open Demo.cmd** to compile and edit, or **Play Demo.cmd** to compile and play.

## Run and edit on macOS

Install Unreal Engine 5.8, Python 3 and a full Xcode installation. To edit the
project, double-click **Open Demo.command**.
The default map is now **DeliveryEditable**. Buildings, trees and rocks can be
moved, rotated, scaled, duplicated or deleted; gameplay reads their collidable
mesh bounds on each Play. Roads use editable splines, and hill actors control
the terrain. See [地图编辑指南](地图编辑指南.md).

Double-click **Play Demo.command**, click the game window and press **Enter**.
Use **Open Demo.command** to edit the native city/park map in Unreal.

| Key | Action |
| --- | --- |
| Enter | Start; resume; retry after timeout |
| W / Up | Pedal forward |
| A, D / Left, Right | Turn the bicycle |
| S / Down | Brake, then reverse |
| Space | Brake |
| Right mouse + mouse movement | Look around |
| P / Escape | Pause; Escape can stop PIE in the editor |
| R | Restart from the beginning |

The bicycle now reaches 20 m/s (72 km/h), with acceleration of 6 m/s²
and 18% more steering response than v0.5. Braking is also a little stronger.
The ground includes broad grass hills and gentler city rises. Bicycle and car
height, pitch and bank follow the same height field as the terrain collider.
The game uses a baked native static mesh for the terrain's visible surface.
The chase camera keeps a level horizon while following the rider.

Steering changes world heading. Releasing the pedals allows the bike to coast
to a stop. The camera follows the bicycle's orientation. The navigation map
shows roads, the rider and the destination, without enemy markers.

## Project

The project uses UE 5.8 build settings and C++20. Gameplay and impacts use an arcade simulation; this
is not a physically simulated Chaos bicycle or ragdoll.

`Design/city_layout.json` now supplies the preserved template and portable test
fixtures. The editable game's live obstacles, start, goal, roads and ground come
from level actors and actual terrain collision, rather than those template
coordinates. Ordinary collidable static-mesh props use oriented rectangular
footprints; roof/window/crown components are decoration. Grass remains traversable.
Cars use a visibility graph and shortest routes through streets and park paths,
with controlled recovery turns and faster rear catch-up runs. New cars
enter on visible roads ahead; existing pursuers are never removed just because
they passed the bicycle. The initial group enters together after the rider has
covered 30 metres; a straight, continuously accelerating rider meets the first
collision about five seconds after starting. Different routes and braking change
that timing. Low-speed bumps and controlled turns do not automatically count as
overturns. Car rollover uses an arcade load/impact model and a visible roll
animation, rather than Chaos rigid-body vehicle physics.

```sh
python3 tools/ue.py play       # Build and open the standalone demo
python3 tools/ue.py editor     # Open the native editor project
python3 tools/ue.py prepare    # Build and generate missing native content
python3 tools/check_demo.py    # Run the production simulation checks
python3 tools/ue.py package    # Package for the current host; Mac arm64 verified
```

The current map is `/Game/Maps/DeliveryEditable`. Preparation preserves existing
edited maps. Editable terrain visuals are saved per map.
