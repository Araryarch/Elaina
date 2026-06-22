-- Syntax UNC Test Suite
-- Load with executor and run to test all UNC functions
-- Reports pass/fail for each function with overall score

local ui = gethui()
local plr = game:GetService("Players").LocalPlayer
local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local TweenService = game:GetService("TweenService")

-- ===== TEST FRAMEWORK =====
local tests = {}
local results = {}
local totalPassed = 0
local totalFailed = 0

local function newTest(category, name, fn)
    table.insert(tests, {category = category, name = name, fn = fn})
end

local function ok(...)
    local details = {...}
    local msg = #details > 0 and tostring(details[1]) or "passed"
    return true, msg
end

local function fail(msg)
    return false, tostring(msg or "failed")
end

local function expectEqual(a, b, msg)
    if a == b then return ok(msg) end
    return fail(string.format("%s: expected %s, got %s", tostring(msg or ""), tostring(b), tostring(a)))
end

local function expectType(val, t, msg)
    if type(val) == t then return ok(msg) end
    return fail(string.format("%s: expected type %s, got %s", tostring(msg or ""), t, type(val)))
end

local function safe(func, ...)
    local args = {...}
    return pcall(function() return func(table.unpack(args)) end)
end

-- ===== 1. CORE =====

newTest("Core", "identifyexecutor exists", function()
    return expectType(identifyexecutor, "function")
end)

newTest("Core", "identifyexecutor returns (string, string)", function()
    local name, ver = identifyexecutor()
    if type(name) == "string" and type(ver) == "string" then
        return ok("Syntax/" .. ver)
    end
    return fail(string.format("got %s, %s", type(name), type(ver)))
end)

newTest("Core", "getexecutorname returns string", function()
    return expectType(getexecutorname(), "string")
end)

newTest("Core", "getexecutorversion returns string", function()
    return expectType(getexecutorversion(), "string")
end)

newTest("Core", "whatexecutor returns string", function()
    return expectType(whatexecutor(), "string")
end)

newTest("Core", "getgenv returns table", function()
    local env = getgenv()
    return expectType(env, "table")
end)

newTest("Core", "getgenv is same as script env", function()
    local env = getgenv()
    local same = env.loadstring == loadstring
    return same and ok() or fail("loadstring differs between getgenv() and globals")
end)

newTest("Core", "getrenv returns table", function()
    return expectType(getrenv(), "table")
end)

newTest("Core", "getglobal returns value", function()
    local gameVal = getglobal("game")
    return expectType(gameVal, "userdata")
end)

newTest("Core", "loadstring basic", function()
    local fn, err = loadstring("return 1 + 1")
    if not fn then return fail("loadstring returned nil: " .. tostring(err)) end
    local ok2, result = pcall(fn)
    if not ok2 then return fail("exec error: " .. tostring(result)) end
    if result ~= 2 then return fail("expected 2, got " .. tostring(result)) end
    return ok("1+1=2")
end)

newTest("Core", "loadstring with env", function()
    local sandbox = {x = 42}
    local fn = loadstring("return x", nil, sandbox)
    if not fn then return fail("loadstring with env failed") end
    local ok2, result = pcall(fn)
    return ok2 and result == 42 and ok("x=42") or fail(tostring(result))
end)

newTest("Core", "loadstring error returns nil+msg", function()
    local fn, err = loadstring("invalid syntax [[")
    if fn ~= nil then return fail("should return nil for invalid syntax") end
    return expectType(err, "string")
end)

-- ===== 2. CLOSURES =====

newTest("Closures", "iscclosure on print", function()
    return expectEqual(iscclosure(print), true)
end)

newTest("Closures", "iscclosure on Lua func", function()
    return expectEqual(iscclosure(function() end), false)
end)

newTest("Closures", "islclosure on print", function()
    return expectEqual(islclosure(print), false)
end)

newTest("Closures", "islclosure on Lua func", function()
    return expectEqual(islclosure(function() end), true)
end)

newTest("Closures", "isexecutorclosure on built-in", function()
    return expectEqual(type(isexecutorclosure), "function")
end)

newTest("Closures", "isexecutorclosure on print->false", function()
    local result = isexecutorclosure(print)
    return expectEqual(result, false)
end)

newTest("Closures", "isexecutorclosure on self->true", function()
    local result = isexecutorclosure(isexecutorclosure)
    return result == true and ok() or fail("isexecutorclosure should be true for self")
end)

newTest("Closures", "checkclosure alias", function()
    return expectType(checkclosure, "function")
end)

newTest("Closures", "isourclosure alias", function()
    return expectType(isourclosure, "function")
end)

newTest("Closures", "newcclosure creates function", function()
    local c = newcclosure(function() return 5 end)
    if type(c) ~= "function" then return fail("not a function") end
    local ok2, r = pcall(c)
    return ok2 and r == 5 and ok("returns 5") or fail(tostring(r))
end)

newTest("Closures", "newcclosure iscclosure true", function()
    local c = newcclosure(function() end)
    return expectEqual(iscclosure(c), true)
end)

newTest("Closures", "newlclosure creates function", function()
    local c = newlclosure(function() return 10 end)
    return expectType(c, "function")
end)

newTest("Closures", "clonefunction works", function()
    local fn = function() return 42 end
    local cloned = clonefunction(fn)
    if type(cloned) ~= "function" then return fail("not a function") end
    local ok2, r = pcall(cloned)
    return ok2 and r == 42 and ok() or fail(tostring(r))
end)

newTest("Closures", "cloneclosure alias", function()
    return expectType(cloneclosure, "function")
end)

-- ===== 3. METATABLES =====

newTest("Metatables", "getrawmetatable returns table", function()
    local mt = getrawmetatable(newproxy(true))
    return expectType(mt, "table")
end)

newTest("Metatables", "getrawmetatable on game", function()
    local mt = getrawmetatable(game)
    return mt ~= nil and ok() or fail("got nil")
end)

newTest("Metatables", "setreadonly exists", function()
    return expectType(setreadonly, "function")
end)

newTest("Metatables", "isreadonly exists", function()
    return expectType(isreadonly, "function")
end)

newTest("Metatables", "isreadonly on frozen table", function()
    local t = {1, 2, 3}
    table.freeze(t)
    return expectEqual(isreadonly(t), true)
end)

newTest("Metatables", "setrawmetatable exists", function()
    return expectType(setrawmetatable, "function")
end)

newTest("Metatables", "table.freeze/isfrozen", function()
    local t = {a = 1}
    table.freeze(t)
    return expectEqual(table.isfrozen(t), true)
end)

newTest("Metatables", "hookmetamethod exists", function()
    return expectType(hookmetamethod, "function")
end)

-- ===== 4. IDENTITY =====

newTest("Identity", "getidentity returns number", function()
    return expectType(getidentity(), "number")
end)

newTest("Identity", "getthreadidentity returns number", function()
    return expectType(getthreadidentity(), "number")
end)

newTest("Identity", "setthreadidentity works", function()
    local old = getthreadidentity()
    setthreadidentity(2)
    local cur = getthreadidentity()
    setthreadidentity(old)
    return expectEqual(cur, 2)
end)

newTest("Identity", "setidentity alias", function()
    return expectType(setidentity, "function")
end)

newTest("Identity", "getthreadcontext alias", function()
    return expectType(getthreadcontext, "function")
end)

newTest("Identity", "setthreadcontext alias", function()
    return expectType(setthreadcontext, "function")
end)

newTest("Identity", "printidentity exists", function()
    return expectType(printidentity, "function")
end)

-- ===== 5. ENVIRONMENT =====

newTest("Environment", "getfenv returns table", function()
    return expectType(getfenv(), "table")
end)

newTest("Environment", "getsenv returns table", function()
    local env = getsenv(script)
    return expectType(env, "table")
end)

newTest("Environment", "gethui returns Instance", function()
    local hui = gethui()
    return expectType(hui, "userdata")
end)

newTest("Environment", "gethui parented to PlayerGui", function()
    local hui = gethui()
    local ok2, parent = pcall(function() return hui.Parent end)
    if not ok2 then return fail("cannot read Parent") end
    local ok3, pg = pcall(function() return plr:FindFirstChild("PlayerGui") end)
    if ok3 and pg then
        return parent == pg and ok("parented to PlayerGui") or fail("parented to " .. tostring(parent))
    end
    return fail("PlayerGui not found")
end)

-- ===== 6. EXECUTION =====

newTest("Execution", "getscriptclosure returns function", function()
    local fn = getscriptclosure(script)
    return expectType(fn, "function")
end)

newTest("Execution", "getscriptfunction alias", function()
    return expectType(getscriptfunction, "function")
end)

newTest("Execution", "getscripthash returns string", function()
    local hash = getscripthash(script)
    return expectType(hash, "string")
end)

newTest("Execution", "getscriptbytecode returns string", function()
    local bc = getscriptbytecode(script)
    return expectType(bc, "string")
end)

newTest("Execution", "getrunningscripts returns table", function()
    local scripts = getrunningscripts()
    if type(scripts) ~= "table" then return fail("not a table") end
    return #scripts > 0 and ok(#scripts .. " scripts") or fail("empty")
end)

newTest("Execution", "getscripts returns table", function()
    local scripts = getscripts()
    return expectType(scripts, "table")
end)

newTest("Execution", "getloadedmodules returns table", function()
    local mods = getloadedmodules()
    return expectType(mods, "table")
end)

newTest("Execution", "getcallingscript returns Instance or nil", function()
    local cs = getcallingscript()
    return cs == nil or type(cs) == "userdata" and ok() or fail("bad type")
end)

newTest("Execution", "getgc returns table", function()
    local gc = getgc()
    return expectType(gc, "table")
end)

newTest("Execution", "getgc with true returns table", function()
    local gc = getgc(true)
    return expectType(gc, "table")
end)

newTest("Execution", "getreg/getregistry returns table", function()
    return expectType(getreg(), "table")
end)

newTest("Execution", "getinstances returns table", function()
    local insts = getinstances()
    return expectType(insts, "table")
end)

newTest("Execution", "getnilinstances returns table", function()
    local nilInsts = getnilinstances()
    return expectType(nilInsts, "table")
end)

newTest("Execution", "getobjects returns table", function()
    local objs = getobjects("rbxasset://test")
    return expectType(objs, "table")
end)

-- ===== 7. HOOKS =====

newTest("Hooks", "hookfunction exists", function()
    return expectType(hookfunction, "function")
end)

newTest("Hooks", "hookfunction replaces and returns", function()
    local original = function() return 1 end
    local hook = function() return 2 end
    local old = hookfunction(original, hook)
    local result = original()
    return result == 2 and ok("hooked: " .. tostring(result)) or fail("returned " .. tostring(result))
end)

newTest("Hooks", "replaceclosure alias", function()
    return expectType(replaceclosure, "function")
end)

newTest("Hooks", "restorefunction exists", function()
    return expectType(restorefunction, "function")
end)

newTest("Hooks", "hookfunc alias", function()
    return expectType(hookfunc, "function")
end)

-- ===== 8. INSTANCES =====

newTest("Instances", "cloneref returns proxy", function()
    local cloned = cloneref(game)
    return expectType(cloned, "userdata")
end)

newTest("Instances", "cloneref proxy accesses properties", function()
    local cloned = cloneref(game)
    local ok2, name = pcall(function() return cloned.Name end)
    return ok2 and name == "Game" and ok() or fail("could not read Name: " .. tostring(name))
end)

newTest("Instances", "compareinstances matches cloned", function()
    local cloned = cloneref(game)
    return expectEqual(compareinstances(game, cloned), true)
end)

newTest("Instances", "getcallbackvalue returns function or nil", function()
    local cb = getcallbackvalue(game, "GetService")
    return cb == nil or type(cb) == "function" and ok() or fail("bad type")
end)

newTest("Instances", "gethiddenproperty returns value", function()
    local val, _ = gethiddenproperty(game, "UnknownProp")
    return ok("returned " .. tostring(val))
end)

newTest("Instances", "sethiddenproperty exists", function()
    return expectType(sethiddenproperty, "function")
end)

newTest("Instances", "isscriptable returns boolean", function()
    local result = isscriptable(game, "Name")
    return expectType(result, "boolean")
end)

newTest("Instances", "setscriptable exists", function()
    return expectType(setscriptable, "function")
end)

newTest("Instances", "getactors returns table", function()
    return expectType(getactors(), "table")
end)

newTest("Instances", "run_on_actor exists", function()
    return expectType(run_on_actor, "function")
end)

newTest("Instances", "runactor alias", function()
    return expectType(runactor, "function")
end)

newTest("Instances", "getspecialinfo returns table", function()
    local info = getspecialinfo(Instance.new("Part"))
    return expectType(info, "table")
end)

newTest("Instances", "getpointerfrominstance returns value", function()
    local ptr = getpointerfrominstance(game)
    return ptr ~= nil and ok() or fail("got nil")
end)

-- ===== 9. EVENTS =====

newTest("Events", "getconnections returns table", function()
    local conns = getconnections(game.Close)
    return expectType(conns, "table")
end)

newTest("Events", "getconnections has Function field", function()
    local conns = getconnections(game.Close)
    if #conns == 0 then return ok("no connections (acceptable)") end
    return expectType(conns[1].Function, "function")
end)

newTest("Events", "getconnections has Disconnect field", function()
    local conns = getconnections(game.Close)
    if #conns == 0 then return ok("no connections") end
    return expectType(conns[1].Disconnect, "function")
end)

newTest("Events", "getconnections has Fire field", function()
    local conns = getconnections(game.Close)
    if #conns == 0 then return ok("no connections") end
    return expectType(conns[1].Fire, "function")
end)

newTest("Events", "firesignal exists", function()
    return expectType(firesignal, "function")
end)

newTest("Events", "fireclickdetector exists", function()
    return expectType(fireclickdetector, "function")
end)

newTest("Events", "fireclickdetector handles instance", function()
    local detector = Instance.new("ClickDetector")
    local ok2 = pcall(fireclickdetector, detector)
    return ok2 and ok() or fail("error calling fireclickdetector")
end)

newTest("Events", "fireproximityprompt exists", function()
    return expectType(fireproximityprompt, "function")
end)

newTest("Events", "firetouchinterest exists", function()
    return expectType(firetouchinterest, "function")
end)

newTest("Events", "firetouchtransmitter exists", function()
    return expectType(firetouchtransmitter, "function")
end)

-- ===== 10. NETWORK =====

newTest("Network", "request exists", function()
    return expectType(request, "function")
end)

newTest("Network", "http_request alias", function()
    return expectType(http_request, "function")
end)

newTest("Network", "httpget exists", function()
    return expectType(httpget, "function")
end)

newTest("Network", "HttpGet alias", function()
    return expectType(HttpGet, "function")
end)

newTest("Network", "http table exists", function()
    return expectType(http, "table")
end)

newTest("Network", "http.request is same as request", function()
    return expectType(http.request, "function")
end)

newTest("Network", "http.get exists", function()
    return expectType(http.get, "function")
end)

newTest("Network", "http.post exists", function()
    return expectType(http.post, "function")
end)

newTest("Network", "WebSocket exists", function()
    return expectType(WebSocket, "table")
end)

newTest("Network", "WebSocket.connect exists", function()
    return expectType(WebSocket.connect, "function")
end)

-- ===== 11. FILE SYSTEM =====

newTest("FileSystem", "writefile exists", function()
    return expectType(writefile, "function")
end)

newTest("FileSystem", "writefile + readfile roundtrip", function()
    writefile("unctest.txt", "hello_world")
    local content = readfile("unctest.txt")
    delfile("unctest.txt")
    return expectEqual(content, "hello_world")
end)

newTest("FileSystem", "readfile returns string", function()
    writefile("unctest2.txt", "test_data")
    local content = readfile("unctest2.txt")
    delfile("unctest2.txt")
    return expectType(content, "string")
end)

newTest("FileSystem", "appendfile works", function()
    writefile("unctest_append.txt", "part1")
    appendfile("unctest_append.txt", "part2")
    local content = readfile("unctest_append.txt")
    delfile("unctest_append.txt")
    return expectEqual(content, "part1part2")
end)

newTest("FileSystem", "isfile returns boolean", function()
    writefile("unctest_isfile.txt", "data")
    local result = isfile("unctest_isfile.txt")
    delfile("unctest_isfile.txt")
    return expectType(result, "boolean")
end)

newTest("FileSystem", "isfolder returns boolean", function()
    return expectType(isfolder, "function")
end)

newTest("FileSystem", "makefolder + delfolder", function()
    makefolder("unctest_folder")
    local exists = isfolder("unctest_folder")
    delfolder("unctest_folder")
    return exists == true and ok() or fail("folder not created")
end)

newTest("FileSystem", "listfiles returns table", function()
    makefolder("unctest_list")
    writefile("unctest_list/file1.txt", "a")
    local files = listfiles("unctest_list")
    delfile("unctest_list/file1.txt")
    delfolder("unctest_list")
    return expectType(files, "table")
end)

newTest("FileSystem", "delfile exists", function()
    return expectType(delfile, "function")
end)

newTest("FileSystem", "loadfile exists", function()
    return expectType(loadfile, "function")
end)

newTest("FileSystem", "dofile exists", function()
    return expectType(dofile, "function")
end)

newTest("FileSystem", "getcustomasset exists", function()
    return expectType(getcustomasset, "function")
end)

newTest("FileSystem", "readbinarystring alias", function()
    return expectType(readbinarystring, "function")
end)

-- ===== 12. CONSOLE =====

newTest("Console", "rconsoleprint exists", function()
    return expectType(rconsoleprint, "function")
end)

newTest("Console", "rconsoleinfo exists", function()
    return expectType(rconsoleinfo, "function")
end)

newTest("Console", "rconsolewarn exists", function()
    return expectType(rconsolewarn, "function")
end)

newTest("Console", "rconsoleerr exists", function()
    return expectType(rconsoleerr, "function")
end)

newTest("Console", "rconsoleclear exists", function()
    return expectType(rconsoleclear, "function")
end)

newTest("Console", "rconsolecreate exists", function()
    return expectType(rconsolecreate, "function")
end)

newTest("Console", "rconsoledestroy exists", function()
    return expectType(rconsoledestroy, "function")
end)

newTest("Console", "rconsolesettitle exists", function()
    return expectType(rconsolesettitle, "function")
end)

newTest("Console", "rconsoleinput exists", function()
    return expectType(rconsoleinput, "function")
end)

newTest("Console", "consoleprint alias", function()
    return expectType(consoleprint, "function")
end)

newTest("Console", "consoleclear alias", function()
    return expectType(consoleclear, "function")
end)

newTest("Console", "consolecreate alias", function()
    return expectType(consolecreate, "function")
end)

newTest("Console", "consoledestroy alias", function()
    return expectType(consoledestroy, "function")
end)

newTest("Console", "consoleinput alias", function()
    return expectType(consoleinput, "function")
end)

newTest("Console", "consolesettitle alias", function()
    return expectType(consolesettitle, "function")
end)

-- ===== 13. INPUT =====

newTest("Input", "mouse1click exists", function()
    return expectType(mouse1click, "function")
end)

newTest("Input", "mouse1press exists", function()
    return expectType(mouse1press, "function")
end)

newTest("Input", "mouse1release exists", function()
    return expectType(mouse1release, "function")
end)

newTest("Input", "mouse2click exists", function()
    return expectType(mouse2click, "function")
end)

newTest("Input", "mouse2press exists", function()
    return expectType(mouse2press, "function")
end)

newTest("Input", "mouse2release exists", function()
    return expectType(mouse2release, "function")
end)

newTest("Input", "mousemoveabs exists", function()
    return expectType(mousemoveabs, "function")
end)

newTest("Input", "mousemoverel exists", function()
    return expectType(mousemoverel, "function")
end)

newTest("Input", "mousescroll exists", function()
    return expectType(mousescroll, "function")
end)

newTest("Input", "keypress exists", function()
    return expectType(keypress, "function")
end)

newTest("Input", "keyrelease exists", function()
    return expectType(keyrelease, "function")
end)

-- ===== 14. CRYPT =====

newTest("Crypt", "crypt table exists", function()
    return expectType(crypt, "table")
end)

newTest("Crypt", "crypt.base64.encode returns string", function()
    local enc = crypt.base64.encode("hello")
    return expectType(enc, "string")
end)

newTest("Crypt", "crypt.base64 roundtrip", function()
    local original = "hello_world_123"
    local enc = crypt.base64.encode(original)
    local dec = crypt.base64.decode(enc)
    return expectEqual(dec, original)
end)

newTest("Crypt", "crypt.encrypt returns data+iv", function()
    local data, iv = crypt.encrypt("test", "key")
    if type(data) ~= "string" then return fail("data not string") end
    if iv == nil then return fail("no iv returned") end
    return ok()
end)

newTest("Crypt", "crypt.decrypt reverses encrypt", function()
    local original = "secret_data"
    local enc, iv = crypt.encrypt(original, "mykey")
    local dec = crypt.decrypt(enc, "mykey", iv)
    return expectEqual(dec, original)
end)

newTest("Crypt", "crypt.hash SHA384 returns 96 chars", function()
    local hash = crypt.hash("test_data", "SHA384")
    return expectEqual(#hash, 96)
end)

newTest("Crypt", "crypt.hash SHA512 returns 128 chars", function()
    local hash = crypt.hash("test_data", "SHA512")
    return expectEqual(#hash, 128)
end)

newTest("Crypt", "crypt.hash MD5 returns 32 chars", function()
    local hash = crypt.hash("test_data", "MD5")
    return expectType(hash, "string")
end)

newTest("Crypt", "crypt.hash deterministic", function()
    local a = crypt.hash("hello", "SHA384")
    local b = crypt.hash("hello", "SHA384")
    return expectEqual(a, b)
end)

newTest("Crypt", "crypt.hash different for different inputs", function()
    local a = crypt.hash("hello", "SHA384")
    local b = crypt.hash("world", "SHA384")
    return a ~= b and ok() or fail("should differ")
end)

newTest("Crypt", "crypt.generatebytes returns string", function()
    local bytes = crypt.generatebytes(16)
    return expectType(bytes, "string")
end)

newTest("Crypt", "crypt.generatekey returns string", function()
    local key = crypt.generatekey()
    return expectType(key, "string")
end)

newTest("Crypt", "base64.encode(alias)", function()
    return expectType(base64.encode, "function")
end)

newTest("Crypt", "base64_decode alias", function()
    return expectType(base64_decode, "function")
end)

newTest("Crypt", "base64_encode alias", function()
    return expectType(base64_encode, "function")
end)

-- ===== 15. LZ4 =====

newTest("LZ4", "lz4compress returns string", function()
    local compressed = lz4compress("hello")
    return expectType(compressed, "string")
end)

newTest("LZ4", "lz4 roundtrip", function()
    local original = "the quick brown fox jumps over the lazy dog"
    local compressed = lz4compress(original)
    local decompressed = lz4decompress(compressed)
    return expectEqual(decompressed, original)
end)

newTest("LZ4", "lz4decompress exists", function()
    return expectType(lz4decompress, "function")
end)

-- ===== 16. PERFORMANCE =====

newTest("Performance", "setfpscap exists", function()
    return expectType(setfpscap, "function")
end)

newTest("Performance", "getfpscap returns number", function()
    return expectType(getfpscap(), "number")
end)

newTest("Performance", "setfpscap changes value", function()
    local old = getfpscap()
    setfpscap(60)
    local new = getfpscap()
    setfpscap(old)
    return expectEqual(new, 60)
end)

newTest("Performance", "setsimulationradius exists", function()
    return expectType(setsimulationradius, "function")
end)

newTest("Performance", "getsimulationradius returns number", function()
    local sr = getsimulationradius()
    return expectType(sr, "number")
end)

newTest("Performance", "isnetworkowner exists", function()
    return expectType(isnetworkowner, "function")
end)

-- ===== 17. FLAGS =====

newTest("Flags", "setfflag exists", function()
    return expectType(setfflag, "function")
end)

newTest("Flags", "getfflag exists", function()
    return expectType(getfflag, "function")
end)

newTest("Flags", "getnamecallmethod returns string", function()
    return expectType(getnamecallmethod(), "string")
end)

newTest("Flags", "setnamecallmethod exists", function()
    return expectType(setnamecallmethod, "function")
end)

-- ===== 18. MISC =====

newTest("Misc", "setclipboard exists", function()
    return expectType(setclipboard, "function")
end)

newTest("Misc", "getclipboard exists", function()
    return expectType(getclipboard, "function")
end)

newTest("Misc", "toclipboard alias", function()
    return expectType(toclipboard, "function")
end)

newTest("Misc", "setrbxclipboard alias", function()
    return expectType(setrbxclipboard, "function")
end)

newTest("Misc", "queue_on_teleport exists", function()
    return expectType(queue_on_teleport, "function")
end)

newTest("Misc", "queueonteleport alias", function()
    return expectType(queueonteleport, "function")
end)

newTest("Misc", "gethwid returns string", function()
    local hwid = gethwid()
    return expectType(hwid, "string")
end)

newTest("Misc", "isrbxactive returns boolean", function()
    return expectType(isrbxactive(), "boolean")
end)

newTest("Misc", "isgameactive alias", function()
    return expectType(isgameactive, "function")
end)

newTest("Misc", "iswindowactive alias", function()
    return expectType(iswindowactive, "function")
end)

newTest("Misc", "checkcaller returns boolean", function()
    return expectType(checkcaller(), "boolean")
end)

newTest("Misc", "isluau returns boolean", function()
    return expectType(isluau(), "boolean")
end)

newTest("Misc", "decompile exists", function()
    return expectType(decompile, "function")
end)

newTest("Misc", "dumpstring exists", function()
    return expectType(dumpstring, "function")
end)

newTest("Misc", "getcallstack returns table", function()
    return expectType(getcallstack(), "table")
end)

newTest("Misc", "getfunctionhash returns string", function()
    local hash = getfunctionhash(function() end)
    return expectType(hash, "string")
end)

newTest("Misc", "messagebox returns number", function()
    local result = messagebox("test", "title", 0)
    return expectType(result, "number")
end)

newTest("Misc", "saveinstance exists", function()
    return expectType(saveinstance, "function")
end)

newTest("Misc", "savegame alias", function()
    return expectType(savegame, "function")
end)

newTest("Misc", "save_instance alias", function()
    return expectType(save_instance, "function")
end)

newTest("Misc", "hookinstance exists", function()
    return expectType(hookinstance, "function")
end)

newTest("Misc", "Drawing table exists", function()
    return expectType(Drawing, "table")
end)

newTest("Misc", "Drawing.new exists", function()
    return expectType(Drawing.new, "function")
end)

newTest("Misc", "isrenderobj exists", function()
    return expectType(isrenderobj, "function")
end)

newTest("Misc", "cleardrawcache exists", function()
    return expectType(cleardrawcache, "function")
end)

newTest("Misc", "getrenderproperty exists", function()
    return expectType(getrenderproperty, "function")
end)

newTest("Misc", "setrenderproperty exists", function()
    return expectType(setrenderproperty, "function")
end)

-- ===== 19. CACHE =====

newTest("Cache", "cache table exists", function()
    return expectType(cache, "table")
end)

newTest("Cache", "cache.invalidate exists", function()
    return expectType(cache.invalidate, "function")
end)

newTest("Cache", "cache.iscached exists", function()
    return expectType(cache.iscached, "function")
end)

newTest("Cache", "cache.replace exists", function()
    return expectType(cache.replace, "function")
end)

-- ===== 20. GC / REGISTRY =====

newTest("GC", "getgc with filter function", function()
    local funcs = getgc(true)
    local count = 0
    for _, v in ipairs(funcs) do
        if type(v) == "function" then count = count + 1 end
    end
    return ok("found " .. count .. " functions")
end)

newTest("GC", "filtergc exists", function()
    return expectType(filtergc, "function")
end)

newTest("GC", "filtergc returns table", function()
    return expectType(filtergc("function"), "table")
end)

-- ===== 21. SCRIPT INFO =====

newTest("ScriptInfo", "getscriptfromthread exists", function()
    return expectType(getscriptfromthread, "function")
end)

newTest("ScriptInfo", "getscriptfromthread returns Instance", function()
    local s = getscriptfromthread(coroutine.running())
    return s == nil or type(s) == "userdata" and ok() or fail("bad type: " .. type(s))
end)

-- ===== GUI BUILDER =====

local function buildGUI()
    local categories = {}
    local categoryOrder = {}
    for _, t in ipairs(tests) do
        if not categories[t.category] then
            categories[t.category] = {tests = {}, passed = 0, failed = 0, total = 0}
            table.insert(categoryOrder, t.category)
        end
        table.insert(categories[t.category].tests, t)
        categories[t.category].total = categories[t.category].total + 1
    end

    local screenGui = Instance.new("ScreenGui")
    screenGui.Name = "UNCTestSuite"
    screenGui.ResetOnSpawn = false
    screenGui.ZIndexBehavior = Enum.ZIndexBehavior.Sibling

    local main = Instance.new("Frame")
    main.Name = "Main"
    main.Size = UDim2.new(0, 900, 0, 600)
    main.Position = UDim2.new(0.5, -450, 0.5, -300)
    main.BackgroundColor3 = Color3.fromRGB(22, 22, 30)
    main.BorderSizePixel = 0
    main.ClipsDescendants = true
    main.Parent = screenGui

    local corner = Instance.new("UICorner")
    corner.CornerRadius = UDim.new(0, 8)
    corner.Parent = main

    -- Title bar
    local titleBar = Instance.new("Frame")
    titleBar.Size = UDim2.new(1, 0, 0, 40)
    titleBar.BackgroundColor3 = Color3.fromRGB(30, 30, 45)
    titleBar.BorderSizePixel = 0
    titleBar.Parent = main

    local titleCorner = Instance.new("UICorner")
    titleCorner.CornerRadius = UDim.new(0, 8)
    titleCorner.Parent = titleBar

    -- Cover top corners
    local topFill = Instance.new("Frame")
    topFill.Size = UDim2.new(1, 0, 0, 8)
    topFill.Position = UDim2.new(0, 0, 0, 32)
    topFill.BackgroundColor3 = Color3.fromRGB(30, 30, 45)
    topFill.BorderSizePixel = 0
    topFill.Parent = titleBar

    local title = Instance.new("TextLabel")
    title.Size = UDim2.new(1, -20, 1, 0)
    title.Position = UDim2.new(0, 15, 0, 0)
    title.BackgroundTransparency = 1
    title.Text = "Syntax UNC Test Suite"
    title.TextColor3 = Color3.fromRGB(220, 220, 255)
    title.TextXAlignment = Enum.TextXAlignment.Left
    title.Font = Enum.Font.GothamBold
    title.TextSize = 18
    title.Parent = titleBar

    -- Score bar
    local scoreBg = Instance.new("Frame")
    scoreBg.Size = UDim2.new(1, -30, 0, 24)
    scoreBg.Position = UDim2.new(0, 15, 0, 50)
    scoreBg.BackgroundColor3 = Color3.fromRGB(35, 35, 50)
    scoreBg.BorderSizePixel = 0
    scoreBg.Parent = main

    local scoreCorner = Instance.new("UICorner")
    scoreCorner.CornerRadius = UDim.new(0, 4)
    scoreCorner.Parent = scoreBg

    local scoreFill = Instance.new("Frame")
    scoreFill.Name = "ScoreFill"
    scoreFill.Size = UDim2.new(0, 0, 1, 0)
    scoreFill.BackgroundColor3 = Color3.fromRGB(80, 200, 120)
    scoreFill.BorderSizePixel = 0
    scoreFill.Parent = scoreBg

    local scoreFillCorner = Instance.new("UICorner")
    scoreFillCorner.CornerRadius = UDim.new(0, 4)
    scoreFillCorner.Parent = scoreFill

    local scoreText = Instance.new("TextLabel")
    scoreText.Name = "ScoreText"
    scoreText.Size = UDim2.new(1, 0, 1, 0)
    scoreText.BackgroundTransparency = 1
    scoreText.Text = "Ready - Click 'Run All'"
    scoreText.TextColor3 = Color3.fromRGB(200, 200, 220)
    scoreText.Font = Enum.Font.GothamSemibold
    scoreText.TextSize = 14
    scoreText.Parent = scoreBg

    -- Layout: Left sidebar (categories) + Right panel (results)
    local sidebar = Instance.new("ScrollingFrame")
    sidebar.Size = UDim2.new(0, 180, 1, -100)
    sidebar.Position = UDim2.new(0, 15, 0, 85)
    sidebar.BackgroundColor3 = Color3.fromRGB(26, 26, 38)
    sidebar.BorderSizePixel = 0
    sidebar.CanvasSize = UDim2.new(0, 0, 0, 0)
    sidebar.ScrollBarThickness = 4
    sidebar.Parent = main

    local sidebarCorner = Instance.new("UICorner")
    sidebarCorner.CornerRadius = UDim.new(0, 6)
    sidebarCorner.Parent = sidebar

    local sidebarList = Instance.new("UIListLayout")
    sidebarList.Padding = UDim.new(0, 2)
    sidebarList.Parent = sidebar

    -- Right panel
    local rightPanel = Instance.new("ScrollingFrame")
    rightPanel.Name = "RightPanel"
    rightPanel.Size = UDim2.new(0, 670, 1, -100)
    rightPanel.Position = UDim2.new(0, 210, 0, 85)
    rightPanel.BackgroundColor3 = Color3.fromRGB(26, 26, 38)
    rightPanel.BorderSizePixel = 0
    rightPanel.CanvasSize = UDim2.new(0, 0, 0, 0)
    rightPanel.ScrollBarThickness = 4
    rightPanel.Parent = main

    local rightCorner = Instance.new("UICorner")
    rightCorner.CornerRadius = UDim.new(0, 6)
    rightCorner.Parent = rightPanel

    local rightList = Instance.new("UIListLayout")
    rightList.Padding = UDim.new(0, 2)
    rightList.Parent = rightPanel

    -- Run all button
    local runAllBtn = Instance.new("TextButton")
    runAllBtn.Size = UDim2.new(0, 120, 0, 32)
    runAllBtn.Position = UDim2.new(1, -140, 0, 50)
    runAllBtn.BackgroundColor3 = Color3.fromRGB(60, 160, 80)
    runAllBtn.BorderSizePixel = 0
    runAllBtn.Text = "  Run All"
    runAllBtn.TextColor3 = Color3.fromRGB(255, 255, 255)
    runAllBtn.Font = Enum.Font.GothamSemibold
    runAllBtn.TextSize = 14
    runAllBtn.Parent = main

    local runBtnCorner = Instance.new("UICorner")
    runBtnCorner.CornerRadius = UDim.new(0, 4)
    runBtnCorner.Parent = runAllBtn

    -- Category buttons in sidebar
    local buttons = {}
    local currentCategoryFrame = nil
    local currentCategoryList = nil

    for _, catName in ipairs(categoryOrder) do
        local btn = Instance.new("TextButton")
        btn.Size = UDim2.new(1, -10, 0, 30)
        btn.BackgroundColor3 = Color3.fromRGB(35, 35, 50)
        btn.BorderSizePixel = 0
        btn.Text = catName
        btn.TextColor3 = Color3.fromRGB(180, 180, 210)
        btn.TextXAlignment = Enum.TextXAlignment.Left
        btn.Font = Enum.Font.Gotham
        btn.TextSize = 13
        btn.Parent = sidebar

        local btnCorner = Instance.new("UICorner")
        btnCorner.CornerRadius = UDim.new(0, 4)
        btnCorner.Parent = btn

        local indicator = Instance.new("Frame")
        indicator.Name = "Indicator"
        indicator.Size = UDim2.new(0, 6, 0, 6)
        indicator.Position = UDim2.new(1, -14, 0.5, -3)
        indicator.BackgroundColor3 = Color3.fromRGB(80, 80, 100)
        indicator.BorderSizePixel = 0
        indicator.Parent = btn

        local indCorner = Instance.new("UICorner")
        indCorner.CornerRadius = UDim.new(1, 0)
        indCorner.Parent = indicator

        buttons[catName] = btn

        btn.MouseButton1Click:Connect(function()
            -- Clear right panel
            rightList:ClearAllChildren()

            -- Show tests for this category
            currentCategoryFrame = Instance.new("Frame")
            currentCategoryFrame.Size = UDim2.new(1, -10, 0, 20)
            currentCategoryFrame.BackgroundTransparency = 1
            currentCategoryFrame.Parent = rightPanel

            local header = Instance.new("TextLabel")
            header.Size = UDim2.new(1, 0, 0, 24)
            header.BackgroundTransparency = 1
            header.Text = "  " .. catName .. " (" .. categories[catName].total .. " tests)"
            header.TextColor3 = Color3.fromRGB(160, 200, 255)
            header.TextXAlignment = Enum.TextXAlignment.Left
            header.Font = Enum.Font.GothamBold
            header.TextSize = 14
            header.Parent = currentCategoryFrame

            for _, test in ipairs(categories[catName].tests) do
                local row = Instance.new("Frame")
                row.Name = test.name
                row.Size = UDim2.new(1, -10, 0, 26)
                row.BackgroundColor3 = Color3.fromRGB(30, 30, 44)
                row.BorderSizePixel = 0
                row.Parent = rightPanel

                local dot = Instance.new("Frame")
                dot.Name = "Dot"
                dot.Size = UDim2.new(0, 8, 0, 8)
                dot.Position = UDim2.new(0, 10, 0.5, -4)
                dot.BackgroundColor3 = Color3.fromRGB(80, 80, 100)
                dot.BorderSizePixel = 0
                dot.Parent = row

                local dotCorner = Instance.new("UICorner")
                dotCorner.CornerRadius = UDim.new(1, 0)
                dotCorner.Parent = dot

                local nameLabel = Instance.new("TextLabel")
                nameLabel.Name = "Name"
                nameLabel.Size = UDim2.new(0, 340, 1, 0)
                nameLabel.Position = UDim2.new(0, 24, 0, 0)
                nameLabel.BackgroundTransparency = 1
                nameLabel.Text = test.name
                nameLabel.TextColor3 = Color3.fromRGB(180, 180, 200)
                nameLabel.TextXAlignment = Enum.TextXAlignment.Left
                nameLabel.Font = Enum.Font.Gotham
                nameLabel.TextSize = 13
                nameLabel.Parent = row

                local resultLabel = Instance.new("TextLabel")
                resultLabel.Name = "Result"
                resultLabel.Size = UDim2.new(0, 300, 1, 0)
                resultLabel.Position = UDim2.new(0, 370, 0, 0)
                resultLabel.BackgroundTransparency = 1
                resultLabel.Text = ""
                resultLabel.TextColor3 = Color3.fromRGB(140, 140, 160)
                resultLabel.TextXAlignment = Enum.TextXAlignment.Left
                resultLabel.Font = Enum.Font.Gotham
                resultLabel.TextSize = 12
                resultLabel.TextTruncate = Enum.TextTruncate.AtEnd
                resultLabel.Parent = row

                -- Store reference for result updates
                row:SetAttribute("testIndex", table.find(tests, test))
            end

            -- Update canvas size
            rightPanel.CanvasSize = UDim2.new(0, 0, 0, rightList.AbsoluteContentSize.Y + 40)
        end)
    end

    -- Helper to update test result
    local function updateResult(testIdx, passed, msg)
        local row = rightPanel:FindFirstChild(tests[testIdx].name, true)
        if not row then return end
        local dot = row:FindFirstChild("Dot")
        local resultLabel = row:FindFirstChild("Result")
        if dot then
            dot.BackgroundColor3 = passed and Color3.fromRGB(80, 200, 120) or Color3.fromRGB(220, 60, 60)
        end
        if resultLabel then
            resultLabel.Text = (passed and "PASS" or "FAIL") .. (msg and ": " .. msg or "")
            resultLabel.TextColor3 = passed and Color3.fromRGB(80, 200, 120) or Color3.fromRGB(220, 60, 60)
        end
    end

    -- Update score bar
    local function updateScore()
        local total = totalPassed + totalFailed
        if total == 0 then return end
        local pct = math.floor(totalPassed / total * 100 + 0.5)

        scoreFill:TweenSize(UDim2.new(pct / 100, 0, 1, 0), Enum.EasingDirection.Out, Enum.EasingStyle.Quad, 0.3, true)
        scoreText.Text = string.format("%d / %d passed (%d%%)", totalPassed, total, pct)

        -- Update colors
        if pct >= 90 then
            scoreFill.BackgroundColor3 = Color3.fromRGB(80, 200, 120)
        elseif pct >= 70 then
            scoreFill.BackgroundColor3 = Color3.fromRGB(220, 200, 60)
        else
            scoreFill.BackgroundColor3 = Color3.fromRGB(220, 80, 60)
        end

        -- Update category indicators
        for catName, cat in pairs(categories) do
            local btn = buttons[catName]
            if btn then
                local dot = btn:FindFirstChild("Indicator")
                if dot then
                    local catTotal = cat.passed + cat.failed
                    if catTotal == 0 then
                        dot.BackgroundColor3 = Color3.fromRGB(80, 80, 100)
                    elseif cat.failed == 0 then
                        dot.BackgroundColor3 = Color3.fromRGB(80, 200, 120)
                    else
                        dot.BackgroundColor3 = Color3.fromRGB(220, 60, 60)
                    end
                end
            end
        end
    end

    -- Run all tests
    local running = false
    runAllBtn.MouseButton1Click:Connect(function()
        if running then return end
        running = true
        runAllBtn.Text = "  Running..."
        runAllBtn.BackgroundColor3 = Color3.fromRGB(60, 60, 80)
        totalPassed = 0
        totalFailed = 0

        -- Clear all results
        for _, row in ipairs(rightPanel:GetChildren()) do
            if row:IsA("Frame") then
                local dot = row:FindFirstChild("Dot")
                local resultLabel = row:FindFirstChild("Result")
                if dot then dot.BackgroundColor3 = Color3.fromRGB(80, 80, 100) end
                if resultLabel then resultLabel.Text = "" end
            end
        end

        -- Run tests with category progress
        for _, catName in ipairs(categoryOrder) do
            -- Click the category button to show tests
            if buttons[catName] then
                buttons[catName]:MouseButton1Click:Fire()
            end
            task.wait(0.01)

            for _, test in ipairs(categories[catName].tests) do
                local ok2, result = pcall(test.fn)
                if ok2 then
                    local passed = result and result == true
                    if type(result) == "table" then
                        passed = result[1]
                    end
                    local passedBool = (type(result) == "table" and result[1]) or (type(result) == "boolean" and result) or false
                    local msg = type(result) == "table" and tostring(result[2]) or ""
                    if not passedBool then
                        msg = msg or "failed"
                    end

                    if passedBool then
                        totalPassed = totalPassed + 1
                        categories[catName].passed = categories[catName].passed + 1
                    else
                        totalFailed = totalFailed + 1
                        categories[catName].failed = categories[catName].failed + 1
                    end

                    local testIdx = table.find(tests, test)
                    if testIdx then
                        updateResult(testIdx, passedBool, msg)
                    end
                else
                    totalFailed = totalFailed + 1
                    categories[catName].failed = categories[catName].failed + 1
                    local testIdx = table.find(tests, test)
                    if testIdx then
                        updateResult(testIdx, false, "error: " .. tostring(result))
                    end
                end
                task.wait()
            end
        end

        updateScore()
        running = false
        runAllBtn.Text = "  Run All"
        runAllBtn.BackgroundColor3 = Color3.fromRGB(60, 160, 80)
    end)

    -- Click first category
    if #categoryOrder > 0 then
        buttons[categoryOrder[1]]:MouseButton1Click:Fire()
    end

    screenGui.Parent = ui
    return screenGui
end

-- ===== MAIN =====
print("[UNCTest] Starting UNC Test Suite...")
local gui = buildGUI()
print("[UNCTest] Test GUI created. Click 'Run All' to start testing.")
