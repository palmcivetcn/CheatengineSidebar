# CE Sidebar Dock (Cheat Engine 7.6 plugin)

一个把你现有的 Lua 侧边栏脚本包装成真正 Windows 顶层窗口的 CE 插件（v1.0.0）。

## 功能
- 自动隐藏默认的“CE 窗口列表”菜单项，使用精简标题显示 CE 侧边窗口。
- 通过插件层把 Lua 表单提升为 Windows 顶层窗口（`WS_EX_APPWINDOW`），支持 Alt+Tab、多窗口切换。
- 内置 `ce_sidebar_reload()` 方便热重载 Lua 脚本。

## 文件结构
- `src/ce_sidebar_plugin.cpp`：插件主体（C++，基于 CE 7.6 SDK）。
- `cepluginsdk.h`：从 CE 7.6 安装目录 `Cheat Engine\plugin\cepluginsdk.h` 提取的 SDK 头文件（已放在仓库便于直接构建）。

## 编译（示例：VS 2019 / MSVC）
1. 打开 VS 开发者命令行，进入本目录。
2. 依赖：
   - 头文件：`cepluginsdk.h`、`lua.h`、`lualib.h`、`lauxlib.h`、`luaconf.h`（已附，无需再从 CE 目录拷贝）。
   - Lua 静态库：`lua53-64.lib`（64 位）或 `lua53-32.lib`（32 位），从 CE 7.6 `plugin` 目录取。
3. 生成 64 位 DLL（适配 CE 7.6 默认 64 位）：  
   ```powershell
   cl /LD /std:c++17 /EHsc /utf-8 ^
      src\ce_sidebar_plugin.cpp ^
      /I. ^
      "C:\\Program Files\\Cheat Engine\\plugin\\lua53-64.lib"
   ```
   如果使用 32 位 CE，请改用 `lua53-32.lib` 并用 32 位工具链。
4. 将编译好的 DLL 复制到 `Cheat Engine 7.6\plugins\`。

## 使用
1. 启动 CE 7.6，插件会自动加载并执行内置 Lua，输出 `CE Sidebar Dock plugin loaded (v1.0.0)`。
2. 屏幕左侧会出现“CE 侧边栏”窗口（可 Alt+Tab）。点击列表项可切换焦点到对应 CE 窗口。
3. 如果需要重新加载脚本，可在 CE Lua 控制台执行：  
   ```lua
   ce_sidebar_reload()
   ```
4. 若需调整贴靠偏移、标题长度等，可直接修改 `kSidebarLua` 字符串中的对应变量并重新编译 DLL。

## 注意事项
- 仅针对 CE 7.6 API 编写；其他版本接口可能不同。
- 插件通过 WinAPI 去除窗口所有者并添加 `WS_EX_APPWINDOW`，如遇特殊皮肤或多显示器异常，可尝试在 Lua 中调整 `xOffset/yOffset` 或在 C++ 中去掉 `WS_OVERLAPPEDWINDOW` 设置。
- 卸载：从 `plugins` 目录移除 DLL，重启 CE。

迭代版本 v1.0.0 + 初始发布
