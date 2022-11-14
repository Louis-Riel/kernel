local cjson = require "cjson"
local str = require "resty.string"

function BakeKey()
    if not ngx.req.get_uri_args().path then
        ngx.status = 400
        ngx.say("Missing path parameter")
        ngx.exit(ngx.OK)
    end

    if not ngx.req.get_uri_args().method then
        ngx.status = 400
        ngx.say("Missing method parameter")
        ngx.exit(ngx.OK)
    end

    if not ngx.req.get_uri_args().ttl then
        ngx.status = 400
        ngx.say("Missing ttl parameter")
        ngx.exit(ngx.OK)
    end

    if not ngx.req.get_uri_args().name then
        ngx.status = 400
        ngx.say("Missing name parameter")
        ngx.exit(ngx.OK)
    end

    local letters = {"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","y","z","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","Y","Z"}
    local numbers = {1,2,3,4,5,6,7,8,9,0,10,11,12,12,13,14,15,16,17,18,19}
    local symbols = {"!","@","#","$","%","^","&","*","(",")","/"}
    
    local pwd = ""
    local ttl = tonumber(ngx.req.get_uri_args().ttl)
    
    for i = 1, 254, 1 do
        if (i % 3 == 0) then
            pwd = pwd .. letters[math.random(1,#letters)]
        end
        if (i % 3 == 1) then
            pwd = pwd .. numbers[math.random(1,#numbers)]
        end
        if (i % 3 == 2) then
            pwd = pwd .. symbols[math.random(1,#symbols)]
        end
    end
    
    local keyid = ""
    
    for i = 1, 20, 1 do
        if (i % 2 == 0) then
            keyid = keyid .. letters[math.random(1,#letters)]
        end
        if (i % 2 == 1) then
            keyid = keyid .. numbers[math.random(1,#numbers)]
        end
    end

    local rulePath = string.gsub("/"..ngx.req.get_uri_args().name.."/"..ngx.req.get_uri_args().path,"//","/")
    
    local succ, err, forcible = ngx.shared.dskeys:add( keyid..":"..rulePath..":"..ngx.req.get_uri_args().method, pwd,ttl)
    
    if not succ then
        ngx.say("config set error:" .. err)
        ngx.exit(ngx.HTTP_INTERNAL_SERVER_ERROR )
    end

    ngx.log(ngx.ERR,keyid..":"..rulePath..":"..ngx.req.get_uri_args().method)
    
    ngx.say(cjson.encode({ keyid = keyid, password = pwd, ttl = ttl }))
    if forcible then
        ngx.exit(ngx.HTTP_INSUFFICIENT_STORAGE)
    else
        ngx.exit(ngx.HTTP_CREATED )
    end    
end

function Split(pString, pPattern)
    local Table = {}  -- NOTE: use {n = 0} in Lua-5.0
    local fpat = "(.-)" .. pPattern
    local last_end = 1
    local s, e, cap = pString:find(fpat, 1)
    while s do
       if s ~= 1 or cap ~= "" then
      table.insert(Table,cap)
       end
       last_end = e+1
       s, e, cap = pString:find(fpat, last_end)
    end
    if last_end <= #pString then
       cap = pString:sub(last_end)
       table.insert(Table, cap)
    end
    return Table
 end

function GetKey()
    local keyCandidates = {}
    local wildcards = {}
    for key, value in pairs(ngx.shared.dskeys:get_keys()) do
        local key = Split(value,":")
        if string.match(ngx.req.get_uri_args().path, key[2]) and (ngx.req.get_uri_args().method == key[3]) then
            if (key[2]==".*") then
                wildcards[#wildcards+1] = value
            else
                keyCandidates[#keyCandidates+1] = value
            end
        end
    end

    if ((#wildcards + #keyCandidates) == 0) then
        ngx.status = 500
        ngx.say("No keys available")
        ngx.exit(ngx.OK)
    end

    local chosen
    if (#keyCandidates > 0) then
        chosen = keyCandidates[math.random(1,#keyCandidates)]
    else
        chosen = wildcards[math.random(1,#wildcards)]
    end
    ngx.say(cjson.encode({keyid= string.gsub(chosen,":.*","") ,password = ngx.shared.dskeys:get(chosen) }))
    ngx.exit(ngx.HTTP_OK)
end

if ngx.req.get_method() == "POST" then
    BakeKey()
end

if ngx.req.get_method() == "GET" then
    GetKey()
end

ngx.say("This is just wrong")
ngx.exit(ngx.HTTP_INTERNAL_SERVER_ERROR)