function route(req,response,events,finder) {
    console.log(req.url);
    let route = undefined;
    if ((req.url === "/devices") && (req.method === "GET")) {
        route = finder.findHosts();
    } else if ((req.url === "/device/secure") && (req.method === "POST")) {
        route = require("./configurator").secureClients(req,finder,events);
    }
    
    if (route !== undefined) {
        route.then(res=>respond(res,200))
             .catch(err=>respond({message:err.message,stack:err.stack},500))
             .finally(()=>response.end());
    } else {
        respond(404,{ message: "Route not found" });
    }

    function respond(res,code) {
        response.writeHead(code, { "Content-Type": "application/json" });
        response.write(JSON.stringify(res));
    }
}

exports.serve = (events, finderService, port) => {
    require("http").createServer((req,response) => route(req,response,events,finderService))
                   .listen(port, ()=>console.log(`Server started on port ${port}`));
};
