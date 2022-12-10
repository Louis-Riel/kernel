let events;
const WebSocket = require('ws');

let clients = [];

function processMessage(client,buffer) {
    try {
        events.emit("command",JSON.parse(buffer.toString()));
    } catch (ex) {
        client.send(JSON.stringify({error:ex.toString()}));
    }
}

exports.serve = (eventHost, port) => {
    events = eventHost;
    events.on("clients",esps => clients.forEach(client => client.send(JSON.stringify(esps))));
    new WebSocket.Server({port:port})
                 .on('connection',client => {
                    events.emit("clientConnected",client);
                    clients.push(client);
                    client.on("message",(msg) => processMessage(client, msg))
                    client.on("close",() => clients.splice(clients.findIndex(cl=>cl === client)))
                }).on("listening",_evt=>{
                    events.emit("command",{command:"scan"});
                    console.log(`Web socket running on port ${port}`)
                });
};
