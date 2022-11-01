local resty_sha256 = require "resty.sha256"
local str = require "resty.string"
local sha256 = resty_sha256:new()

ngx.req.set_header("The-Hash-Time", os.time())
ngx.req.set_header("The-Hash-Id", math.random())

local headers, herr = ngx.req.get_headers()
local args, uerr = ngx.req.get_uri_args()
ngx.log(ngx.NOTICE, "method ", ngx.req.get_method(), " uri ", ngx.var.uri)
sha256:update("seretpassword")
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
