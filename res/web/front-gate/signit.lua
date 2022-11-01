local cjson = require "cjson"
local httpc = require("resty.http").new()
local str = require "resty.string"
local resty_sha256 = require "resty.sha256"

function GetRandomString ()
    local letters = {"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","y","z","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","Y","Z"}
    local numbers = {1,2,3,4,5,6,7,8,9,0,10,11,12,12,13,14,15,16,17,18,19}
    local symbols = {"!","@","#","$","%","^","&","*","(",")","'","/","{","}"}
    
    local pwd = ""
    
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
    return pwd
end

function GetRequestSha(password)
    ngx.req.read_body()
    local sha256 = resty_sha256:new()
    sha256:update(password)
    sha256:update(ngx.var.uri)
    sha256:update(ngx.req.get_method())
    for key, value in pairs(ngx.resp.get_headers()) do
        sha256:update(key .. ":" .. value)
    end
    for key, value in pairs(ngx.resp.get_headers()) do
        sha256:update(key .. ":" .. value)
    end

    -- local body = ngx.req.get_body_data()
    -- if (body) then
    --     sha256:update(ngx.req.get_body_data())
    -- end
    -- ngx.req.discard_body()

    return str.to_hex(sha256:final())    
end

local res = httpc:request_uri("http://gate-keeper:1234/key?path=" .. ngx.var.uri, {
    method = "GET"
})

if res.status == 200 then
    local pwd = cjson.decode(res.body)
    ngx.req.set_header('The-Key-Id', pwd.keyid)
    ngx.req.set_header('The-Hash', GetRequestSha(pwd.password))
end