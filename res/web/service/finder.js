const nmap = require('node-nmap');
const http = require('http');
nmap.nmapLocation = 'C:\\Program Files (x86)\\Nmap\\nmap.exe';

exports.Finder = class Finder {
    goodDevices = [];
    badDevices = [];
    events;
    netMask = "";
    scanning = false;
    
    constructor(events,mask) {
        this.events = events;
        this.netMask = mask;
        events.on("command",this.processCommand.bind(this));
        events.on("clientConnected",client => client.send(JSON.stringify({clients:this.goodDevices})));
    }

    processCommand(message) {
        switch (message.command) {
            case "scan":
                this.findHosts();
                break;
            default:
                console.error("Bad command",message);
                break;
        }
    }

    isArray(a) {
        return (!!a) && (a.constructor === Array);
    }

    isObject(a) {
        return (!!a) && (a.constructor === Object);
    }

    fromVersionedToPlain(obj) {
        let ret = this.isObject(obj) ? {} : [];
        if (this.isObject(obj)){
            Object.entries(obj).forEach(fld => {
                if (this.isObject(fld[1])) {
                    ret[fld[0]] = (fld[1].value !== undefined) && (fld[1].version !== undefined) ? fld[1].value 
                                                                                                 : this.fromVersionedToPlain(fld[1]);
                } else if (this.isArray(fld[1])) {
                    ret[fld[0]] = fld[1].map(this.fromVersionedToPlain.bind(this));
                } else {
                    ret[fld[0]] = fld[1];
                }
            })
        }
        return ret;
    }

    validateHost(host) {
        return new Promise((resolve,reject) => {
            const ip = host.ip;
            let abort = new AbortController();
            let timer = setTimeout(()=>abort.abort(),400000);
            http.request(`http://${ip}/config`,{
                method:"POST",
                signal: abort.signal
            },res => {
                let data = [];
                res.on('data',chunck=>data=[...data,...chunck])
                   .on('end', () => {
                    try {
                        let config = this.fromVersionedToPlain(JSON.parse(Buffer.from(data).toString()));
                        if ((config.deviceid !== undefined) && !this.goodDevices.some(client=>client.config.deviceid===config.deviceid)) {
                            this.goodDevices.push({config:config,ip:ip});
                        }
                        resolve(config);
                    } catch (err) {
                        this.badDevices.push(ip);
                        reject(err);
                    }
                });
            }).on("error",err => {
                this.badDevices.push(ip);
                reject(err);
            }).on("close", () => clearTimeout(timer))
              .end();
        });
    }
    
    findHosts() {
        if (this.scanning) {
            return Promise.resolve(this.goodDevices);
        }
        this.scanning = true;
        return new Promise((resolve,reject) => {
            console.log(`Scanning for hosts`);
            new nmap.NmapScan(this.netMask ,"-Pn -p80")
                    .on("complete",hosts=>Promise.allSettled(hosts.filter(host => !this.badDevices.includes(host.ip))
                                                                  .filter(host => host.openPorts && host.openPorts.find(port => port.port === 80))
                                                                  .map(host => this.validateHost(host)))
                                                                  .then(_res=>resolve(this.goodDevices))
                                                                  .catch(err=>reject(err))
                                                                  .finally(() => {
                                                                    this.events.emit("clients",{clients:this.goodDevices});
                                                                    this.scanning = false;
                                                                }))
                    .on("error",reject)
                    .startScan();
        })
    }
}