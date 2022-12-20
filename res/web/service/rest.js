function route(req,response,events,finder) {
    let route = undefined;
    if (req.url.endsWith("/devices") && (req.method === "GET")) {
        route = finder.findHosts();
    } else if (req.url.endsWith("/device/secure") && (req.method === "POST")) {
        route = require("./configurator").secureClients(req,finder,events);
    }
    
    if (route !== undefined) {
        route.then(res=>respond(res,200))
             .catch(err=>respond({message:err.message,stack:err.stack},500))
             .finally(()=>response.end());
    } else {
        respond({message:"Invalid route"}, 404);
        response.end();
    }

    function respond(res,code) {
        response.writeHead(code, { "Content-Type": "application/json" });
        response.write(JSON.stringify(res));
    }
}

exports.serve = (events, finderService, port) => {
    require("http").createServer((req,response) => route(req,response,events,finderService))
                   .listen(port, ()=>console.log(`Server started on port ${port}.`));
};
