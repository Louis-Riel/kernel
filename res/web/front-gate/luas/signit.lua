local resty_sha256 = require "resty.sha256"
local str = require "resty.string"
local http = require "resty.http"
local cjson = require "cjson"

local sha256 = resty_sha256:new()
local httpc = http.new()

local function sortIt (t, f)
    local a = {}
    for n in pairs(t) do table.insert(a, n) end
    table.sort(a, f)
    local i = 0
    local iter = function ()
        i = i + 1
        if a[i] == nil then return nil
        else return a[i], t[a[i]]
        end
    end
    return iter
end

ngx.log(ngx.ERR, "method ", ngx.req.get_method(), " uri ", ngx.var.request_uri)

local res, err = httpc:request_uri("http://gate-keeper:1234/keys", { query = {["path"] = ngx.var.request_uri, ["method"] = ngx.req.get_method() }})
if not res or err then
    ngx.log(ngx.ERR, "request failed. path:"..ngx.var.request_uri.." method:"..ngx.req.get_method().."...", err)
else
    if (res.status ~= 200) then
        ngx.log(ngx.ERR, "request bad: ", res.body)
    else 
        local data = cjson.decode(res.body)

        ngx.req.set_header("TheHashTime", os.time())
        ngx.req.set_header("TheHashRandom", math.random())
        ngx.req.set_header("TheHashKey", data.keyid)

        local headers, herr = ngx.req.get_headers()

        sha256:update(ngx.req.get_method())

        sha256:update(ngx.var.uri)
        sha256:update(headers.TheHashTime)
        sha256:update(headers.TheHashRandom)
        sha256:update(headers.TheHashKey)
        sha256:update(data.password)
        
        ngx.req.set_header("TheHash", str.to_hex(sha256:final()))
    end
end
