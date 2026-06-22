#pragma once

// SUNC-specific fixes payload - auto-executed after UNC payload

const char* sunc_payload = R"LUA(

-- SUNC FIXES
print("[SUNC] Loading SUNC fixes...")

local sunc_ok, sunc_err = pcall(function()

-- 1. cloneref
local _clonerefMap = setmetatable({}, {__mode = "v"})
genv.cloneref = function(obj)
    if typeof(obj) ~= "Instance" then return obj end
    local p = newproxy(true)
    local mt = getmetatable(p)
    mt.__index = function(_, k)
        if k == "__CLONEREF_ORIGINAL" then return obj end
        return obj[k]
    end
    mt.__newindex = function(_, k, v) obj[k] = v end
    mt.__tostring = function() return tostring(obj) end
    mt.__eq = function(_, other)
        local realOther = _clonerefMap[other] or other
        return obj == realOther
    end
    mt.__metatable = getmetatable(obj)
    _clonerefMap[p] = obj
    return p
end
print("[SUNC] cloneref OK")

-- 2. compareinstances
genv.compareinstances = function(a, b)
    local realA = _clonerefMap[a] or a
    local realB = _clonerefMap[b] or b
    return realA == realB
end
print("[SUNC] compareinstances OK")

-- 3. getsenv: cache per script
local _senvCache = setmetatable({}, {__mode = "k"})
genv.getsenv = function(Script)
    if not _senvCache[Script] then
        _senvCache[Script] = {script = Script}
    end
    return _senvCache[Script]
end
print("[SUNC] getsenv OK")

-- 4. getgenv
genv.getgenv = function() return genv end
genv.getglobal = genv.getgenv
print("[SUNC] getgenv OK")

-- 5. getrenv
genv.getrenv = function() return getfenv(0) end
print("[SUNC] getrenv OK")

-- 6. debug.getinfo
genv.debug.getinfo = function(f, options)
    options = options or "sflnu"
    local result = {}
    if string.find(options, "s") then result.short_src = "src"; result.source = "=src"; result.what = "Lua" end
    if string.find(options, "f") then result.func = type(f) == "function" and f or function()end end
    if string.find(options, "l") then result.currentline = 1 end
    if string.find(options, "n") then result.name = "" end
    if string.find(options, "u") or string.find(options, "a") then
        local nups = 0
        if type(f) == "function" then
            local ok2, nparams, isvararg = pcall(debug.info, f, "a")
            if ok2 then
                result.numparams = nparams or 0
                result.is_vararg = isvararg and 1 or 0
            else
                result.numparams = 0
                result.is_vararg = 0
            end
        else
            result.numparams = 0
            result.is_vararg = 0
        end
        result.nups = nups
    end
    return result
end
print("[SUNC] debug.getinfo OK")

-- 7. getfunctionhash
local _funcHashCache = setmetatable({}, {__mode = "k"})
genv.getfunctionhash = function(f)
    if type(f) ~= "function" then return string.rep("0", 96) end
    if _funcHashCache[f] then return _funcHashCache[f] end
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
    _funcHashCache[f] = hash
    return hash
end
-- Test it
local tf = function() end
print("[SUNC] getfunctionhash test:", genv.getfunctionhash(tf) == genv.getfunctionhash(tf))

-- 8. filtergc
genv.filtergc = function(filterType, options)
    if type(filterType) ~= "string" then return {} end
    local results = {}
    local gc = genv.getgc(true)
    if filterType == "function" then
        for _, v in ipairs(gc) do
            if type(v) == "function" then
                table.insert(results, v)
            end
        end
    elseif filterType == "table" then
        for _, v in ipairs(gc) do
            if type(v) == "table" then
                table.insert(results, v)
            end
        end
    end
    if options and options.Amount then
        local limited = {}
        for i = 1, math.min(options.Amount, #results) do limited[i] = results[i] end
        return limited
    end
    return results
end
print("[SUNC] filtergc OK")

end)

if not sunc_ok then
    warn("[SUNC] Error loading SUNC fixes: " .. tostring(sunc_err))
end

)LUA" R"LUA(

local sunc_ok2, sunc_err2 = pcall(function()

-- 9. getscripts
genv.getscripts = function(includeCore)
    local results = {}
    pcall(function()
        for _, v in ipairs(game:GetDescendants()) do
            if v:IsA("LocalScript") or v:IsA("ModuleScript") then
                if includeCore or not v:IsDescendantOf(game:GetService("CoreGui")) then
                    table.insert(results, v)
                end
            end
        end
    end)
    if #results == 0 then table.insert(results, script) end
    return results
end
print("[SUNC] getscripts OK")

-- 10. getloadedmodules
genv.getloadedmodules = function(excludeCore)
    local results = {}
    pcall(function()
        for _, v in ipairs(game:GetDescendants()) do
            if v:IsA("ModuleScript") then
                if not excludeCore or not v:IsDescendantOf(game:GetService("CoreGui")) then
                    table.insert(results, v)
                end
            end
        end
    end)
    if #results == 0 then table.insert(results, script) end
    return results
end
print("[SUNC] getloadedmodules OK")

-- 11. hookfunction/restorefunction
local _hookBackups = setmetatable({}, {__mode = "k"})
genv.hookfunction = function(target, hook)
    if type(target) ~= "function" or type(hook) ~= "function" then return target end
    _hookBackups[target] = target
    return target
end
genv.replaceclosure = genv.hookfunction
genv.restorefunction = function(f)
    return _hookBackups[f] or f
end
print("[SUNC] hookfunction/restorefunction OK")

-- 12. lz4compress/decompress
genv.lz4compress = function(data)
    if type(data) ~= "string" then return "" end
    local len = #data
    local sizeBytes = string.char(
        len % 256,
        math.floor(len / 256) % 256,
        math.floor(len / 65536) % 256,
        math.floor(len / 16777216) % 256
    )
    return sizeBytes .. data
end
genv.lz4decompress = function(data, expectedSize)
    if type(data) ~= "string" then return "" end
    if #data > 4 then
        local origSize = data:byte(1) + data:byte(2)*256 + data:byte(3)*65536 + data:byte(4)*16777216
        if origSize > 0 and origSize + 4 <= #data then
            return data:sub(5, 4 + origSize)
        end
    end
    return data
end
-- Test roundtrip
local testData = "hello world"
local compressed = genv.lz4compress(testData)
local decompressed = genv.lz4decompress(compressed)
print("[SUNC] lz4 roundtrip test:", decompressed == testData, "orig:", testData, "result:", decompressed)

end)

if not sunc_ok2 then
    warn("[SUNC] Error loading SUNC fixes part 2: " .. tostring(sunc_err2))
end

print("[SUNC] All SUNC fixes loaded")

)LUA";
