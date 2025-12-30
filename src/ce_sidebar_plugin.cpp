#include <windows.h>
#include "cepluginsdk.h"

// Plugin name/version metadata
static const char* kPluginName = "CE Sidebar Dock v1.0.4";

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
local customNames = {} -- handle-string => custom title
local lastSnapshot = {}

local refreshList -- forward declaration

-- 创建侧边栏
local dockForm = createForm(true)
dockForm.Caption = "CE 侧边栏"
dockForm.Width  = 260
dockForm.Height = 460
dockForm.BorderStyle = "bsSingle"

local listBox = createListBox(dockForm)
listBox.Align = "alClient"

local pinBtn = createButton(dockForm)
pinBtn.Caption = "v" -- down = not pinned; up = pinned
pinBtn.Width = 20
pinBtn.Height = 16
pinBtn.Top = 4
pinBtn.Left = dockForm.Width - pinBtn.Width - 12

local popup = createPopupMenu(dockForm)
local miRename = createMenuItem(popup)
miRename.Caption = "重命名"
popup.Items.add(miRename)
local miClose = createMenuItem(popup)
miClose.Caption = "关闭窗口"
popup.Items.add(miClose)
listBox.PopupMenu = popup

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

local function keyOf(f)
    return tostring(f.Handle)
end

local function alive(obj)
    return obj and (obj.destroyed==nil or obj.destroyed==false)
end

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

local function selectedEntry()
    if not alive(listBox) then return nil end
    local idx = listBox.ItemIndex + 1
    return windowCache[idx]
end

local function doRename()
    local entry = selectedEntry()
    if not entry then return end
    local current = customNames[entry.key] or entry.name
    local ok,newName = pcall(function() return inputQuery("重命名","新的显示名称：", current) end)
    if ok and newName and newName~="" then
        customNames[entry.key] = newName
        refreshList()
    end
end

local function doClose()
    local entry = selectedEntry()
    if not entry or not alive(entry.handle) then return end
    pcall(function() entry.handle.close() end)
end

miRename.OnClick = doRename
miClose.OnClick = doClose

local function updatePinPos()
    if not alive(listBox) or not pinBtn then return end
    local itemH = listBox.ItemHeight or 18
    local idx = -1
    local sidebarKey = keyOf(dockForm)
    for i,entry in ipairs(windowCache) do
        if entry.key == sidebarKey or entry.handle == dockForm or entry.name == dockForm.Caption then
            idx = i-1 -- zero-based row for positioning
            break
        end
    end
    if idx < 0 then idx = 0 end

    -- size the pin to roughly match row height
    local btnH = math.max(14, itemH - 4)
    local btnW = math.max(12, btnH - 2)
    pinBtn.Height = btnH
    pinBtn.Width = btnW

    pinBtn.Left = listBox.Left + listBox.Width - pinBtn.Width - 2
    pinBtn.Top = listBox.Top + idx * itemH + math.floor((itemH - pinBtn.Height)/2)
end

local function applyTopmost()
    if ce_sidebar_set_topmost then
        ce_sidebar_set_topmost(dockForm.Handle, pinned)
    end
    if pinBtn then
        pinBtn.Caption = pinned and "^" or "v"
        pinBtn.Hint = pinned and "取消置顶" or "置顶"
    end
end

function refreshList()
    if not alive(listBox) then return end
    listBox.Items.Clear()
    windowCache = {}
    local activeHandles = {}

    for i=0, getFormCount()-1 do
        local f = getForm(i)
        if f and f.Visible and f.Width>0 and f.Height>0 then
            local name = (f.Caption~="" and f.Caption) or f.ClassName
            local key = keyOf(f)
            local disp = customNames[key] or name
            table.insert(windowCache, {handle=f, name=name, key=key})
            listBox.Items[listBox.Items.Count] = shortTitle(disp)
            activeHandles[key] = true
        end
    end

    for k,_ in pairs(customNames) do
        if not activeHandles[k] then
            customNames[k] = nil
        end
    end

    lastSnapshot = snapshot()
    updatePinPos()
end

-- 用 OnMouseUp 触发切换
listBox.OnMouseUp = function(sender, button, shift, x, y)
    if not alive(listBox) then return end
    local idx = listBox.ItemIndex + 1
    if button==0 then -- left click
        if windowCache[idx] then
            windowCache[idx].handle:SetFocus()
        end
    end
    updatePinPos()
end

-- 初始化显示
dockForm:Show()
-- 交给插件设置 WS_EX_APPWINDOW/解除所有者，让 Windows 识别成真正的顶层窗口
if ce_sidebar_make_appwindow then
    ce_sidebar_make_appwindow(dockForm.Handle)
end

pinBtn.OnClick = function()
    pinned = not pinned
    applyTopmost()
    updatePinPos()
end

-- keep pin button at top-right on resize
local prevOnResize = dockForm.OnResize
local function adjustPin()
    if pinBtn then
        updatePinPos()
    end
end
adjustPin()
dockForm.OnResize = function(sender)
    adjustPin()
    if prevOnResize then prevOnResize(sender) end
end

applyTopmost()
refreshList()
reposition()

-- 周期检测
local checker = nil
checker = createTimer(nil, false)
checker.Interval = 200
checker.OnTimer = function()
    if not alive(dockForm) then
        checker.Enabled = false
        return
    end
    mainCE = findMainCE()
    if mainCE and (mainCE.Left~=lastLeft or mainCE.Top~=lastTop) then
        reposition()
    end

    local snap2 = snapshot()
    if hasChanged(lastSnapshot, snap2) then
        refreshList()
    end
    updatePinPos()
end

dockForm.OnClose = function(sender)
    if checker then checker.Enabled = false end
    listBox = nil
    pinBtn = nil
    dockForm = nil
    customNames = {}
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
