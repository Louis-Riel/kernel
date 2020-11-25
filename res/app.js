function dirname(path) {
    return (path.match(/.*\//) + "").replace(/(.+)\/$/g, "$1");
}

function addTableRow(tbody, path, file) {
    var tr = document.createElement("tr");
    var td = document.createElement("td");
    tr.classList.add(file.ftype);
    tr.appendChild(td);
    var link = document.createElement("a");
    if (file.ftype == "folder") {
        link.href = '#';
        link.onclick = (ctrl => {
            if (file.name != "..") {
                renderFolder(`${path!="/"?path+"/":"/"}${file.name}`);
            } else {
                renderFolder(path);
            }
        });
    } else {
        link.href = `${path}/${file.name}`;
    }
    link.textContent = file.name;
    td.appendChild(link);
    tr.appendChild(td);
    td = document.createElement("td");
    td.textContent = file.size;
    tr.appendChild(td);
    tbody.appendChild(tr);
}

function renderFolder(path) {
    var tbody = document.getElementsByClassName("file-table")[0].getElementsByTagName("tbody")[0];
    var caption = document.getElementsByClassName("file-table")[0].getElementsByTagName("caption")[0];
    fetch('/rest/sdcard' + path, {
        method: 'post'
    }).then(data => {
        data.json().then(jdata => {
            caption.textContent = path;
            tbody.innerHTML = "";
            if (path != "/") {
                addTableRow(tbody, dirname(path), {
                    name: "..",
                    ftype: "folder",
                    size: 0
                });
            }
            jdata.forEach(file => {
                if (file.ftype == "folder") {
                    addTableRow(tbody, path, file);
                }
            });
            var totlen = 0;
            jdata.forEach((file, idx, arr) => {
                if (file.ftype == "file") {
                    addTableRow(tbody, path, file);
                    totlen += file.size;
                }
                if ((idx + 1) == arr.length) {
                    var tfoot = document.getElementsByClassName("file-table")[0].getElementsByTagName("tfoot")[0];
                    tfoot.firstElementChild.children[1].textContent = totlen;
                }
            });
        });
    });
}

function refreshStatus() {
    fetch('/rest/status/system', {
        method: 'post'
    }).then(data => {
        data.json().then(jdata => {
            var csection = document.getElementById("wificonfig");
            while (csection.querySelector("fieldset")) {
                csection.removeChild(csection.querySelector("fieldset"));
            }

            document.getElementsByName("uptime")[0].value = jdata.uptime;
            document.getElementsByName("sleeptime")[0].value = jdata.sleeptime;
            document.getElementsByName("wifitype")[0].value = jdata.wifi.type;
            document.getElementsByName("ip")[0].value = jdata.wifi.ip;
            document.getElementsByName("freeram")[0].value = jdata.freeram;
            document.getElementsByName("ramusage")[0].value = (jdata.freeram / jdata.totalram) * 100.0;
            if (jdata.wifi.type == "AP") {
                fetch('/rest/status/wifi', {
                        method: 'post'
                    })
                    .then(data => {
                        data.json().then(jdata => {
                            jdata.clients.forEach(client => {
                                var hclient = document.createElement("fieldset");
                                csection.appendChild(hclient);
                                hclient.classList.add("wificlient");
                                var legend = document.createElement("legend");
                                legend.textContent = client.mac;
                                hclient.appendChild(legend);
                                Object.keys(client).forEach(fld => {
                                    if (fld != "mac") {
                                        var ctrl = document.createElement("label");
                                        ctrl.textContent = fld;
                                        var ictrl = document.createElement("input");
                                        ictrl.value = client[fld];
                                        ictrl.name = client.mac + fld;
                                        if (fld.match(".*ime$")) {
                                            ictrl.type = "timespan";
                                        }
                                        if (ictrl.value == "true") {
                                            ictrl.type = "checkbox";
                                            ictrl.checked = true;
                                        }
                                        if (ictrl.value == "false") {
                                            ictrl.type = "checkbox";
                                            ictrl.checked = false;
                                        }
                                        ctrl.appendChild(ictrl);
                                        hclient.appendChild(ctrl);
                                    }
                                });
                            });
                        });
                    }).catch(error => {
                        console.error(error);
                    });
            } else {
                csection.classList.add("hidden");
            }
        });
    }).catch(error => {
        console.error(error);
    });
}

function refreshConfig() {
    fetch('/rest/config', {
            method: 'post'
        })
        .then(data => {
            data.json().then(jdata => {
                document.getElementsByName("MisoPin")[0].value = jdata.sdcard.MisoPin;
                document.getElementsByName("MosiPin")[0].value = jdata.sdcard.MosiPin;
                document.getElementsByName("ClkPin")[0].value = jdata.sdcard.ClkPin;
                document.getElementsByName("CsPin")[0].value = jdata.sdcard.CsPin;
                document.getElementsByName("enPin")[0].value = jdata.gps.enPin;
                document.getElementsByName("rxPin")[0].value = jdata.gps.rxPin;
                document.getElementsByName("txPin")[0].value = jdata.gps.txPin;
                document.querySelector("fieldset.gps").classList.add(jdata.gps.state);
                document.querySelector("fieldset.sdcard").classList.add(jdata.sdcard.state);
                for (pos in jdata.gps.pois || []) {
                    document.getElementsByName(`syncpoints[${pos}].lat`)[0].value = jdata.gps.pois[pos].lat;
                    document.getElementsByName(`syncpoints[${pos}].lng`)[0].value = jdata.gps.pois[pos].lng;
                }
            });
        }).catch(error => {
            console.error(error);
        });
}

const getCellValue = (tr, idx) => tr.children[idx].innerText || tr.children[idx].textContent;

const comparer = (idx, asc) => (a, b) => ((v1, v2) =>
    v1 !== '' && v2 !== '' && !isNaN(v1) && !isNaN(v2) ? v1 - v2 : v1.toString().localeCompare(v2)
)(getCellValue(asc ? a : b, idx), getCellValue(asc ? b : a, idx));