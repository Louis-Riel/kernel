let finder;

function route(req,response) {
    console.log(req.url);
    if ((req.url === "/devices") && (req.method === "GET")) {
        finder.findHosts().then(res => {
            response.writeHead(200, { "Content-Type": "application/json" });
            response.write(JSON.stringify(res));
        }).catch(err => {
            response.writeHead(500, { "Content-Type": "application/json" });
            response.write(JSON.stringify(err))
        }).finally(()=>response.end());
    } else {
        response.writeHead(404, { "Content-Type": "application/json" });
        response.end(JSON.stringify({ message: "Route not found" }));
    }
}

exports.serve = (finderService, port) => {
    finder = finderService;
    require("http").createServer(route).listen(port, ()=>console.log(`Server started on port ${port}`))
};
