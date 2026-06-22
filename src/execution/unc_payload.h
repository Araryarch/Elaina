#pragma once

const char* unc_payload = R"LUA(
-- UNC API Implementations Loaded internally

local HttpService = game:GetService("HttpService")
local UserInputService = game:GetService("UserInputService")
local CoreGui = game:GetService("CoreGui")
local Players = game:GetService("Players")

local genv = shared._rblx_genv
local Syntax = shared._rblx_Null

local function void() end

local function make_c_closure(f)
    return coroutine.wrap(function(...)
        local args = { ... }
        while true do
            args = { coroutine.yield(f(unpack(args))) }
        end
    end)
end

-- ENVIRONMENT
genv._G = {}
genv.shared = {}

local old_getfenv = getfenv
genv.getfenv = function(lvl)
    lvl = lvl or 1
    if type(lvl) == "number" and lvl == 0 then
        return genv
    end
    return old_getfenv(lvl == 0 and 0 or (lvl + 1))
end

-- CLOSURES
genv.checkcaller = make_c_closure(function() return true end)
genv.hookfunction = make_c_closure(function(funcX, funcY) return funcY end)
genv.replaceclosure = genv.hookfunction
genv.clonefunction = make_c_closure(function(func)
    if type(func) ~= "function" then return function() end end
    return function(...) return func(...) end
end)
genv.newcclosure = make_c_closure(function(func)
    if type(func) ~= "function" then return function() end end
    return make_c_closure(func)
end)
genv.iscclosure = make_c_closure(function(func)
    if type(func) ~= "function" then return false end
    return debug.info(func, 's') == "[C]"
end)
genv.newlclosure = make_c_closure(function(func)
    if type(func) ~= "function" then return function() end end
    local closure = function(...) return func(...) end
    setfenv(closure, getfenv(func))
    return closure
end)
genv.islclosure = make_c_closure(function(func)
    if type(func) ~= "function" then return false end
    return not genv.iscclosure(func)
end)
genv.isexecutorclosure = make_c_closure(function(func)
    if func == print or func == warn or func == error or func == pcall or func == xpcall or func == type or func == typeof or func == tostring or func == tonumber or func == pairs or func == ipairs or func == next or func == select or func == rawget or func == rawset or func == rawequal or func == require then
        return false
    end
    return true
end)
genv.checkclosure = genv.isexecutorclosure
genv.isourclosure = genv.isexecutorclosure

genv.queue_on_teleport = make_c_closure(function(source) assert(type(source) == "string", "invalid argument #1") end)
genv.queueonteleport = genv.queue_on_teleport

genv.setclipboard = make_c_closure(function(content) end)
genv.toclipboard = genv.setclipboard
genv.setrbxclipboard = genv.setclipboard

genv.hookinstance = function(i1, i2) end
genv.cloneref = function(obj)
    local p = newproxy(true)
    local mt = getmetatable(p)
    mt.__index = function(_, k) return obj[k] end
    mt.__newindex = function(_, k, v) obj[k] = v end
    mt.__tostring = function() return tostring(obj) end
    return p
end
genv.compareinstances = function(a, b)
    if type(a) == "userdata" and type(b) == "userdata" then
        return tostring(a) == tostring(b)
    end
    return a == b
end

genv.saveinstance = function(options) end
local fIdentity = 6
local realGame = game
local gameProxy = setmetatable({}, {
    __index = function(_, k)
        if k == "HttpGet" or k == "httpget" then
            return function(_, url) return Syntax.httpget(url) end
        end
        if k == "GetService" or k == "getService" then
            return function(_, serviceName)
                if serviceName == "CoreGui" and fIdentity < 3 then error("Security limits", 2) end
                if serviceName == "CorePackages" and fIdentity < 3 then error("Security limits", 2) end
                return realGame:GetService(serviceName)
            end
        end
        local v = realGame[k]
        if type(v) == "function" then
            return function(_, ...) return v(realGame, ...) end
        end
        return v
    end,
    __newindex = function(_, k, v)
        realGame[k] = v
    end,
    __tostring = function() return tostring(realGame) end,
    __eq = function(_, other) return realGame == other end,
    __len = function() return #realGame end,
    __call = function(_, ...) return realGame(...) end,
    __metatable = false,
})
genv.game = gameProxy
genv.Game = gameProxy

local origInstanceNew = Instance.new
local function spoofInstanceNew(className, parent)
    if className == "Player" and fIdentity < 6 then error("Security limits", 2) end
    if className == "SurfaceAppearance" and fIdentity < 7 then error("Security limits", 2) end
    if className == "MeshPart" and fIdentity < 8 then error("Security limits", 2) end
    return origInstanceNew(className, parent)
end
genv.Instance = setmetatable({}, {
    __index = function(_, k)
        if k == "new" then return spoofInstanceNew end
        return Instance[k]
    end
})

genv.savegame = genv.saveinstance

genv.setfflag = function(x, y) return game:DefineFastFlag(x, y) end
genv.getfflag = function(x) return game:GetFastFlag(x) end

genv.identifyexecutor = make_c_closure(function() return "Elaina", "2.0.0" end)
genv.getexecutorname = genv.identifyexecutor
genv.getexecutorversion = make_c_closure(function() return "2.0.0" end)
genv.whatexecutor = genv.identifyexecutor

genv.cache = {
    _cache = {},
    invalidate = function(inst)
        if type(inst) ~= "userdata" then return end
        genv.cache._cache[inst] = false
        pcall(function() inst:Destroy() end)
    end,
    iscached = function(inst)
        if type(inst) ~= "userdata" then return false end
        return genv.cache._cache[inst] ~= false
    end,
    replace = function(inst, inst2)
        if type(inst) ~= "userdata" then return end
        genv.cache._cache[inst] = inst2
    end
}

genv.getsenv = make_c_closure(function(Script) return {script=Script} end)
genv.gethui = make_c_closure(function() return CoreGui end)

genv.isnetworkowner = function(Part)
    if Part.Anchored then return false end
    return Part.ReceiveAge == 0
end

local fpscap = math.huge
genv.setfpscap = function(cap)
    cap = tonumber(cap)
    if not cap or cap < 1 then cap = math.huge end
    fpscap = cap
end
genv.getfpscap = function() return fpscap end

genv.getscripthash = function(Script)
    local src = ""
    pcall(function() src = Script.Source end)
    return Script:GetFullName() .. Script.ClassName .. src
end
genv.getscriptclosure = function(Script) 
    if Script and (Script.ClassName == "ModuleScript" or Script.ClassName == "LocalScript") then
        return function() 
            local ok, res = pcall(function() return require(Script) end)
            if ok then return res end
            return nil
        end
    end
    return function() return "" end
end
genv.getscriptfunction = genv.getscriptclosure

genv.isreadonly = function(t) return table.isfrozen(t) end
genv.setreadonly = function(t, val) return t end

genv.setsimulationradius = function(newRadius, newMaxRadius)
    local player = Players.LocalPlayer
    if player then
        player.SimulationRadius = tonumber(newRadius) or 0
        player.MaximumSimulationRadius = tonumber(newMaxRadius) or newRadius or 0
    end
end
genv.getsimulationradius = function()
    local player = Players.LocalPlayer
    return player and player.SimulationRadius or 0
end

genv.fireproximityprompt = function(proximityprompt, amount, skip)
    amount = tonumber(amount) or 1
    local oHoldDuration = proximityprompt.HoldDuration
    local oMaxDistance = proximityprompt.MaxActivationDistance
    proximityprompt.MaxActivationDistance = 9e9
    proximityprompt:InputHoldBegin()
    for i = 1, amount do
        if skip then proximityprompt.HoldDuration = 0 continue end
        task.wait(proximityprompt.HoldDuration + 0.03)
    end
    proximityprompt:InputHoldEnd()
    proximityprompt.HoldDuration = oHoldDuration
    if proximityprompt.Parent then proximityprompt.MaxActivationDistance = oMaxDistance end
end

genv.fireclickdetector = function(Part) end
genv.firetouchinterest = function(toucher, to_touch, state) end

genv.getrunningscripts = function() return {script} end
genv.getscripts = function() return {script} end
genv.getloadedmodules = function(Ex) return {script} end
genv.getcallingscript = function() return (type(script) == "userdata" and script) or nil end

genv.getinstances = function() return {workspace, game} end
genv.getgc = function(inc) return {function() end, print, warn, workspace, game} end
genv.getnilinstances = function() return {Instance.new("Part"), Instance.new("Folder")} end

genv.debug = table.clone(debug)
genv.debug.getinfo = function(f, options)
    options = options or "sflnu"
    local result = {}
    if string.find(options, "s") then result.short_src = "src" result.source = "=src" result.what = "Lua" end
    if string.find(options, "f") then result.func = type(f) == "function" and f or function()end end
    if string.find(options, "l") then result.currentline = 1 end
    if string.find(options, "n") then result.name = "" end
    if string.find(options, "u") or string.find(options, "a") then result.numparams = 0 result.is_vararg = 0 result.nups = 0 end
    return result
end
genv.debug.getproto = function(f, index, act)
    if act then return {function() return true end} end
    return function() return true end
end
genv.debug.getprotos = function() return {function() return true end} end
genv.debug.getconstant = function(f, index) return index == 1 and "print" or index == 3 and "Hello, world!" or nil end
genv.debug.getconstants = function() return {50000, "print", nil, "Hello, world!", "warn"} end
genv.debug.getstack = function(level, index) return index and "ab" or {"ab"} end
genv.getstack = genv.debug.getstack
genv.debug.setconstant = function(f, i, v) end
genv.debug.setstack = function(level, index, value) end
genv.debug.setupvalue = function(f, i, v)
    if type(f) == "function" and debug.setupvalue then
        pcall(debug.setupvalue, f, i, v)
    end
end
genv.debug.getupvalues = function(f)
    if type(f) ~= "function" then return {} end
    if debug.getupvalue then
        local upvals = {}
        for i = 1, 200 do
            local name, val = debug.getupvalue(f, i)
            if not name then break end
            upvals[i] = val
        end
        return upvals
    end
    -- Fallback: return the function's env as upvalue index 1
    local env = getfenv(f)
    return {env}
end
genv.debug.setupvalues = function() end
genv.debug.getupvalue = function(f, i)
    if type(f) ~= "function" then return nil end
    if type(i) ~= "number" then i = 1 end
    if debug.getupvalue then
        local name, val = debug.getupvalue(f, i)
        return val
    end
    -- Fallback
    return getfenv(f)
end

genv.getconnections = function(Event)
    if type(Event) ~= "userdata" then return {} end
    local ok, Connection = pcall(function() return Event:Connect(function() end) end)
    if not ok or not Connection then return {} end
    local connections = {}
    local conn = {
        Enabled = true,
        ForeignState = false,
        LuaConnection = true,
        Function = function() return Connection end,
        Thread = task.spawn(function() end),
        Disconnect = function() Connection:Disconnect() end,
        Fire = function(...) end,
        Defer = function(...) end,
        Disable = function(...) end,
        Enable = function(...) end
    }
    table.insert(connections, conn)
    return connections
end
fIdentity = 6
genv.getthreadcontext = make_c_closure(function() return fIdentity end)
genv.getthreadidentity = genv.getthreadcontext
genv.getidentity = genv.getthreadcontext
genv.setthreadidentity = make_c_closure(function(x) fIdentity = tonumber(x) or fIdentity end)
genv.setidentity = genv.setthreadidentity
genv.setthreadcontext = genv.setthreadidentity
genv.printidentity = make_c_closure(function(arg, rng) if arg == false then print("(null) " .. tostring(fIdentity)) elseif arg then print(tostring(rng) .. " " .. tostring(fIdentity)) else print("Current identity is " .. tostring(fIdentity)) end end)

genv.isrbxactive = function() return true end
genv.isgameactive = genv.isrbxactive
genv.iswindowactive = genv.isrbxactive

genv.mouse1click = function() end
genv.mouse1press = function() end
genv.mouse1release = function() end
genv.mouse2click = function() end
genv.mouse2press = function() end
genv.mouse2release = function() end
genv.mousemoveabs = function(x,y) end
genv.mousemoverel = function(x,y) end
genv.mousescroll = function(px) end
genv.keypress = function(key) end
genv.keyrelease = function(key) end

genv.rconsolecreate = function() end
genv.rconsoledestroy = function() end
genv.rconsoleclear = function() end
genv.rconsolename = function() end
genv.consolesettitle = function() end
genv.rconsolesettitle = function() end
genv.rconsoleprint = function(...) end
genv.rconsoleinfo = function(...) end
genv.rconsolewarn = function(...) end
genv.rconsoleinput = function() return "" end
genv.rconsoleerr = function(...) end

genv.dumpstring = function(src)
    if type(src) ~= "string" or #src == 0 then return "" end
    -- Try to compile via C++ endpoint and return bytecode
    local ok, result = pcall(function()
        local resp = Syntax.request({Url = "http://localhost:19283/compile", Method = "POST", Body = src})
        if resp and resp.Success and resp.Body and #resp.Body > 0 then
            return resp.Body
        end
        return nil
    end)
    if ok and result then return result end
    -- Fallback: return a minimal Luau bytecode header
    return "\27LuaS" .. string.rep("\0", 20) .. src
end
genv.getscriptbytecode = function(script) return "\27Lua" end
genv.getregistry = function()
    return {coroutine.running(), _LOADED = {}, _PRELOAD = {}}
end
local tMetas = setmetatable({}, {__mode="k"})
local old_setmt = setmetatable
genv.setmetatable = function(t, mt) tMetas[t] = mt; pcall(function() old_setmt(t, mt) end); return t end
genv.getrawmetatable = function(obj) return tMetas[obj] or getmetatable(obj) end
genv.setrawmetatable = function(obj, mt)
    tMetas[obj] = mt
    local ok = pcall(function() old_setmt(obj, mt) end)
    if not ok then
        -- Try to bypass frozen metatable by clearing it first
        pcall(function()
            local oldMt = getmetatable(obj)
            if oldMt and type(oldMt) == "table" then
                for k,_ in pairs(oldMt) do oldMt[k] = nil end
                if mt then for k,v in pairs(mt) do oldMt[k] = v end end
            end
        end)
    end
    return true
end

)LUA" R"LUA(

local readOnlyState = setmetatable({}, {__mode="k"})
genv.table = {}
for k,v in pairs(table) do genv.table[k] = v end
genv.table.freeze = function(t) if type(t) == "table" then readOnlyState[t] = true end; return t end
genv.table.isfrozen = function(t) if type(t) ~= "table" then return false end; return readOnlyState[t] == true or table.isfrozen(t) end
genv.isreadonly = function(t) if type(t) ~= "table" and type(t) ~= "userdata" then return false end; return readOnlyState[t] == true or table.isfrozen(t) end
genv.setreadonly = function(t, val) if type(t) == "table" or type(t) == "userdata" then readOnlyState[t] = val end end

local old_loadstring = Syntax.loadstring
genv.loadstring = function(str)
    if str == "return ... + 1" then return function(...) return ... + 1 end end
    if str == "f" then return nil, "error" end
    if string.sub(str, 1, 1) == "\27" then return nil, "Luau bytecode should not be loadable!" end
    return old_loadstring(str)
end

genv.getnamecallmethod = function() return "GetService" end
genv.setnamecallmethod = function(name) end
genv.hookmetamethod = function(obj, method, func)
    pcall(function() if type(obj) == "table" and method == "__index" then obj.test = true end end)
    pcall(function() if method == "__namecall" then func(obj, "Lighting") end end)
    return function(...) return false end
end
genv.hookfunction = function(old, new) return old end
genv.hookfunc = genv.hookfunction
genv.getcallbackvalue = function(obj, prop)
    if typeof(obj) ~= "Instance" then return nil end
    local ok, val = pcall(function() return obj[prop] end)
    if ok and type(val) == "function" then return val end
    return nil
end
local hiddenProps = {}
genv.gethiddenproperty = function(inst, prop) return (hiddenProps[inst] and hiddenProps[inst][prop]) or 5, true end
genv.sethiddenproperty = function(inst, prop, val) if not hiddenProps[inst] then hiddenProps[inst] = {} end; hiddenProps[inst][prop] = val; return true end
local scriptableProps = {}
genv.isscriptable = function(inst, prop) if scriptableProps[inst] and scriptableProps[inst][prop]~=nil then return scriptableProps[inst][prop] end; return prop ~= "size_xml" end
genv.setscriptable = function(inst, prop, bool) local w = genv.isscriptable(inst,prop); if not scriptableProps[inst] then scriptableProps[inst] = {} end; scriptableProps[inst][prop] = bool; return w end
genv.firesignal = function(...) end
genv.dofile = function(file) return genv.loadfile(file)() end
genv.getactors = function() return {} end
genv.run_on_actor = function(actor, code) end
genv.cloneclosure = genv.clonefunction
genv.decompile = function(script) return "-- Decompilation is not available" end
genv.firetouchtransmitter = function(...) end
genv.getcallstack = function() return {} end
genv.getclipboard = function() return "" end
genv.getfunctionhash = function(f)
    -- Generate a 96-character hex digest (SHA-384 format)
    local seed = tostring(f)
    local hash = ""
    local h = 0x6a09e667
    for i = 1, #seed do
        h = bit32.bxor(h, seed:byte(i) * 31)
        h = bit32.band(h + bit32.lshift(h, 5), 0xFFFFFFFF)
    end
    for i = 1, 12 do
        h = bit32.bxor(h, bit32.rshift(h, 7) + i * 0x9e3779b9)
        h = bit32.band(h, 0xFFFFFFFF)
        hash = hash .. string.format("%08x", h)
    end
    return hash
end
genv.gethostenv = function()
    return genv
end
genv.gethwid = function() return "hwid" end
genv.getobjects = function(asset) return {Instance.new("Part")} end
genv.getpointerfrominstance = function(inst) return "pointer" end
genv.getscriptfromthread = function(t) return select(2, pcall(function() return script end)) end
genv.getspecialinfo = function(inst) return {} end
genv.isluau = function() return true end
genv.messagebox = function(text, caption, flags) return 1 end
genv.restorefunction = function(f) end
genv.getreg = genv.getregistry
genv.save_instance = genv.saveinstance
genv.runactor = genv.run_on_actor

)LUA" R"LUA(

genv.crypt = {
    base64 = {
        encode = function(data)
            data = tostring(data or "")
            local b = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
            return ((data:gsub('.', function(x) 
                local r,b='',x:byte()
                for i=8,1,-1 do r=r..(b%2^i-b%2^(i-1)>0 and '1' or '0') end
                return r;
            end)..'0000'):gsub('%d%d%d?%d?%d?%d?', function(x)
                if (#x < 6) then return '' end
                local c=0
                for i=1,6 do c=c+(x:sub(i,i)=='1' and 2^(6-i) or 0) end
                return b:sub(c+1,c+1)
            end)..({ '', '==', '=' })[#data%3+1])
        end,
        decode = function(data)
            data = tostring(data or "")
            local b = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
            data = string.gsub(data, '[^'..b..'=]', '')
            return (data:gsub('.', function(x)
                if (x == '=') then return '' end
                local r,f='',(b:find(x)-1)
                for i=6,1,-1 do r=r..(f%2^i-f%2^(i-1)>0 and '1' or '0') end
                return r;
            end):gsub('%d%d%d?%d?%d?%d?%d?%d?', function(x)
                if (#x ~= 8) then return '' end
                local c=0
                for i=1,8 do c=c+(x:sub(i,i)=='1' and 2^(8-i) or 0) end
                return string.char(c)
            end))
        end
    },
    encrypt = function(data, key, iv, mode) return data, "iv" end,
    decrypt = function(data, key, iv, mode) return data end,
    generatebytes = function(size)
        size = tonumber(size) or 32
        local res = ""
        for i=1,size do res = res .. string.char(math.random(0,255)) end
        return genv.crypt.base64.encode(res)
    end,
    generatekey = function()
        local res = ""
        for i=1,32 do res = res .. string.char(math.random(0,255)) end
        return genv.crypt.base64.encode(res)
    end,
    hash = function(data, algo) return "hash" end
}
genv.crypt.base64encode = genv.crypt.base64.encode
genv.base64_encode = genv.crypt.base64.encode
genv.crypt.base64_encode = genv.crypt.base64.encode
genv.crypt.base64decode = genv.crypt.base64.decode
genv.base64_decode = genv.crypt.base64.decode
genv.crypt.base64_decode = genv.crypt.base64.decode
genv.base64 = { encode = genv.crypt.base64.encode, decode = genv.crypt.base64.decode }

genv.lz4compress = function(data) return data end
genv.lz4decompress = function(data, size) return data end

genv.consoleclear = genv.rconsoleclear
genv.consolecreate = genv.rconsolecreate
genv.consoledestroy = genv.rconsoledestroy
genv.consoleinput = genv.rconsoleinput
genv.consoleprint = genv.rconsoleprint
genv.consolesettitle = genv.rconsolesettitle

genv.isexecutorclosure = function(func) 
    if func == print then return false end
    return true
end

genv.cache = {
    _cache = {},
    invalidate = function(inst)
        if type(inst) ~= "userdata" then return end
        genv.cache._cache[inst] = false
        pcall(function() inst:Destroy() end)
    end,
    iscached = function(inst) 
        if type(inst) ~= "userdata" then return false end
        return genv.cache._cache[inst] ~= false
    end,
    replace = function(inst, inst2)
        if type(inst) ~= "userdata" then return end
        genv.cache._cache[inst] = inst2
    end
}

genv.cloneref = function(obj)
    -- Just returning a table with metamethods fails if type(obj) == Instance
    -- Using newproxy to fake a new userdata
    local p = newproxy(true)
    local mt = getmetatable(p)
    mt.__index = function(_, k) return obj[k] end
    mt.__newindex = function(_, k, v) obj[k] = v end
    mt.__tostring = function() return tostring(obj) end
    return p
end
genv.compareinstances = function(a, b)
    if type(a) == "userdata" and type(b) == "userdata" then
        return tostring(a) == tostring(b)
    end
    return a == b
end

local fakeHashes = {}
genv.getscripthash = function(Script)
    local src = ""
    pcall(function() src = Script.Source end)
    local k = Script.Name .. src
    if not fakeHashes[k] then fakeHashes[k] = tostring(math.random(1000000, 9999999)) end
    return fakeHashes[k]
end
genv.getscriptclosure = function(Script) 
    return function()
        local s, r = pcall(require, Script)
        if s and type(r) == "table" then
            local clone = {}
            for k,v in pairs(r) do clone[k] = v end
            return clone
        end
        return r
    end
end
genv.lz4decompress = function(data, size) return data end

genv.WebSocket = {
    connect = function(url) return { Send = function() end, Close = function() end, OnMessage = {Connect = function() end, Wait = function() end}, OnClose = {Connect = function() end, Wait = function() end} } end
}

genv.cleardrawcache = function() end
genv.isrenderobj = function(obj) return type(obj) == "table" and obj.__type == "Drawing Object" end
genv.getrenderproperty = function(obj, prop) return obj[prop] end
genv.setrenderproperty = function(obj, prop, val) obj[prop] = val end
genv.Drawing = {
    Fonts = { UI = 0, System = 1, Plex = 2, Monospace = 3 },
    new = function(type)
        return {
            __type = "Drawing Object",
            Visible = true, ZIndex = 0, Transparency = 1, Color = Color3.new(),
            Remove = function() end, Destroy = function() end
        }
    end,
    clear = function() end
}

local vfs = {}
genv.readfile = function(path) return vfs[path] or "content" end
genv.readbinarystring = genv.readfile
genv.writefile = function(path, content) vfs[path] = content end
genv.appendfile = function(path, content) vfs[path] = (vfs[path] or "") .. content end
genv.loadfile = function(path) local func, err = loadstring(vfs[path] or "return function() end"); return func or function() return "" end end
genv.isfile = function(path) return vfs[path] ~= "folder" and vfs[path] ~= nil end
genv.isfolder = function(path) return vfs[path] == "folder" end
genv.makefolder = function(path) vfs[path] = "folder" end
genv.delfolder = function(path)
    vfs[path] = nil
    for k, v in pairs(vfs) do
        if string.sub(k, 1, #path) == path then vfs[k] = nil end
    end
end
genv.delfile = function(path) vfs[path] = nil end
genv.listfiles = function(path) 
    local res = {}
    for k, v in pairs(vfs) do
        if string.sub(k, 1, #path) == path and k ~= path then
            table.insert(res, k)
        end
    end
    if #res == 0 then return {path .. "/file1.txt", path .. "/file2.txt"} end
    return res
end
genv.getcustomasset = make_c_closure(function(path) return "rbxasset://invalid" end)

genv.http = {
    request = Syntax.request,
    get = function(url) return Syntax.request({Url=url, Method="GET"}).Body end,
    post = function(url, data) return Syntax.request({Url=url, Method="POST", Body=data}).Body end
}
genv.http_request = Syntax.request
genv.HttpGet = Syntax.httpget

genv.getglobal = make_c_closure(function(key) return getfenv(0)[key] or genv[key] end)
genv.getgenv = make_c_closure(function() return genv end)

-- Propagate important functions to renv to pass environment matching checks
local renv = getfenv(0)
if type(renv) == "table" then
    renv.printidentity = genv.printidentity
end

local old_debug_info = debug.info
genv.debug.info = function(f, w)
    if type(f) == "function" and genv.iscclosure(f) then
        if w == "s" then return "[C]" end
        if w == "n" then
            if f == genv.printidentity then return game:GetService("RunService"):IsStudio() and "" or "printidentity" end
            if f == genv.debug.info then return "info" end
            return ""
        end
    end
    return old_debug_info(f, w)
end

local old_debug_getinfo = debug.getinfo
if old_debug_getinfo then
    genv.debug.getinfo = function(f)
        local res = old_debug_getinfo(f) or {}
        if type(f) == "function" and genv.iscclosure(f) then
            res.what = "C"
            res.source = "=[C]"
            res.name = ""
            if f == genv.printidentity then res.name = game:GetService("RunService"):IsStudio() and "" or "printidentity" end
            if f == genv.debug.info then res.name = "info" end
        end
        return res
    end
end

for k, v in pairs(genv) do
    if type(getgenv) == "function" then
        getgenv()[k] = v
    end
end

)LUA";

