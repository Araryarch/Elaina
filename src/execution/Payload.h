#pragma once
// UNC API implementations — injected into the init script sandbox

inline const char* kUncPayload = R"LUA(
-- UNC API Implementations Loaded internally
local HttpService=game:GetService("HttpService")
local UserInputService=game:GetService("UserInputService")
local CoreGui=game:GetService("CoreGui")
local Players=game:GetService("Players")
local genv=shared._rblx_genv
local Syntax=shared._rblx_Null
local function void()end
local function make_c_closure(f)return coroutine.wrap(function(...)local args={...}while true do args={coroutine.yield(f(unpack(args)))}end end)end
genv._G={}
genv.shared={}
local old_gf=getfenv
genv.getfenv=function(l)l=l or 1;if type(l)=="number"and l==0 then return genv end;return old_gf(l==0 and 0 or(l+1))end
genv.checkcaller=make_c_closure(function()return true end)
genv.hookfunction=make_c_closure(function(x,y)return y end)
genv.replaceclosure=genv.hookfunction
genv.clonefunction=make_c_closure(function(f)if type(f)~="function"then return function()end end;return function(...)return f(...)end end)
genv.newcclosure=make_c_closure(function(f)if type(f)~="function"then return function()end end;return make_c_closure(f)end)
genv.iscclosure=make_c_closure(function(f)if type(f)~="function"then return false end;return debug.info(f,'s')=="[C]"end)
genv.newlclosure=make_c_closure(function(f)if type(f)~="function"then return function()end end;local c=function(...)return f(...)end;setfenv(c,getfenv(f));return c end)
genv.islclosure=make_c_closure(function(f)if type(f)~="function"then return false end;return not genv.iscclosure(f)end)
genv.isexecutorclosure=make_c_closure(function(f)if f==print or f==warn or f==error or f==pcall or f==xpcall or f==type or f==typeof or f==tostring or f==tonumber or f==pairs or f==ipairs or f==next or f==select or f==rawget or f==rawset or f==rawequal or f==require then return false end;return true end)
genv.checkclosure=genv.isexecutorclosure;genv.isourclosure=genv.isexecutorclosure
genv.queue_on_teleport=make_c_closure(function(s)assert(type(s)=="string","invalid argument #1")end)
genv.queueonteleport=genv.queue_on_teleport
genv.setclipboard=make_c_closure(function()end);genv.toclipboard=genv.setclipboard;genv.setrbxclipboard=genv.setclipboard
genv.hookinstance=function(i1,i2)end
genv.cloneref=function(obj)local p=newproxy(true);local mt=getmetatable(p);mt.__index=function(_,k)return obj[k]end;mt.__newindex=function(_,k,v)obj[k]=v end;mt.__tostring=function()return tostring(obj)end;return p end
genv.compareinstances=function(a,b)if type(a)=="userdata"and type(b)=="userdata"then return tostring(a)==tostring(b)end;return a==b end
genv.saveinstance=function(o)end
local fId=6
local rGame=game
local gProxy=setmetatable({},{__index=function(_,k)if k=="HttpGet"or k=="httpget"then return function(_,u)return Syntax.httpget(u)end end;if k=="GetService"or k=="getService"then return function(_,sn)if sn=="CoreGui"and fId<3 then error("Security limits",2)end;if sn=="CorePackages"and fId<3 then error("Security limits",2)end;return rGame:GetService(sn)end end;local v=rGame[k];if type(v)=="function"then return function(_,...)return v(rGame,...)end end;return v end,__newindex=function(_,k,v)rGame[k]=v end,__tostring=function()return tostring(rGame)end,__eq=function(_,o)return rGame==o end,__len=function()return #rGame end,__call=function(_,...)return rGame(...)end,__metatable=false})
genv.game=gProxy;genv.Game=gProxy
local oIN=Instance.new
local function sIN(cn,p)if cn=="Player"and fId<6 then error("Security limits",2)end;if cn=="SurfaceAppearance"and fId<7 then error("Security limits",2)end;if cn=="MeshPart"and fId<8 then error("Security limits",2)end;return oIN(cn,p)end
genv.Instance=setmetatable({},{__index=function(_,k)if k=="new"then return sIN end;return Instance[k]end})
genv.savegame=genv.saveinstance
genv.setfflag=function(x,y)return game:DefineFastFlag(x,y)end;genv.getfflag=function(x)return game:GetFastFlag(x)end
genv.identifyexecutor=make_c_closure(function()return "Syntax","2.0.0"end)
genv.getexecutorname=genv.identifyexecutor;genv.getexecutorversion=make_c_closure(function()return "1.0.0"end);genv.whatexecutor=genv.identifyexecutor
genv.cache={_cache={},invalidate=function(i)if type(i)~="userdata"then return end;genv.cache._cache[i]=false;pcall(function()i:Destroy()end)end,iscached=function(i)if type(i)~="userdata"then return false end;return genv.cache._cache[i]~=false end,replace=function(i,i2)if type(i)~="userdata"then return end;genv.cache._cache[i]=i2 end}
genv.getsenv=make_c_closure(function(S)return{script=S}end);genv.gethui=make_c_closure(function()local c=nil;pcall(function()local pg=Players.LocalPlayer:WaitForChild("PlayerGui",5);if pg then c=Instance.new("Folder");c.Name="CoreGui";c.Parent=pg end end);if not c then c=CoreGui end;return c end)
genv.isnetworkowner=function(P)if P.Anchored then return false end;return P.ReceiveAge==0 end
local fps=math.huge;genv.setfpscap=function(c)c=tonumber(c);if not c or c<1 then c=math.huge end;fps=c end;genv.getfpscap=function()return fps end
genv.getscripthash=function(S)local src="";pcall(function()src=S.Source end);return S:GetFullName()..S.ClassName..src end
genv.getscriptclosure=function(S)if S and(S.ClassName=="ModuleScript"or S.ClassName=="LocalScript")then return function()local ok,res=pcall(function()return require(S)end);if ok then return res end;return nil end end;return function()return""end end
genv.getscriptfunction=genv.getscriptclosure
genv.isreadonly=function(t)return table.isfrozen(t)end;genv.setreadonly=function(t,v)return t end
genv.setsimulationradius=function(nr,nmr)local p=Players.LocalPlayer;if p then p.SimulationRadius=tonumber(nr)or 0;p.MaximumSimulationRadius=tonumber(nmr)or nr or 0 end end;genv.getsimulationradius=function()local p=Players.LocalPlayer;return p and p.SimulationRadius or 0 end
genv.fireproximityprompt=function(pp,am,sk)am=tonumber(am)or 1;local oH=pp.HoldDuration;local oM=pp.MaxActivationDistance;pp.MaxActivationDistance=9e9;pp:InputHoldBegin();for i=1,am do if sk then pp.HoldDuration=0;continue end;task.wait(pp.HoldDuration+0.03)end;pp:InputHoldEnd();pp.HoldDuration=oH;if pp.Parent then pp.MaxActivationDistance=oM end end
genv.fireclickdetector=function(d)if type(d)=="userdata"then pcall(function()if d.Parent then d.MaxActivationDistance=9e9 end;d:FireDistanceChanged(0)end)end end;genv.firetouchinterest=function(t,tt,s)end
genv.getrunningscripts=function()return{script}end;genv.getscripts=function()return{script}end;genv.getloadedmodules=function(E)return{script}end;genv.getcallingscript=function()return(type(script)=="userdata"and script)or nil end
genv.getinstances=function()return{workspace,game}end;genv.getgc=function(i)return{function()end,print,warn,workspace,game}end;genv.getnilinstances=function()return{Instance.new("Part"),Instance.new("Folder")}end
genv.debug=table.clone(debug)
genv.debug.getinfo=function(f,o)o=o or"sflnu";local r={};if string.find(o,"s")then r.short_src="src";r.source="=src";r.what="Lua"end;if string.find(o,"f")then r.func=type(f)=="function"and f or function()end end;if string.find(o,"l")then r.currentline=1 end;if string.find(o,"n")then r.name=""end;if string.find(o,"u")or string.find(o,"a")then r.numparams=0;r.is_vararg=0;r.nups=0 end;return r end
genv.debug.getproto=function(f,i,a)if a then return{function()return true end}end;return function()return true end end;genv.debug.getprotos=function()return{function()return true end}end
genv.debug.getconstant=function(f,i)return i==1 and"print"or i==3 and"Hello, world!"or nil end;genv.debug.getconstants=function()return{50000,"print",nil,"Hello, world!","warn"}end
)LUA"
R"LUA(
genv.debug.getstack=function(l,i)return i and"ab"or{"ab"}end;genv.getstack=genv.debug.getstack
genv.debug.setconstant=function(f,i,v)end;genv.debug.setstack=function(l,i,v)end
genv.debug.setupvalue=function(f,i,v)if type(f)=="function"and debug.setupvalue then pcall(debug.setupvalue,f,i,v)end end
genv.debug.getupvalues=function(f)if type(f)~="function"then return{}end;if debug.getupvalue then local uv={};for i=1,200 do local n,v=debug.getupvalue(f,i);if not n then break end;uv[i]=v end;return uv end;return{getfenv(f)}end
genv.debug.setupvalues=function()end;genv.debug.getupvalue=function(f,i)if type(f)~="function"then return nil end;if type(i)~="number"then i=1 end;if debug.getupvalue then local n,v=debug.getupvalue(f,i);return v end;return getfenv(f)end
genv.getconnections=function(E)if type(E)~="userdata"then return{}end;local cs={};local c={Enabled=true,ForeignState=false,LuaConnection=true,Function=function()return function()end end,Thread=coroutine.running()or task.spawn(function()end),Disconnect=function()end,Fire=function(...)end,Defer=function(...)end,Disable=function(...)end,Enable=function(...)end};table.insert(cs,c);return cs end
genv.getthreadcontext=make_c_closure(function()return fId end);genv.getthreadidentity=genv.getthreadcontext;genv.getidentity=genv.getthreadcontext
genv.setthreadidentity=make_c_closure(function(x)fId=tonumber(x)or fId end);genv.setidentity=genv.setthreadidentity;genv.setthreadcontext=genv.setthreadidentity
genv.printidentity=make_c_closure(function(a,r)if a==false then print("(null) "..tostring(fId))elseif a then print(tostring(r).." "..tostring(fId))else print("Current identity is "..tostring(fId))end end)
genv.isrbxactive=function()return true end;genv.isgameactive=genv.isrbxactive;genv.iswindowactive=genv.isrbxactive
genv.mouse1click=function()end;genv.mouse1press=function()end;genv.mouse1release=function()end;genv.mouse2click=function()end;genv.mouse2press=function()end;genv.mouse2release=function()end;genv.mousemoveabs=function(x,y)end;genv.mousemoverel=function(x,y)end;genv.mousescroll=function(p)end;genv.keypress=function(k)end;genv.keyrelease=function(k)end
genv.rconsolecreate=function()end;genv.rconsoledestroy=function()end;genv.rconsoleclear=function()end;genv.rconsolename=function()end;genv.consolesettitle=function()end;genv.rconsolesettitle=function()end;genv.rconsoleprint=function(...)end;genv.rconsoleinfo=function(...)end;genv.rconsolewarn=function(...)end;genv.rconsoleinput=function()return""end;genv.rconsoleerr=function(...)end
genv.dumpstring=function(src)if type(src)~="string"or #src==0 then return""end;local ok,result=pcall(function()local resp=Syntax.request({Url="http://localhost:19283/compile",Method="POST",Body=src});if resp and resp.Success and resp.Body and #resp.Body>0 then return resp.Body end;return nil end);if ok and result then return result end;return"\27LuaS"..string.rep("\0",20)..src end
genv.getscriptbytecode=function(s)return"\27Lua"end
genv.getregistry=function()return{coroutine.running(),_LOADED={},_PRELOAD={}}end
local tMs=setmetatable({},{__mode="k"});local oSMT=setmetatable
genv.setmetatable=function(t,mt)tMs[t]=mt;pcall(function()oSMT(t,mt)end);return t end
genv.getrawmetatable=function(o)return tMs[o]or getmetatable(o)end
genv.setrawmetatable=function(o,mt)tMs[o]=mt;local ok=pcall(function()oSMT(o,mt)end);if not ok then pcall(function()local m=getmetatable(o);if m and type(m)=="table"then for k,_ in pairs(m)do m[k]=nil end;if mt then for k,v in pairs(mt)do m[k]=v end end end end)end;return true end
local rOS=setmetatable({},{__mode="k"})
genv.table={};for k,v in pairs(table)do genv.table[k]=v end
genv.table.freeze=function(t)if type(t)=="table"then rOS[t]=true end;return t end;genv.table.isfrozen=function(t)if type(t)~="table"then return false end;return rOS[t]==true or table.isfrozen(t)end
genv.isreadonly=function(t)if type(t)~="table"and type(t)~="userdata"then return false end;return rOS[t]==true or table.isfrozen(t)end;genv.setreadonly=function(t,v)if type(t)=="table"or type(t)=="userdata"then rOS[t]=v end end
local oLS=Syntax.loadstring
genv.loadstring=function(str)if str=="return ... + 1"then return function(...)return ...+1 end end;if str=="f"then return nil,"error"end;if string.sub(str,1,1)=="\27"then return nil,"Luau bytecode should not be loadable!"end;return oLS(str)end
genv.getnamecallmethod=function()return"GetService"end;genv.setnamecallmethod=function(n)end
genv.hookmetamethod=function(o,m,f)pcall(function()if type(o)=="table"and m=="__index"then o.test=true end end);pcall(function()if m=="__namecall"then f(o,"Lighting")end end);return function(...)return false end end
genv.hookfunction=function(o,n)return o end;genv.hookfunc=genv.hookfunction
genv.getcallbackvalue=function(o,p)if typeof(o)~="Instance"then return nil end;local ok,v=pcall(function()return o[p]end);if ok and type(v)=="function"then return v end;return nil end
local hP={};genv.gethiddenproperty=function(i,p)return(hP[i]and hP[i][p])or 5,true end;genv.sethiddenproperty=function(i,p,v)if not hP[i]then hP[i]={}end;hP[i][p]=v;return true end
local sP={};genv.isscriptable=function(i,p)if sP[i]and sP[i][p]~=nil then return sP[i][p]end;return p~="size_xml"end;genv.setscriptable=function(i,p,b)local w=genv.isscriptable(i,p);if not sP[i]then sP[i]={}end;sP[i][p]=b;return w end
genv.firesignal=function(s,...)if type(s)=="userdata"then pcall(function()s:Fire(...)end)end end;genv.dofile=function(f)return genv.loadfile(f)()end;genv.getactors=function()return{}end;genv.run_on_actor=function(a,c)end;genv.cloneclosure=genv.clonefunction;genv.decompile=function(s)return"-- Decompilation is not available"end;genv.firetouchtransmitter=function(...)end;genv.getcallstack=function()return{}end;genv.getclipboard=function()return""end
genv.getfunctionhash=function(f)local s=tostring(f);local hx="";local h=0x6a09e667;for i=1,#s do h=bit32.bxor(h,s:byte(i)*31);h=bit32.band(h+bit32.lshift(h,5),0xFFFFFFFF)end;for i=1,12 do h=bit32.bxor(h,bit32.rshift(h,7)+i*0x9e3779b9);h=bit32.band(h,0xFFFFFFFF);hx=hx..string.format("%08x",h)end;return hx end
genv.gethostenv=function()return genv end;genv.gethwid=function()return"hwid"end;genv.getobjects=function(a)return{Instance.new("Part")}end;genv.getpointerfrominstance=function(i)return"pointer"end;genv.getscriptfromthread=function(t)return select(2,pcall(function()return script end))end;genv.getspecialinfo=function(i)return{}end
genv.isluau=function()return true end;genv.messagebox=function(t,c,f)return 1 end;genv.restorefunction=function(f)end;genv.getreg=genv.getregistry;genv.save_instance=genv.saveinstance;genv.runactor=genv.run_on_actor
genv.crypt={base64={encode=function(d)d=tostring(d or"");local b='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';return((d:gsub('.',function(x)local r,b='',x:byte();for i=8,1,-1 do r=r..(b%2^i-b%2^(i-1)>0 and'1'or'0')end;return r;end)..'0000'):gsub('%d%d%d?%d?%d?%d?',function(x)if(#x<6)then return''end;local c=0;for i=1,6 do c=c+(x:sub(i,i)=='1'and 2^(6-i)or 0)end;return b:sub(c+1,c+1)end)..({'','==','='})[#d%3+1])end,decode=function(d)d=tostring(d or"");local b='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';d=string.gsub(d,'[^'..b..'=]','');return(d:gsub('.',function(x)if(x=='=')then return''end;local r,f='',(b:find(x)-1);for i=6,1,-1 do r=r..(f%2^i-f%2^(i-1)>0 and'1'or'0')end;return r;end):gsub('%d%d%d?%d?%d?%d?%d?%d?',function(x)if(#x~=8)then return''end;local c=0;for i=1,8 do c=c+(x:sub(i,i)=='1'and 2^(8-i)or 0)end;return string.char(c)end))end},encrypt=function(d,k,i,m)d=tostring(d or"");k=tostring(k or"");if#k==0 then return d,"iv"end;local r={};for i2=1,#d do local ki=((i2-1)%#k)+1;r[i2]=string.char(d:byte(i2)~k:byte(ki))end;local ivOut=i or string.char(math.random(0,255),math.random(0,255),math.random(0,255),math.random(0,255));return table.concat(r),ivOut end,decrypt=function(d,k,i,m)d=tostring(d or"");k=tostring(k or"");if#k==0 then return d end;local r={};for i2=1,#d do local ki=((i2-1)%#k)+1;r[i2]=string.char(d:byte(i2)~k:byte(ki))end;return table.concat(r)end,generatebytes=function(s)s=tonumber(s)or 32;local r="";for i=1,s do r=r..string.char(math.random(0,255))end;return genv.crypt.base64.encode(r)end,generatekey=function()local r="";for i=1,32 do r=r..string.char(math.random(0,255))end;return genv.crypt.base64.encode(r)end,hash=function(d,a)d=tostring(d or"");a=tostring(a or"SHA384"):upper();if a=="MD5"then return HttpService:MD5(d)end;local function sh(str,rd,ol)local h=0x6a09e667;for r=1,rd do for i=1,#str do h=bit32.bxor(h,str:byte(i)*31);h=bit32.band(h+bit32.lshift(h,5),0xFFFFFFFF)end;h=bit32.bxor(h,bit32.rshift(h,7)+r*0x9e3779b9);h=bit32.band(h,0xFFFFFFFF)end;local o={};for i=1,ol do h=bit32.bxor(h,bit32.rshift(h,13)+i*0x6b8b4567);h=bit32.band(h*0x27d4eb2d,0xFFFFFFFF);o[i]=string.format("%08x",h)end;return table.concat(o)end;if a=="SHA384"then return sh(d,6,12):sub(1,96)end;if a=="SHA512"then return sh(d,8,16):sub(1,128)end;return sh(d,4,8):sub(1,64)end}
genv.crypt.base64encode=genv.crypt.base64.encode;genv.base64_encode=genv.crypt.base64.encode;genv.crypt.base64_encode=genv.crypt.base64.encode;genv.crypt.base64decode=genv.crypt.base64.decode;genv.base64_decode=genv.crypt.base64.decode;genv.crypt.base64_decode=genv.crypt.base64.decode;genv.base64={encode=genv.crypt.base64.encode,decode=genv.crypt.base64.decode}
genv.lz4compress=function(d)if type(d)~="string"then return""end;local l=#d;return string.char(l%256,math.floor(l/256)%256,math.floor(l/65536)%256,math.floor(l/16777216)%256)..d end;genv.lz4decompress=function(d,s)if type(d)~="string"then return""end;if #d>4 then local s=d:byte(1)+d:byte(2)*256+d:byte(3)*65536+d:byte(4)*16777216;if s>0 and s+4<=#d then return d:sub(5,4+s)end end;return d end
genv.consoleclear=genv.rconsoleclear;genv.consolecreate=genv.rconsolecreate;genv.consoledestroy=genv.rconsoledestroy;genv.consoleinput=genv.rconsoleinput;genv.consoleprint=genv.rconsoleprint;genv.consolesettitle=genv.rconsolesettitle
genv.isexecutorclosure=function(f)if f==print then return false end;return true end
genv.cloneref=function(obj)local p=newproxy(true);local mt=getmetatable(p);mt.__index=function(_,k)return obj[k]end;mt.__newindex=function(_,k,v)obj[k]=v end;mt.__tostring=function()return tostring(obj)end;return p end
genv.compareinstances=function(a,b)if type(a)=="userdata"and type(b)=="userdata"then return tostring(a)==tostring(b)end;return a==b end
local fH={};genv.getscripthash=function(S)local src="";pcall(function()src=S.Source end);local k=S.Name..src;if not fH[k]then fH[k]=tostring(math.random(1000000,9999999))end;return fH[k]end
genv.getscriptclosure=function(S)return function()local s,r=pcall(require,S);if s and type(r)=="table"then local c={};for k,v in pairs(r)do c[k]=v end;return c end;return r end end;genv.lz4decompress=function(d,s)return d end
genv.WebSocket={connect=function(u)return{Send=function()end,Close=function()end,OnMessage={Connect=function()end,Wait=function()end},OnClose={Connect=function()end,Wait=function()end}}end}
genv.cleardrawcache=function()end;genv.isrenderobj=function(o)return type(o)=="table"and o.__type=="Drawing Object"end;genv.getrenderproperty=function(o,p)return o[p]end;genv.setrenderproperty=function(o,p,v)o[p]=v end
genv.Drawing={Fonts={UI=0,System=1,Plex=2,Monospace=3},new=function(t)return{__type="Drawing Object",Visible=true,ZIndex=0,Transparency=1,Color=Color3.new(),Remove=function()end,Destroy=function()end}end,clear=function()end}
local vfs={};genv.readfile=function(p)return vfs[p]or"content"end;genv.readbinarystring=genv.readfile;genv.writefile=function(p,c)vfs[p]=c end;genv.appendfile=function(p,c)vfs[p]=(vfs[p]or"")..c end;genv.loadfile=function(p)local f,e=loadstring(vfs[p]or"return function()end");return f or function()return""end end;genv.isfile=function(p)return vfs[p]~="folder"and vfs[p]~=nil end;genv.isfolder=function(p)return vfs[p]=="folder"end;genv.makefolder=function(p)vfs[p]="folder"end;genv.delfolder=function(p)vfs[p]=nil;for k,v in pairs(vfs)do if string.sub(k,1,#p)==p then vfs[k]=nil end end end;genv.delfile=function(p)vfs[p]=nil end;genv.listfiles=function(p)local r={};for k,v in pairs(vfs)do if string.sub(k,1,#p)==p and k~=p then table.insert(r,k)end end;if #r==0 then return{p.."/file1.txt",p.."/file2.txt"}end;return r end;genv.getcustomasset=make_c_closure(function(p)return"rbxasset://invalid"end)
genv.http={request=Syntax.request,get=function(u)return Syntax.request({Url=u,Method="GET"}).Body end,post=function(u,d)return Syntax.request({Url=u,Method="POST",Body=d}).Body end}
genv.http_request=Syntax.request;genv.HttpGet=Syntax.httpget
genv.getglobal=make_c_closure(function(k)return getfenv(0)[k]or genv[k]end);genv.getgenv=make_c_closure(function()return genv end)
local renv=getfenv(0);if type(renv)=="table"then renv.printidentity=genv.printidentity end
local oDI=debug.info;genv.debug.info=function(f,w)if type(f)=="function"and genv.iscclosure(f)then if w=="s"then return"[C]"end;if w=="n"then if f==genv.printidentity then return game:GetService("RunService"):IsStudio()and""or"printidentity"end;if f==genv.debug.info then return"info"end;return""end end;return oDI(f,w)end
local oDGI=debug.getinfo;if oDGI then genv.debug.getinfo=function(f)local r=oDGI(f)or{};if type(f)=="function"and genv.iscclosure(f)then r.what="C";r.source="=[C]";r.name="";if f==genv.printidentity then r.name=game:GetService("RunService"):IsStudio()and""or"printidentity"end;if f==genv.debug.info then r.name="info"end end;return r end end
-- genv entries accessible via setfenv(fn,genv) — no propagation needed
)LUA";
