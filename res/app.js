var controller = null;
var ws = null;
var fileStats = {};
var lastFolder = "";
var activeTab = null;

function dirname(path) {
    return (path.match(/.*\//) + "").replace(/(.+)\/$/g, "$1");
}

function elementInViewport(el) {
    var top = el.offsetTop;
    var left = el.offsetLeft;
    var width = el.offsetWidth;
    var height = el.offsetHeight;

    while (el.offsetParent) {
        el = el.offsetParent;
        top += el.offsetTop;
        left += el.offsetLeft;
    }

    return (
        top >= window.pageYOffset &&
        left >= window.pageXOffset &&
        (top + height) <= (window.pageYOffset + window.innerHeight) &&
        (left + width) <= (window.pageXOffset + window.innerWidth)
    );
}

function addTableRow(tbody, path, file, totCtrl) {
    var tr = document.createElement("tr");
    var td = document.createElement("td");
    tr.classList.add(file.ftype);
    tr.appendChild(td);
    var link = document.createElement("a");
    if (file.ftype == "folder") {
        link.href = '#';
        link.onclick = (ctrl => {
            if (file.name != "..") {
                renderFolder(`${path!="/"?file.folder+"/":"/"}${file.name}`);
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
    if (file.ftype == "file") {
        td.textContent = file.size;
    }
    tr.appendChild(td);
    tbody.appendChild(tr);
    if (file.ftype == 'file') { //&& elementInViewport(tr)) {
        if (fileStats[file.name]) {
            td = tr.children[1];
            td.innerText = fileStats[file.name];
            totCtrl.innerText = parseInt(totCtrl.innerText) + fileStats[file.name];
        } else {
            fetch(`/stat${file.folder}/${file.name}`, {
                method: 'post',
                signal: controller.signal
            }).then(data => {
                data.json().then(jdata => {
                    td = tr.children[1];
                    td.innerText = jdata.size;
                    fileStats[file.name] = jdata.size;
                    totCtrl.innerText = parseInt(totCtrl.innerText) + jdata.size;
                });
            }).catch(ex => {
                if (ex.code != 20)
                    console.error(ex);
            });
        }
    }
}

function SendCommand(body) {
    return fetch("/status/cmd", {
        method: 'put',
        body: JSON.stringify(body)
    });
}

function renderFolder(path) {
    if (controller)
        controller.abort();

    if (lastFolder != path) {
        fileStats = {};
    }
    lastFolder = path;
    controller = new AbortController();
    var tbody = document.getElementsByClassName("file-table")[0].getElementsByTagName("tbody")[0];
    activeTab = tbody;
    var caption = document.getElementsByClassName("file-table")[0].getElementsByTagName("caption")[0];
    var totctrl = document.getElementsByClassName("file-table")[0].getElementsByTagName("tfoot")[0].firstElementChild.children[1];
    totctrl.innerText = 0;
    fetch('/files' + path, {
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
                    addTableRow(tbody, path, file, totctrl);
                    totlen += file.size;
                }
            });
        });
    });
}

function isFloat(n) {
    return Number(n) === n && n % 1 !== 0;
}


function GetObject(data, path) {
    if (path == "/") {
        return data;
    }
    var curDir = path.substr(1, path.length - 1);
    var childObj;
    var idx = -1;
    if (curDir.indexOf("/") > 0) {
        curDir = curDir.substr(0, curDir.indexOf("/"));
    }

    if (curDir.indexOf("[") > 0) {
        idx = parseInt(curDir.replace(/.*\[([0-9]+).*/, "$1"));
        var arrayName = curDir.substr(0, curDir.indexOf("["));
        data[arrayName] = data[arrayName] || [];
        data[arrayName][idx] = data[arrayName][idx] || {}
        return data[arrayName][idx];
    } else {
        childObj = data[curDir] = data[curDir] || {};
        path = path.substr(curDir.length + 1);
    }

    if (path == "") {
        return childObj;
    }

    return GetObject(childObj, path);
}
//{"type":"AP","sdcard":{"state":"valid","MisoPin":"19","MosiPin":"23","ClkPin":"18","CsPin":"4"},"gps":{"state":"inactive","txPin":"0","rxPin":"0","enPin":"0"}}
function SaveForm(form) {
    fetch(form.action, {
        method: 'post',
        body: JSON.stringify(GetJsonFromInputs(form))
    }).then(data => {
        refreshConfig(true);
    });
}

function GetJsonFromInputs(form) {
    var res = {};
    form.querySelectorAll("input").forEach(el => {
        var path = dirname(el.name);
        var prop = el.name.substr(path.length + (path.length > 1 ? 1 : 0));
        var idx = parseInt(prop.indexOf("[") > 0 ? prop.replace(/.*\[([0-9]+)\].*/, "$1") : -1);
        var value = el.type == "checkbox" ? el.checked :
            el.type == "number" ? parseFloat(el.value) : el.value;
        if (idx >= 0) {
            path = `${path}/${prop.substr(0,prop.indexOf("]")+1)}`;
            prop = prop.substr(0, prop.indexOf("["));
            var subProp = el.name.substr(el.name.indexOf("]") + 1);
            GetObject(res, path)[subProp] = value;
        } else {
            GetObject(res, path)[prop] = value;
        }
    });
    return res;
}

function jsonifyField(fPath, curfs, fld, val) {
    var tp = val.toString().indexOf("\n").toString() > 0 ? "textarea" : "input";
    var cfld = Array.from(curfs.querySelectorAll("label")).find(cfld => {
        var input = cfld.querySelector(tp);
        if (input && (input.name == fPath)) {
            return cfld;
        }
        return undefined;
    }) || curfs.appendChild(document.createElement("label"));
    if (cfld.querySelectorAll("div").length == 0) {
        var label = document.createElement("div");
        label.innerText = fld;
        cfld.appendChild(label);
    }
    var input = cfld.querySelector(tp) || cfld.appendChild(document.createElement(tp));
    input.name = fPath;
    if (tp == "textarea") {
        val = val.split("\n").filter(ln => ln != "").sort().join("\n");
        input.cols = 40;
        input.rows = val.split("\n").length;
    }
    if (isFloat(val)) {
        if ((fld != "Lattitude") && (fld != "Longitude") && (fld != "lat") && (fld != "lng")) {
            val = parseFloat(val).toFixed(2);
            input.step = "0.01"
        } else {
            val = parseFloat(val).toFixed(8);
            input.step = "0.00000001"
        }
    }

    if ((val != true) && (val != false) && !isNaN(val) && (val !== "")) {
        input.type = "number";
    }

    if (fld.match(".*ime$")) {
        var dt = new Date(val);
        if (dt.valid)
            val = dt;
    }
    if ((val == "true") || (val == "yes") || (val === true)) {
        input.type = "checkbox";
        input.checked = true;
    }
    if ((val == "false") || (val == "no") || (val === false)) {
        input.type = "checkbox";
        input.checked = false;
    }
    input.value = val;
    cfld.classList.add(input.type);
}

function jsonifyFormFielset(curPath, curfs, curObject) {
    curfs.name = curPath;
    Object.keys(curObject).forEach(fld => {
        var fPath = `${curPath}${fld}`;
        if (Array.isArray(curObject[fld])) {
            var arrFs = curfs.querySelector(`label[id='${fPath}']`) || curfs.appendChild(document.createElement("label"));
            if (arrFs.querySelectorAll("div").length == 0) {
                var label = document.createElement("div");
                label.innerText = fld;
                arrFs.appendChild(label);
            }
            arrFs.id = fPath;
            curObject[fld].forEach((arit, idx) => {
                var childPath = `${curPath}${fld}[${idx}]`;
                var childFs = arrFs.querySelector(`fieldset[id='${childPath}']`) || arrFs.appendChild(document.createElement("fieldset"));
                childFs.id = childPath;
                (childFs.querySelector("legend") || childFs.appendChild(document.createElement("legend"))).innerText = idx;
                jsonifyFormFielset(childPath, childFs, arit);
            });
            Array.from(arrFs.querySelectorAll("fieldset")).forEach(fs => {
                if (!curObject[fld].some((ait, idx) => fs.id == `${curPath}${fld}[${idx}]`)) {
                    arrFs.removeChild(fs);
                }
            });
        } else if (typeof curObject[fld] == 'object') {
            if (Object.keys(curObject[fld]).some(elem => elem == "version")) {
                jsonifyField(fPath, curfs, fld, curObject[fld].value);
            } else {
                var childFs = curfs.querySelector(`fieldset[id='${fPath}']`) || curfs.appendChild(document.createElement("fieldset"));
                (childFs.querySelector("legend") || childFs.appendChild(document.createElement("legend"))).innerText = fld;
                childFs.id = fPath;
                jsonifyFormFielset(`${curPath}${fld}/`, childFs, curObject[fld]);
            }
        } else {
            jsonifyField(fPath, curfs, fld, curObject[fld]);
        }
    });
}

function refreshStatus(clicked) {
    var form = document.querySelectorAll(".system-config form")[0];
    if (clicked)
        activeTab = form;
    if (controller)
        controller.abort();
    controller = null;
    fetch('/status/', {
        method: 'post'
    }).then(data => {
        data.json().then(status => {
            jsonifyFormFielset(
                "/",
                Array.from(form.querySelectorAll("fieldset")).find(fs => fs.id == "") || form.appendChild(document.createElement("fieldset")),
                status);
        });
    });
}

function refreshConfig(clicked) {
    var form = document.querySelectorAll(".system-config form")[1];
    if (clicked)
        activeTab = form;
    if (controller)
        controller.abort();
    controller = null;
    fetch('/config', {
        method: 'post'
    }).then(data => {
        data.json().then(config => {
            var saveBtn = form.querySelector("button");
            jsonifyFormFielset(
                "/",
                Array.from(form.querySelectorAll("fieldset")).find(fs => fs.id == "") || form.insertBefore(document.createElement("fieldset"), saveBtn),
                config);
        });
    });
}

function refreshSystem() {
    var logs = document.getElementById("slide-4").getElementsByClassName("loglines")[0];
    activeTab = logs;
    if (controller)
        controller.abort();
    controller = null;

    if (ws == null) {
        ws = new WebSocket("ws://" + window.location.hostname + "/ws");
        var lmsg;
        var log;
        ws.onmessage = function(event) {
            if ((event.data == null) || (event.data.length == 0)) {
                return;
            }
            var data = event.data.replaceAll(/^[^IDVWE]*/g, "").replaceAll(/(.*)([\x1B].*\n?)/g, "$1");

            if (data.match(/^[IDVWE] \([0-9.:]{12}\) [^:]+:.*/)) {
                log = document.createElement("div");
                log.classList.add("log");
                log.classList.add(`LOG${data.substr(0,1)}`);

                var llevel = document.createElement("div");
                llevel.classList.add(`LOGLEVEL`);
                llevel.innerText = data.substr(0, 1);
                log.appendChild(llevel);

                var ldate = document.createElement("div");
                ldate.classList.add(`LOGDATE`)
                ldate.innerText = data.substr(3, 12);
                log.appendChild(ldate);

                var lfunc = document.createElement("div");
                lfunc.classList.add(`LOGFUNCTION`)
                lfunc.innerText = data.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");
                log.appendChild(lfunc);

                lmsg = document.createElement("div");
                lmsg.classList.add(`LOGLINE`)
                lmsg.innerText = data.substr(data.indexOf(lfunc.innerText) + lfunc.innerText.length + 2).replaceAll(/^[\r\n]*/g, "");
                log.appendChild(lmsg);

                logs.appendChild(log);
            } else {
                lmsg.innerText += `\n${data}`;
            }
            if (activeTab == logs)
                lmsg.scrollIntoView();
        }
        ws.onopen = () => { ws.send("Logs") };
        ws.onerror = (err) => { ws = null };
    }
}

const getCellValue = (tr, idx) => tr.children[idx].innerText || tr.children[idx].textContent;

const comparer = (idx, asc) => (a, b) => ((v1, v2) =>
    v1 !== '' && v2 !== '' && !isNaN(v1) && !isNaN(v2) ? v1 - v2 : v1.toString().localeCompare(v2)
)(getCellValue(asc ? a : b, idx), getCellValue(asc ? b : a, idx));