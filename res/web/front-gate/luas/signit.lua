local resty_sha256 = require "resty.sha256"
local str = require "resty.string"
local http = require "resty.http"
local cjson = require "cjson"

local sha256 = resty_sha256:new()
local httpc = http.new()

ngx.req.set_header("The-Hash-Time", os.time())
ngx.req.set_header("The-Hash-Id", math.random())

local headers, herr = ngx.req.get_headers()
local args, uerr = ngx.req.get_uri_args()
ngx.log(ngx.NOTICE, "method ", ngx.req.get_method(), " uri ", ngx.var.request_uri)

local res, err = httpc:request_uri("http://gate-keeper:1234/keys", { method = "GET", query = {["path"] = ngx.var.request_uri} })
if not res or err then
    ngx.log(ngx.ERR, "request failed: ", err, ngx.var.request_uri)
else
    if (res.status ~= 200) then
        ngx.log(ngx.ERR, "request bad: ", res.body)
    else 
        local data = cjson.decode(res.body)
        ngx.req.set_header("The-Hash-Key", data.keyid)
        sha256:update(data.password)
    end
end

sha256:update(ngx.req.get_method())
sha256:update(ngx.var.uri)

for k, v in pairs(headers) do
    sha256:update(k)
    sha256:update(v)
end

for k, v in pairs(args) do
    sha256:update(k)
    sha256:update(v)
end

ngx.req.set_header("The-Hash", str.to_hex(sha256:final()))
