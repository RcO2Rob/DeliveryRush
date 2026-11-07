# 在 Windows 上运行和制作 Delivery Rush

本指南针对根目录的自行车配送游戏 `DeliveryRush.uproject`。

## 首次配置

1. 在 Epic Games Launcher 的 Unreal Engine → 库中安装 **UE 5.8**，优先与 Mac 上的 **5.8.4** 对齐。安装位置可以选 E 盘；启动工具会读取 Windows 注册表和 Epic 安装记录。当前地图由 UE 5.8 保存，电脑原有的 UE 5.7.1 无法直接使用。
2. 打开 **Visual Studio Installer**，找到 Visual Studio Community 2022，点击“修改”。勾选 **使用 C++ 的游戏开发 / Game development with C++**，并在安装详情中确认选中 **MSVC v143 x64/x86 C++ 生成工具**及 **Windows 10/11 SDK**。按 UE 5.8 的安装要求选取工具链版本。

   项目提供 `.vsconfig` 安装配置，也可在 Visual Studio Installer 的“更多 → 导入配置”中选择它。
3. 双击 **Check Setup.cmd**。`engine_compatible` 应为 `true`，`windows_toolchain.available` 应为 `true`；精确工具链版本是否适用仍由首次 Unreal 编译确认。
4. 双击 **Open Demo.cmd**。首次启动会编译 Windows C++ 模块，随后打开 `/Game/Maps/DeliveryEditable`。首次着色器编译可能需要等待。

启动脚本需要 Python 3，推荐使用 Python 3.11，并将 Python 加入 PATH。
无需修改 `.uproject` 的版本号来迁就 5.7；这样不会转换地图。

## 每天制作游戏

| 要做的事 | 入口 |
| --- | --- |
| 打开关卡、移动建筑、制作地图 | 双击 `Open Demo.cmd` |
| 用独立窗口试玩当前工程 | 双击 `Play Demo.cmd` |
| 查看引擎和编译工具是否就绪 | 双击 `Check Setup.cmd` |
| 只编译 C++ | `python -X utf8 tools/ue.py build` |
| 检查运动、追击和碰撞模拟 | `python -X utf8 tools/check_demo.py`（独立模拟检查，不会打开引擎） |
| 导出 Windows 游戏 | `python -X utf8 tools/ue.py package`，输出到 `Builds/Win64` |

编辑器打开后，先在世界大纲找到 `Delivery/Buildings`，选一栋建筑，按 **W 移动、E 旋转、R 缩放**。
停止试玩后修改，保存，再点顶部 Play，点击游戏画面并按 Enter 开始。可先“另存关卡”保留地图副本；启动入口默认仍打开 `DeliveryEditable`。
山丘、道路、起点、终点、倒计时的编辑方法见 [地图编辑指南](地图编辑指南.md)。

## 改玩法代码

可用 VS Code 打开整个项目文件夹，或使用 Visual Studio 的 Unreal 工程支持。

| 文件 | 用途 |
| --- | --- |
| `Source/DeliveryRush/RideSimulation.cpp` | 自行车运动、敌车追击与碰撞等模拟规则 |
| `Source/DeliveryRush/DeliveryActors.h`、`.cpp` | 自行车、敌车、游戏流程、画面和输入逻辑 |
| `Source/DeliveryRush/DeliveryLevel.h`、`.cpp` | 可编辑的道路、山丘、关卡设置，以及读取关卡障碍 |
| `Content/Maps/DeliveryEditable.umap` | 当前地图，只用 Unreal 编辑器修改 |

修改 C++ 后先保存并关闭编辑器，再双击 `Open Demo.cmd` 重新编译、打开。首次建议完整编译，减少热重载造成的混淆。
纯地图修改无需手工改 C++ 或运行地图生成脚本。不要把重新生成模板当作日常保存地图的方法。

如果引擎装在自定义目录，通常能自动识别；也可从终端指定：

```powershell
python -X utf8 tools/ue.py doctor --engine "E:\Epic Games\UE_5.8"
python -X utf8 tools/ue.py editor --engine "E:\Epic Games\UE_5.8"
```

运行 `python -X utf8 tools/check_demo.py` 可验证配送、计时和车辆逻辑。
