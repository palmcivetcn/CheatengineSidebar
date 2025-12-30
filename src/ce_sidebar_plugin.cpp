#include <windows.h>
#include "cepluginsdk.h"

// Plugin name/version metadata
static const char* kPluginName = "CE Sidebar Dock v1.0.0";

static PExportedFunctions g_exports = nullptr;
static int g_pluginId = 0;

// Forward declarations for Lua bindings
static int lua_make_appwindow(lua_State* L);
static int lua_reload_sidebar(lua_State* L);
static int lua_set_topmost(lua_State* L);

// The original Lua sidebar script with a small hook to elevate the form to a true app window.
// Note: Uses CE 7.6 Lua API (createForm, getFormCount, etc.).
static const char* kSidebarLua = R"SIDEBAR(
-- CE 自定义侧边栏（隐藏默认列表 + 精简标题 + 顶层窗口）
local windowCache = {}
local lastSnapshot = {}

-- 创建侧边栏
local dockForm = createForm(true)
dockForm.Caption = "CE 侧边栏"
dockForm.Width  = 260
dockForm.Height = 460
dockForm.BorderStyle = "bsSingle"

local listBox = createListBox(dockForm)
listBox.Align = "alClient"

-- 查找主 CE 窗口
local function findMainCE()
    for i=0, getFormCount()-1 do
        local f = getForm(i)
        if f and f.Caption and f.Caption:match("Cheat Engine") and f.Visible then
            return f
        end
    end
    return nil
end

local mainCE = findMainCE()

-- 隐藏预置的 “CE 窗口列表” 菜单项
if mainCE and mainCE.Menu then
    for i=0, mainCE.Menu.Items.Count-1 do
        local item = mainCE.Menu.Items[i]
        if item.Caption and item.Caption:match("CE 窗口列表") then
            item.Visible = false
        end
    end
end

-- 标题精简函数（最多 15 个字符）
local function shortTitle(name)
    if #name > 15 then
        return name:sub(1,15) .. "…"
    else
        return name
    end
end

-- 贴靠调整
local xOffset = -dockForm.Width - 6
local yOffset = 0
local lastLeft,lastTop = 0,0

local function reposition()
    if mainCE then
        dockForm.Left = mainCE.Left + xOffset
        dockForm.Top  = mainCE.Top  + yOffset
        lastLeft = mainCE.Left
        lastTop  = mainCE.Top
    end
end

-- 快照可见窗口
local function snapshot()
    local t = {}
    for i=0, getFormCount()-1 do
        local f = getForm(i)
        if f and f.Visible and f.Width>0 and f.Height>0 then
            local nm = (f.Caption~="" and f.Caption) or f.ClassName
            t[nm] = true
        end
    end
    return t
end

local function hasChanged(old,new)
    for k,_ in pairs(old) do
        if not new[k] then return true end
    end
    for k,_ in pairs(new) do
        if not old[k] then return true end
    end
    return false
end

local function refreshList()
    listBox.Items.Clear()
    windowCache = {}

    for i=0, getFormCount()-1 do
        local f = getForm(i)
        if f and f.Visible and f.Width>0 and f.Height>0 then
            local name = (f.Caption~="" and f.Caption) or f.ClassName
            table.insert(windowCache, {handle=f, name=name})
            -- 使用简化标题
            listBox.Items[listBox.Items.Count] = shortTitle(name)
        end
    end

    lastSnapshot = snapshot()
end

-- 用 OnMouseUp 触发切换
listBox.OnMouseUp = function(sender, button, shift, x, y)
    local idx = listBox.ItemIndex + 1
    if windowCache[idx] then
        windowCache[idx].handle:SetFocus()
    end
end

-- 初始化显示
dockForm:Show()
-- 交给插件设置 WS_EX_APPWINDOW/解除所有者，让 Windows 识别成真正的顶层窗口
if ce_sidebar_make_appwindow then
    ce_sidebar_make_appwindow(dockForm.Handle)
end
-- 设置置顶，避免被其他 CE 窗口遮挡；需要时可改为 false
if ce_sidebar_set_topmost then
    ce_sidebar_set_topmost(dockForm.Handle, true)
end
refreshList()
reposition()

-- 周期检测
local checker = createTimer(nil, false)
checker.Interval = 200
checker.OnTimer = function()
    mainCE = findMainCE()
    if mainCE and (mainCE.Left~=lastLeft or mainCE.Top~=lastTop) then
        reposition()
    end

    local snap2 = snapshot()
    if hasChanged(lastSnapshot, snap2) then
        refreshList()
    end
end
checker.Enabled = true

print("✔ CE 侧边栏已启用（隐藏默认窗体列表 + 显示精简标题 + 顶层窗口）")
)SIDEBAR";

// Lua: ce_sidebar_make_appwindow(hwnd)
static int lua_make_appwindow(lua_State* L)
{
    if (!lua_isnumber(L, 1))
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    HWND hwnd = (HWND)(UINT_PTR)lua_tointeger(L, 1);
    if (!IsWindow(hwnd))
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    // Remove owner to make the form show up in Alt+Tab
    SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, 0);

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle &= ~WS_EX_TOOLWINDOW;
    exStyle |= WS_EX_APPWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style |= WS_OVERLAPPEDWINDOW;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    lua_pushboolean(L, 1);
    return 1;
}


// Lua: ce_sidebar_set_topmost(hwnd, bool)
static int lua_set_topmost(lua_State* L)
{
    if (!lua_isnumber(L, 1))
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    HWND hwnd = (HWND)(UINT_PTR)lua_tointeger(L, 1);
    if (!IsWindow(hwnd))
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    BOOL topmost = TRUE;
    if (lua_gettop(L) >= 2)
        topmost = lua_toboolean(L, 2);

    SetWindowPos(hwnd,
                 topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    lua_pushboolean(L, 1);
    return 1;
}

// Lua: ce_sidebar_reload() -> rerun the sidebar script
static int lua_reload_sidebar(lua_State* L)
{
    luaL_dostring(L, kSidebarLua);
    return 0;
}

static void register_lua(lua_State* L)
{
    lua_register(L, "ce_sidebar_make_appwindow", lua_make_appwindow);
    lua_register(L, "ce_sidebar_set_topmost", lua_set_topmost);
    lua_register(L, "ce_sidebar_reload", lua_reload_sidebar);
    luaL_dostring(L, kSidebarLua);
}

extern "C" __declspec(dllexport) BOOL __stdcall CEPlugin_GetVersion(PPluginVersion pv, int sizeofpluginversion)
{
    if (!pv || sizeofpluginversion != sizeof(PluginVersion))
        return FALSE;

    pv->version = CESDK_VERSION;
    pv->pluginname = const_cast<char*>(kPluginName);
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL __stdcall CEPlugin_InitializePlugin(PExportedFunctions ef, int pluginid)
{
    g_exports = ef;
    g_pluginId = pluginid;

    if (!ef || !ef->GetLuaState)
        return FALSE;

    lua_State* L = ef->GetLuaState();
    if (L)
    {
        register_lua(L);
    }

    return TRUE;
}

extern "C" __declspec(dllexport) BOOL __stdcall CEPlugin_DisablePlugin(void)
{
    // Optional: we could tear down UI here, but CE will reclaim Lua forms on exit.
    return TRUE;
}
