const nmap = require('node-nmap');
const http = require('http');
const https = require('https');
const fs = require('fs');
const {fromVersionedToPlain} = require('./utils');
nmap.nmapLocation = process.env.NMAP;

exports.Finder = class Finder {
    goodDevices = [];
    events;
    netMask = "";
    scanning = false;
    secureClients = [];
    
    constructor(events,mask) {
        this.events = events;
        this.netMask = mask;
        events.on("command",this.processCommand.bind(this));
        events.on("refreshSecureHosts",this.refreshSecureHosts.bind(this));
        setInterval(this.findHosts.bind(this),10000);
        this.refreshSecureHosts();
    }

    refreshSecureHosts() {
        fs.readFile('./cfg/configured-clients.json',(err,data)=>{
            if (err) {
                console.error(err);
            } else {
                this.secureClients=JSON.parse(Buffer.from(data).toString());
                Promise.allSettled(this.secureClients.map(this.validateHost.bind(this)));
            }
        });
    }

    processCommand(message) {
        switch (message.command) {
            case "scan":
                this.findHosts();
                break;
            case "refreshSecureHosts":
                this.findHosts();
                break;
            default:
                console.error("Bad command",message);
                break;
        }
    }

    getConfigUrl(host) {
        if (this.isSecure(host)) {
            return `https://${process.env.REACT_APP_FRONT_GATE}:${process.env.REACT_APP_FRONT_GATE_PORT}/${host.config.devName.value}/config/`;
        } else {
            return `http://${host.ip}/config`;
        }
    }

    isSecure(host) {
        return this.secureClients.some(client => client.ip === host.ip);
    }

    validateHost(host) {
        if (host === undefined) {
            return null;
        }
        return new Promise((resolve,reject) => {
            let abort = new AbortController();
            let timer = setTimeout(()=>abort.abort(),400000);
            let provider = this.isSecure(host) ? https: http;
            // console.log(this.getConfigUrl(host));
            provider.request(this.getConfigUrl(host),{method:"POST",signal: abort.signal},
                  (res) => this.processDeviceConfig(res, host)
                               .then(resolve)
                               .catch(reject))
                    .on("error",reject)
                    .on("close", () => clearTimeout(timer))
                    .end();
        });
    }
    
    processDeviceConfig(res, host) {
        return new Promise((resolve,reject) => {
            let data = [];
            res.on('data', chunck => data = [...data, ...chunck])
               .on('error', err => { console.error(err); reject(err); })
               .on('end', () => {
                    try {
                        let config = JSON.parse(Buffer.from(data).toString());
                        if (config.deviceid !== undefined) {
                            const client = this.goodDevices.find(client => client.config.deviceid.value === config.deviceid.value);
                            if (client === undefined) {
                                this.goodDevices.push({ config: config, ip: host.ip });
                                console.log(`Found ${this.isSecure(host)?"secure":"insecure"} device at ${host.ip}`);
                            } else {
                                client.config = config;
                            }
                            resolve(config);
                        } else {
                            reject(Buffer.from(data).toString());
                        }
                    } catch (err) {
                        reject(err);
                    }
                });
            });
    }

    findHosts() {
        if (!this.scanning) {
            this.scanning = true;
            new nmap.NmapScan(this.netMask ,"-p80")
                    .on("complete",hosts=>Promise.allSettled(hosts.filter(host => host.openPorts && host.openPorts.find(service => service.port === 80))
                                                                    .map(this.validateHost.bind(this)))
                                                                    .catch(console.error)
                                                                    .finally(() => {
                                                                        this.events.emit("clients",{clients:this.goodDevices.map(fromVersionedToPlain)});
                                                                        this.scanning = false;
                                                                    }))
                    .startScan();
        }
    }
}