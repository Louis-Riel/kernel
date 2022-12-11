const tls = require('tls');
const http = require('http');
const fs = require('fs');
const secureClientFileName = './cfg/configured-clients.json';
const secureClients = require(secureClientFileName);

let serverCert = undefined;

function getServerCert() {
    return new Promise((resolve,reject) => {
        if (serverCert) {
            resolve(tox509(serverCert));
        } else {
            const conn = tls.connect({
                host:process.env.REACT_APP_BACK_GATE,
                port:process.env.REACT_APP_BACK_GATE_PORT,
                rejectUnauthorized: false
            },()=>{
                serverCert = conn.getPeerCertificate();
                conn.destroy();
                resolve(tox509(serverCert));
            });
        }
        
        function tox509(cert) {
            return `-----BEGIN CERTIFICATE-----\n${chunk(cert.raw.toString('base64'), 64).join('\n')}\n-----END CERTIFICATE-----\n`;
        }

        function chunk(str, size) {
            return [].concat.apply([],
                str.split('').map((x,i)=>{ return i%size ? [] : str.slice(i,i+size) }, str)
            )
        }
    });
}

exports.secureClients = function secureClients(request,finder,events) {
    return new Promise((resolve,reject) => 
        getSecureConfig(request,finder).then(sendSecureConfig)
                                       .then(rebootDevice)
                                       .then(registerSecureDevice)
                                       .then(device => updateUpstreams(device,finder,events))
                                       .then(resolve)
                                       .catch(reject));
}

function registerSecureDevice(device) {
    return new Promise((resolve,reject) => {
        fs.writeFile(secureClientFileName,JSON.stringify([...secureClients,device]),err=>err?reject(err):resolve(device));
    });
}

function updateUpstreams(device,finder,events) {
    return new Promise((resolve,reject) => {
        fs.readFile(process.env.REACT_APP_UPSTREAM_PATH,(err,upstreams)=>err?reject(err):registerDevice(upstreams).then(resolve).catch(reject));
    });

    function registerDevice(upstreams) {
        return new Promise((resolve,reject) => {
            const newUpstreams = finder.goodDevices.map(device => `upstream ${device.config.devName.value} {
                server ${device.ip};\n}\n`).join('\n');
            if (upstreams.toString().localeCompare(newUpstreams) !== 0) {
                fs.writeFile(process.env.REACT_APP_UPSTREAM_PATH,newUpstreams,err=>err?reject(err):resolve(refreshSecures()));
            } else {
                console.warn(`Weird dummy update to upstreams`);
                resolve(refreshSecures());
            }
        });

        function refreshSecures() {
            events.emit("refreshSecureHosts","{}");
            return device;
        }
    }
}

function rebootDevice(device) {
    return new Promise((resolve,reject) => {
        const req = http.request(`http://${device.ip}/status/cmd`, {
            method: "put",
            headers: { "content-type": "application/json" }
        }).on("socket", socket => socket.writable ? req.end(JSON.stringify({command:"reboot"}), "utf8", () => { console.log("reboot sent"); resolve(device)}) : console.log("weirdness is afoot"))
          .on("error", err => { req.end(); reject(err); });
    });
}

function sendSecureConfig(device) {
    return new Promise((resolve,reject) => {
        const req = http.request(`http://${device.ip}/config/`, {
            method: "put",
            headers: { "content-type": "application/json" }
        }, (res) => {
            if (res.statusCode === 200) {
                console.log(`${device.ip} is configured`);
                resolve(device);
            } else {
                console.log(`${res.statusCode}:${res.read()}`);
                reject({ "message": `Failed HTTP${res.statusCode}` });
            }
        }).on("socket", socket => socket.writable ? req.end(JSON.stringify(device.config), "utf8", () => { console.log("config written"); }) : console.log("weirdness is afoot"))
            .on("error", err => { req.end(); reject(err); });
    });
}

function getSecureConfig(request,finder) {
    return new Promise((resolve, reject) => {
        let body = [];
        request.on('data', chunck => body.push(chunck))
               .on('end', () => {
            try {
                const device = JSON.parse(Buffer.concat(body).toString());
                if (device) {
                    finder.validateHost(device)
                          .then(config => {
                            config.Rest=config.Rest||{};
                            updateValue(config.Rest, "Access-Control-Allow-Origin" , `http://${process.env.REACT_APP_BACK_GATE}:${process.env.REACT_APP_BACK_GATE_PORT}`);
                            updateValue(config.Rest, "Access-Control-Allow-Methods" , `POST,PUT,OPTIONS,DELETE`);
                            getServerCert().then(cert => {
                                config.Rest.KeyServer = config.Rest.KeyServer || {};
                                updateValue(config.Rest.KeyServer,"keyServer",process.env.REACT_APP_BACK_GATE);
                                updateValue(config.Rest.KeyServer,"keyServerPath","/keys");
                                updateValue(config.Rest.KeyServer,"keyServerPort",1443);
                                updateValue(config.Rest.KeyServer,"serverCert",cert);
                                resolve({config:config,ip:device.ip});
                            }).catch(reject);

                            function updateValue(obj,fld,val) {
                                if (obj[fld] === undefined) {
                                    obj[fld] = {value:val,version:0};  
                                } else if (obj[fld]?.value !== val) {
                                    obj[fld].value = val;
                                    obj[fld].version++;
                                }
                            }
                          }).catch(reject);
                } else {
                    reject({"message":"Bad device parameter"});
                }
            } catch(ex) {
                reject(ex);
            }
        });
    });
}