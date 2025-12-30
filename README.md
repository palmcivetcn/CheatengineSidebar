# CE Sidebar Dock (Cheat Engine 7.6 plugin)

一个侧边栏显示当前CE显示所有页面并支持点击切换的 CE 插件（可被windows识别为独立窗口，方便切换使用）
## 实机演示
https://github.com/user-attachments/assets/2f89748d-c55d-4219-aa8c-5f89030f22b2
## 功能
- 自动隐藏默认的“CE 窗口列表”菜单项，使用精简标题显示 CE 侧边窗口。
- 通过插件层把 Lua 表单提升为 Windows 顶层窗口（`WS_EX_APPWINDOW`），支持 Alt+Tab、多窗口切换。
- 内置 `ce_sidebar_reload()` 方便热重载 Lua 脚本。

## 文件结构
- `src/ce_sidebar_plugin.cpp`：插件主体（C++，基于 CE 7.6 SDK）。
- `cepluginsdk.h`：从 CE 7.6 安装目录 `Cheat Engine\plugin\cepluginsdk.h` 提取的 SDK 头文件（已放在仓库便于直接构建）。

## 使用
1. 将relase的ce_sidebar_plugin.dll下载
2.复制到ce根目录\plugins下即可
3.打开ce修改器，点击编辑->设置->插件->添加新项
<img width="692" height="584" alt="image" src="https://github.com/user-attachments/assets/03f638b1-53dd-4e5b-9849-563ce06f4aa6" />
4.找到你刚才复制好的文件，随后确定
5.千万**别忘了**去勾选上并点击下方的确定哦
6.重启ce，开心食用吧！

## 注意事项
- 仅针对 CE 7.6 API 编写；其他版本接口可能不同。
- 插件通过 WinAPI 去除窗口所有者并添加 `WS_EX_APPWINDOW`，如遇特殊皮肤或多显示器异常，可尝试在 Lua 中调整 `xOffset/yOffset` 或在 C++ 中去掉 `WS_OVERLAPPEDWINDOW` 设置。
- 卸载：从 `plugins` 目录移除 DLL，重启 CE。

## 开发者留言
还有点小bug，欢迎大家提pr和issue，或者是其他的建议，有时间会更新，提pr会审。实际作用源代码在src里面，文件有点小乱
