var controller = null;
var logWs = null;
var stateWs = null;
var fileStats = {};
var fileStatsToFetch = [];
var lastFolder = "";
var activeTab = null;

//#region utility functions
function dirname(path) {
    return (path.match(/.*\//) + "").replace(/(.+)\/$/g, "$1");
}

function isFloat(n) {
    return Number(n) === n && n % 1 !== 0;
}

function IsDatetimeValue(fld) {
    return fld.match(".*ime$");
}

function IsBooleanValue(val) {
    return (val == "true") ||
        (val == "yes") ||
        (val === true) ||
        (val == "false") ||
        (val == "no") ||
        (val === false);
}

function IsNumberValue(val) {
    return ((val !== true) && (val !== false) && (typeof(val) != "string") && !isNaN(val) && (val !== ""));
}

//#endregion

//#region file browser
function addTableRow(tbody, path, file, totCtrl) {
    var tr = document.createElement("tr");
    var td = document.createElement("td");
    tr.classList.add(file.ftype);
    tr.appendChild(td);
    td.appendChild(GetFileLink(file, path));
    td = document.createElement("td");
    if (file.ftype == "file") {
        td.textContent = file.size;
    }
    tr.appendChild(td);
    tbody.appendChild(tr);
    if (file.ftype == 'file') {
        if (fileStats[file.name]) {
            td = tr.children[1];
            td.innerText = fileStats[file.name];
            totCtrl.innerText = parseInt(totCtrl.innerText) + fileStats[file.name];
        } else {
            td = tr.children[1];
            return { "file": file, "td": td, "totctrl": totCtrl };
        }
    }
}

function GetFileLink(file, path) {
    var link = document.createElement("a");
    if (file.ftype == "folder") {
        link.href = '#';
        link.onclick = (ctrl => {
            if (file.name != "..") {
                renderFolder(`${path != "/" ? file.folder + "/" : "/"}${file.name}`);
            } else {
                renderFolder(path);
            }
        });
    } else {
        link.href = `${path}/${file.name}`;
    }
    link.textContent = file.name;
    return link;
}

function renderFolder(path) {
    if (controller)
        controller.abort();

    if (lastFolder != path) {
        fileStats = {};
    }
    lastFolder = path;
    fileStatsToFetch = [];
    controller = new AbortController();
    var tbody = document.getElementsByClassName("file-table")[0].getElementsByTagName("tbody")[0];
    activeTab = tbody;
    var caption = document.getElementsByClassName("file-table")[0].getElementsByTagName("caption")[0];
    var totctrl = document.getElementsByClassName("file-table")[0].getElementsByTagName("tfoot")[0].firstElementChild.children[1];
    totctrl.innerText = 0;
    fetch('/files' + path, {
        method: 'post'
    }).then(data => {
        data.json().then(jdata => RenderFolderItem(caption, path, tbody, jdata, totctrl));
    });
}

function RenderFolderItem(caption, path, tbody, jdata, totctrl) {
    caption.textContent = path;
    tbody.innerHTML = "";
    if (path != "/") {
        addTableRow(tbody, dirname(path), {
            name: "..",
            ftype: "folder",
            size: 0
        });
    }
    var totlen = 0;
    jdata.sort((f1, f2) => {
        if (f1.ftype != f2.ftype) {
            return f1.ftype == "folder" ? 1 : -1;
        }
        return f1.name == f2.name ? 0 : f1.name > f2.name ? 1 : -1;
    }).forEach(file => {
        if (file.ftype == "folder") {
            addTableRow(tbody, path, file);
        }
        if (file.ftype == "file") {
            var toFetch = addTableRow(tbody, path, file, totctrl);
            if (toFetch) {
                fileStatsToFetch.push(toFetch);
            }
            totlen += file.size;
        }
    });
    for (var idx = 0; idx < Math.min(5, fileStatsToFetch.length); idx++) {
        if (fileStatsToFetch.length) {
            GetFileStat(fileStatsToFetch.pop(), totctrl);
        }
    }
}

function GetFileStat(fileToFetch, totctrl) {
    fetch(`/stat${fileToFetch.file.folder}/${fileToFetch.file.name}`, {
        method: 'post',
        signal: controller.signal
    }).then(data => {
        data.json().then(jdata => {
            fileToFetch.td.innerText = jdata.size;
            fileStats[fileToFetch.file.name] = jdata.size;
            fileToFetch.totctrl.innerText = parseInt(totctrl.innerText) + jdata.size;
        });
        if (fileStatsToFetch.length) {
            GetFileStat(fileStatsToFetch.pop(), totctrl);
        }
    }).catch(ex => {
        if (ex.code != 20)
            console.error(ex);
    });
}

//#endregion

//#region JSON Parsing
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
        var subProp = path.substr(path.indexOf("]") + 2);
        return data[arrayName][idx][subProp] = data[arrayName][idx][subProp] || {};
    } else {
        childObj = data[curDir] = data[curDir] || {};
        path = path.substr(curDir.length + 1);
    }

    if (path == "") {
        return childObj;
    }

    return GetObject(childObj, path);
}
//#endregion

//#region form extract
function GetJsonFromInputs(form) {
    var res = {};
    form.querySelectorAll("input").forEach(el => {
        var value = el.type == "checkbox" ? el.checked :
            el.type == "number" ? parseFloat(el.value) : el.value;
        GetObject(res, el.name).value = value;
        GetObject(res, el.name).version = el.version;
    });
    return res;
}
//#endregion

//#region EventManager
function formEvent(container, event, key) {
    var hevent = container.querySelector(`details[id=event${key}`) || container.appendChild(document.createElement("details"));
    hevent.id = `event${key}`;
    hevent.data = event;
    var summary = hevent.querySelector("summary") || hevent.appendChild(document.createElement("summary"));
    AddTriggerField(summary, null, "eventBase", event, "label");
    AddTriggerField(summary, null, "eventId", event, "label");

    BuildTriggerSection(hevent, event);
    BuildRunSection(hevent, event);
}

function BuildRunSection(hevent, event) {
    var hrun = hevent.querySelector("details.run") || hevent.appendChild(document.createElement("details"));
    hrun.classList.add("run");
    var summary = hrun.querySelector("summary") || hrun.appendChild(document.createElement("summary"));
    AddTriggerField(summary, null, "method", event);

    if (event.params) {
        BuildRunParamsSection(hrun, event.params);
    }
}

function BuildRunParamsSection(parent, params) {
    Object.keys(params).forEach(key => AddTriggerField(parent, key, key, params));
}

function BuildTriggerSection(hevent, event) {
    var triggerDiv = hevent.querySelector(`div.trigger`) || hevent.appendChild(document.createElement("div"));
    triggerDiv.classList.add("trigger");
    AddTriggerField(triggerDiv, "Event Base", "eventBase", event);
    AddTriggerField(triggerDiv, "Event Id", "eventId", event);
}

function AddTriggerField(triggerDiv, labelName, fld, value, tp = "input") {
    var baseLabel = triggerDiv;
    if (labelName) {
        baseLabel = triggerDiv.querySelector(`label.${fld}`) || triggerDiv.appendChild(document.createElement("label"));
        baseLabel.classList.add(fld);
        baseLabel.classList.add("label");
        baseLabel.textContent = labelName;
    }
    var baseInput = baseLabel.querySelector(`${tp}.${fld}`) || baseLabel.appendChild(document.createElement(tp));
    baseInput.classList.add(fld);
    baseInput.data = value[fld] = {
        "value": baseInput.value = value[fld].value === undefined ? value[fld] : value[fld].value,
        "version": baseInput.version = value[fld].version || 0
    };
    baseInput.onchange = () => { value[fld] = isNaN(baseInput.value) ? baseInput.value : parseInt(baseInput.value) };
    if (tp != "input") {
        baseInput.innerText = value[fld].value;
    }
}

function formEvents(curfs, fPath, config) {
    var childFs = curfs.querySelector(`fieldset[id='${fPath}']`) || curfs.appendChild(document.createElement("fieldset"));
    (childFs.querySelector("legend") || childFs.appendChild(document.createElement("legend"))).innerText = "Events";
    childFs.id = fPath;
    childFs.data = config;
    var mainDiv = childFs.querySelector(`.events`) || childFs.appendChild(document.createElement("div"));
    mainDiv.classList.add("events");
    config.forEach((event, idx) => formEvent(mainDiv, event, idx));
}

//#endregion

//#region form populate

function jsonifyField(fPath, parentElement, fld, val) {
    var isEditable = val !== undefined && val["version"] !== undefined;
    var tp = isEditable ? val.value.toString().indexOf("\n").toString() > 0 ? "textarea" : "input" : "div";
    var fieldContainer = GetFieldContainer(parentElement, tp, fPath, isEditable);
    fieldContainer.classList.add(isEditable ? "editable" : "readonly");
    if (parentElement.nodeName != "TD")
        BuildFieldLabel(fieldContainer, fld);
    GetFieldInput(fieldContainer, tp, fPath, fld, val);
}

function GetFieldInput(fieldContainer, tp, fPath, fld, value) {
    if (value === undefined)
        return;
    var val = value.value === undefined ? value : value.value;
    var input = fieldContainer.querySelector(`${tp}.value`) || fieldContainer.appendChild(document.createElement(tp));
    input.name = fPath;
    if (tp == "textarea") {
        input.cols = 40;
        input.rows = val.split("\n").filter(ln => ln != "").sort().join("\n");;
    }
    if (IsDatetimeValue(fld)) {
        val = HandleDatetimeValue(fld, input, val);
        input.type = "date";
    } else if (IsBooleanValue(val)) {
        val = HandleBooleanValue(val, input, fld);
        input.type = "checkbox";
        input.checked = val;
    } else if (IsNumberValue(val)) {
        input.type = "number";
        val = HandleNumberValue(val, input, fld);
    } else {
        input.type = "text"
    }

    if (tp == "div") {
        input.innerText = val;
    } else {
        input.value = val;
    }

    input.name = fPath;
    input.classList.add("value");
    input.version = value.version;

    input.data = value;
    input.onchange = () => { value.value = isNaN(input.value) ? input.value : parseInt(input.value) };


    return input;
}

function BuildFieldLabel(fieldContainer, fld) {
    var label = Array.from(fieldContainer.querySelectorAll("div.label")).find(div => div.innerText == fld) || fieldContainer.appendChild(document.createElement("div"));
    label.innerText = fld
    label.classList.add("label");
    return label;
}

function GetFieldContainer(parentElement, tp, fPath, isEditable) {
    if (parentElement.nodeName == "TD") {
        return parentElement;
    }
    var fc = Array.from(parentElement.querySelectorAll("label")).find(cfld =>
        Array.from(cfld.querySelectorAll(`${tp}.value`)).find(input => input.name == fPath)
    ) || parentElement.appendChild(document.createElement("label"));
    return fc;
}

function jsonifyFormFielset(curPath, curfs, curObject) {
    curfs.name = curPath;
    curfs.data = curObject;
    if (curfs.querySelector("div.controls") === null) {
        var headerDiv = curfs.appendChild(document.createElement("div"));
        headerDiv.classList.add("controls");
    }
    Object.keys(curObject).forEach(fld => {
        var fPath = `${curPath}${fld}`;
        if (fld == "events") {
            formEvents(curfs, fPath, curObject[fld]);
        } else if (Array.isArray(curObject[fld])) {
            jsonifyArray(curfs, fPath, fld, curObject[fld], curPath);
        } else if (typeof curObject[fld] == 'object') {
            jsonifyObject(curObject[fld], fld, fPath, curfs, curPath);
        } else {
            jsonifyField(fPath, curfs, fld, curObject[fld]);
        }
    });
}

function jsonifyObject(curObject, fld, fPath, curfs, curPath) {
    if (Object.keys(curObject).some(elem => elem == "version")) {
        jsonifyField(fPath, curfs, fld, curObject);
    } else {
        var childFs = curfs.querySelector(`fieldset[id='${fPath}']`) || curfs.appendChild(document.createElement("fieldset"));
        (childFs.querySelector("legend") || childFs.appendChild(document.createElement("legend"))).innerText = fld;
        childFs.id = fPath;
        jsonifyFormFielset(`${curPath}${fld}/`, childFs, curObject);
    }
}

function jsonifyArray(curfs, fPath, fld, curArray, curPath) {;
    var arrFs = curfs.querySelector(`label[id='${fPath}']`) || curfs.appendChild(document.createElement("label"));
    arrFs.id = fPath;
    arrFs.classList.add("table");
    if (curArray.length > 0) {
        var table = arrFs.querySelector("table") || arrFs.appendChild(document.createElement("table"));
        table.data = curArray;
        table.classList.add("greyGridTable");
        var thead = table.querySelector("thead") || table.appendChild(document.createElement("thead"));
        var tableHeader = table.querySelector("caption") || table.appendChild(document.createElement("caption"));
        if (tableHeader.querySelector("div.controls") === null) {
            tableHeader.innerText = fld;
            tableHeader.appendChild(document.createElement("div")).classList.add("controls");
        }

        var tbody = table.querySelector("tbody") || table.appendChild(document.createElement("tbody"));
        var headrow = thead.querySelector("tr") || thead.appendChild(document.createElement("tr"));
        var cols = curArray.filter(obj => obj != null).map(obj => Object.keys(obj))
            .reduce((c1, c2) => c1.concat(c2.filter(col => !c1.find(ccol => col == ccol))));
        headrow.childNodes = cols.map((col, colidx) => {
            var td = headrow.childNodes[colidx] || headrow.appendChild(document.createElement("td"));
            td.innerText = col.value || col;
            return td;
        });

        while (headrow.childNodes.length > cols.length) {
            headrow.removeChild(headrow.childNodes[headrow.childNodes.length - 1]);
        }

        var keyName = cols[0];
        tbody.childNodes = curArray.sort((a1, a2) => a1[keyName] > (a2[keyName]) ? 1 : a1[keyName] < (a2[keyName]) ? -1 : 0)
            .map((obj, idx) => {
                var curRow = tbody.childNodes[idx] || tbody.appendChild(document.createElement("tr"));
                var isEditable = false;
                curRow.childNodes = cols.map(col => {
                    isEditable |= obj[col] !== undefined && obj[col].version !== undefined;
                    var curCol = curRow.querySelector(`td.${col}`) || curRow.appendChild(document.createElement("td"));
                    curCol.classList.add(col);
                    jsonifyField(`${fPath != "/" ? fPath : ""}[${idx}].${col}`, curCol, fld, obj[col]);
                    return curCol;
                });
                if (isEditable) {
                    var btn = curRow.querySelector("button.dup") || curRow.appendChild(document.createElement("button"));
                    btn.classList.add("dup");
                    btn.textContent = "Dup";
                    btn.onclick = () => {
                        curArray.push(JSON.parse(JSON.stringify(obj)));
                        jsonifyArray(curfs, fPath, fld, curArray, curPath);
                        return false;
                    };
                    btn = curRow.querySelector("button.delete") || curRow.appendChild(document.createElement("button"));
                    btn.classList.add("delete");
                    btn.textContent = "Del"
                    btn.onclick = () => {
                        curArray.splice(idx, 1);
                        tbody.removeChild(curRow);
                        return false;
                    };
                }
                return curRow;
            });

        while (tbody.childNodes.length > curArray.length) {
            tbody.removeChild(tbody.childNodes[tbody.childNodes.length - 1]);
        }
    }
}

function HandleDatetimeValue(fld, input, val) {
    if (IsDatetimeValue(fld)) {
        var dt = new Date(val);
        if (dt && dt.valid) {
            return dt;
        }
    }
    return val;
}

function HandleBooleanValue(val, input) {
    if (input.nodeName == "INPUT")
        return ((val == "true") || (val == "yes") || (val === true));
    return val;
}

function HandleNumberValue(val, input, fld) {
    if (IsNumberValue(val)) {
        input.type = "number";
        if (isFloat(val)) {
            if ((fld != "Lattitude") && (fld != "Longitude") && (fld != "lat") && (fld != "lng")) {
                val = parseFloat(val).toFixed(2);
                input.step = "0.01";
            } else {
                val = parseFloat(val).toFixed(8);
                input.step = "0.00000001";
            }
        } else {
            val = parseInt(val);
        }
        return val;
    }
    return -1;
}
//#endregion

//#region Logs
function AddLogLine(logLn, logs) {
    var data = logLn.replaceAll(/^[^IDVWE]*/g, "").replaceAll(/(.*)([\x1B].*\n?)/g, "$1");

    if (data.match(/^[IDVWE] \([0-9.:]{12}\) [^:]+:.*/)) {
        log = document.createElement("div");
        log.classList.add("log");
        log.classList.add(`LOG${data.substr(0, 1)}`);

        var llevel = document.createElement("div");
        llevel.classList.add(`LOGLEVEL`);
        llevel.innerText = data.substr(0, 1);
        log.appendChild(llevel);

        var ldate = document.createElement("div");
        ldate.classList.add(`LOGDATE`);
        ldate.innerText = data.substr(3, 12);
        log.appendChild(ldate);

        var lfunc = document.createElement("div");
        lfunc.classList.add(`LOGFUNCTION`);
        lfunc.innerText = data.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");
        log.appendChild(lfunc);

        lmsg = document.createElement("div");
        lmsg.classList.add(`LOGLINE`);
        lmsg.innerText = data.substr(data.indexOf(lfunc.innerText) + lfunc.innerText.length + 2).replaceAll(/^[\r\n]*/g, "");
        log.appendChild(lmsg);

        logs.appendChild(log);
    } else {
        lmsg.innerText += `\n${data}`;
    }
    if (activeTab == logs)
        lmsg.scrollIntoView();
}

function refreshSystem() {
    var logs = document.getElementById("slide-4").getElementsByClassName("loglines")[0];
    activeTab = logs;

    if (logWs == null) {
        logWs = new WebSocket("ws://" + window.location.hostname + "/ws");
        logWs.onmessage = (event) => { event && event.data ? AddLogLine(event.data, logs) : null };
        logWs.onopen = () => { logWs.send("Logs") };
        logWs.onerror = (err) => {
            console.error(err);
            logWs = null;
        };
        logWs.onclose = (evt => {
            console.log("closed");
            logWs = null;
        })
    }
}
//#endregion

function SendCommand(body) {
    return fetch("/status/cmd", {
        method: 'put',
        body: JSON.stringify(body)
    });
}

function SaveForm(form) {
    fetch(form.action, {
        method: 'post',
        body: JSON.stringify(form.querySelector("fieldset").data)
    }).then(data => {
        refreshConfig(true);
    });
}


function refreshStatus(clicked) {
    var form = document.querySelectorAll(".system-config form")[0];
    if (clicked) {
        activeTab = form;
        if (controller)
            controller.abort();
    }
    const timeout = new AbortController();
    const timer = setTimeout(() => timeout.abort(), 1000);
    fetch('/status/', {
        method: 'post',
        signal: timeout.signal
    }).then(data => {
        clearTimeout(timer);
        data.json().then(status => {
            jsonifyFormFielset(
                "/",
                Array.from(form.querySelectorAll("fieldset")).find(fs => fs.id == "") || form.appendChild(document.createElement("fieldset")),
                status);
            if (stateWs == null) {
                stateWs = new WebSocket("ws://" + window.location.hostname + "/ws");
                stateWs.onmessage = (event) => {
                    event && event.data ? jsonifyFormFielset(
                        "/",
                        Array.from(form.querySelectorAll("fieldset")).find(fs => fs.id == "") || form.appendChild(document.createElement("fieldset")),
                        JSON.parse(event.data)) : null
                };
                stateWs.onopen = () => { stateWs.send("State") };
                stateWs.onerror = (err) => {
                    console.error(err);
                    stateWs = null;
                };
                stateWs.onclose = (evt => {
                    console.log("closed");
                    stateWs = null;
                })
            }
        });
    }).catch((err) => {
        clearTimeout(timer);
    });
}

function refreshConfig(clicked) {
    var form = document.querySelectorAll(".system-config form")[1];
    if (clicked) {
        activeTab = form;
        if (controller)
            controller.abort();
    }
    const timeout = new AbortController();
    const timer = setTimeout(() => timeout.abort(), 1000);
    fetch('/config', {
        method: 'post',
        signal: timeout.signal
    }).then(data => {
        clearTimeout(timer);
        data.json().then(config => {
            var saveBtn = form.querySelector("button");
            jsonifyFormFielset(
                "/",
                Array.from(form.querySelectorAll("fieldset")).find(fs => fs.id == "") || form.insertBefore(document.createElement("fieldset"), saveBtn),
                config);
        });
    }).catch((err) => {
        clearTimeout(timer);
    });
}

/*
 * A JavaScript implementation of the Secure Hash Algorithm, SHA-1, as defined
 * in FIPS 180-1
 * Version 2.2 Copyright Paul Johnston 2000 - 2009.
 * Other contributors: Greg Holt, Andrew Kepert, Ydnar, Lostinet
 * Distributed under the BSD License
 * See http://pajhome.org.uk/crypt/md5 for details.
 */
var hexcase = 0;
var b64pad = "";

function hex_sha1(a) { return rstr2hex(rstr_sha1(str2rstr_utf8(a))) }

function hex_hmac_sha1(a, b) { return rstr2hex(rstr_hmac_sha1(str2rstr_utf8(a), str2rstr_utf8(b))) }

function sha1_vm_test() { return hex_sha1("abc").toLowerCase() == "a9993e364706816aba3e25717850c26c9cd0d89d" }

function rstr_sha1(a) { return binb2rstr(binb_sha1(rstr2binb(a), a.length * 8)) }

function rstr_hmac_sha1(c, f) {
    var e = rstr2binb(c);
    if (e.length > 16) { e = binb_sha1(e, c.length * 8) }
    var a = Array(16),
        d = Array(16);
    for (var b = 0; b < 16; b++) {
        a[b] = e[b] ^ 909522486;
        d[b] = e[b] ^ 1549556828
    }
    var g = binb_sha1(a.concat(rstr2binb(f)), 512 + f.length * 8);
    return binb2rstr(binb_sha1(d.concat(g), 512 + 160))
}

function rstr2hex(c) {
    try { hexcase } catch (g) { hexcase = 0 };
    var f = hexcase ? "0123456789ABCDEF" : "0123456789abcdef";
    var b = "";
    var a;
    for (var d = 0; d < c.length; d++) {
        a = c.charCodeAt(d);
        b += f.charAt((a >>> 4) & 15) + f.charAt(a & 15)
    }
    return b
}

function str2rstr_utf8(c) {
    var b = "";
    var d = -1;
    var a, e;
    while (++d < c.length) {
        a = c.charCodeAt(d);
        e = d + 1 < c.length ? c.charCodeAt(d + 1) : 0;
        if (55296 <= a && a <= 56319 && 56320 <= e && e <= 57343) {
            a = 65536 + ((a & 1023) << 10) + (e & 1023);
            d++
        }
        if (a <= 127) { b += String.fromCharCode(a) } else { if (a <= 2047) { b += String.fromCharCode(192 | ((a >>> 6) & 31), 128 | (a & 63)) } else { if (a <= 65535) { b += String.fromCharCode(224 | ((a >>> 12) & 15), 128 | ((a >>> 6) & 63), 128 | (a & 63)) } else { if (a <= 2097151) { b += String.fromCharCode(240 | ((a >>> 18) & 7), 128 | ((a >>> 12) & 63), 128 | ((a >>> 6) & 63), 128 | (a & 63)) } } } }
    }
    return b
}

function rstr2binb(b) {
    var a = Array(b.length >> 2);
    for (var c = 0; c < a.length; c++) { a[c] = 0 }
    for (var c = 0; c < b.length * 8; c += 8) { a[c >> 5] |= (b.charCodeAt(c / 8) & 255) << (24 - c % 32) }
    return a
}

function binb2rstr(b) {
    var a = "";
    for (var c = 0; c < b.length * 32; c += 8) { a += String.fromCharCode((b[c >> 5] >>> (24 - c % 32)) & 255) }
    return a
}

function binb_sha1(v, o) {
    v[o >> 5] |= 128 << (24 - o % 32);
    v[((o + 64 >> 9) << 4) + 15] = o;
    var y = Array(80);
    var u = 1732584193;
    var s = -271733879;
    var r = -1732584194;
    var q = 271733878;
    var p = -1009589776;
    for (var l = 0; l < v.length; l += 16) {
        var n = u;
        var m = s;
        var k = r;
        var h = q;
        var f = p;
        for (var g = 0; g < 80; g++) {
            if (g < 16) { y[g] = v[l + g] } else { y[g] = bit_rol(y[g - 3] ^ y[g - 8] ^ y[g - 14] ^ y[g - 16], 1) }
            var z = safe_add(safe_add(bit_rol(u, 5), sha1_ft(g, s, r, q)), safe_add(safe_add(p, y[g]), sha1_kt(g)));
            p = q;
            q = r;
            r = bit_rol(s, 30);
            s = u;
            u = z
        }
        u = safe_add(u, n);
        s = safe_add(s, m);
        r = safe_add(r, k);
        q = safe_add(q, h);
        p = safe_add(p, f)
    }
    return Array(u, s, r, q, p)
}

function sha1_ft(e, a, g, f) {
    if (e < 20) { return (a & g) | ((~a) & f) }
    if (e < 40) { return a ^ g ^ f }
    if (e < 60) { return (a & g) | (a & f) | (g & f) }
    return a ^ g ^ f
}

function sha1_kt(a) {
    return (a < 20) ? 1518500249 : (a < 40) ? 1859775393 : (a < 60) ? -1894007588 : -899497514
}

function safe_add(a, d) {
    var c = (a & 65535) + (d & 65535);
    var b = (a >> 16) + (d >> 16) + (c >> 16);
    return (b << 16) | (c & 65535)
}

function bit_rol(a, b) {
    return (a << b) | (a >>> (32 - b))
};