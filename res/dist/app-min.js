'use strict';

const e = React.createElement;

const httpPrefix = "";//"http://irtracker";

//#region SHA-1
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
};//#region utility functions
function genUUID() {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function (c) {
        var r = Math.random() * 16 | 0,
            v = c == 'x' ? r : (r & 0x3 | 0x8);
        return v.toString(16);
    });
}

function dirname(path) {
    return (path.match(/.*\//) + "").replace(/(.+)\/$/g, "$1");
}

function isFloat(n) {
    return Number(n) === n && n % 1 !== 0;
}

function IsDatetimeValue(fld) {
    return fld.match(".*ime_.s$") || fld.match(".*ime_sec$");
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
    return ((val !== true) && (val !== false) && (typeof (val) != "string") && !isNaN(val) && (val !== ""));
}

function degToRad(degree) {
    var factor = Math.PI / 180;
    return degree * factor;
}

function fromVersionedToPlain(obj, level = "") {
    var ret = {};
    var arr = Object.keys(obj);
    var fldidx;
    for (fldidx in arr) {
        var fld = arr[fldidx];
        var isObj = false;
        if ((typeof obj[fld] == 'object') &&
            (Object.keys(obj[fld]).filter(cfld => !(isObj |= !(cfld == "version" || cfld == "value"))).length == 2)) {
            ret[fld] = obj[fld]["value"];
        } else if (Array.isArray(obj[fld])) {
            ret[fld] = [];
            obj[fld].forEach((item, idx) => {
                if (obj[fld][idx].boper) {
                    ret[fld][ret[fld].length - 1]["boper"] = obj[fld][idx].boper;
                } else {
                    ret[fld].push(fromVersionedToPlain(item, `${level}/${fld}[${idx}]`));
                }
            });
        } else if (typeof obj[fld] == 'object') {
            ret[fld] = fromVersionedToPlain(obj[fld], `${level}/${fld}`);
        } else {
            ret[fld] = obj[fld];
        }
    }
    return ret;
}

function fromPlainToVersionned(obj, vobj, level = "") {
    var ret = {};
    var arr = Object.keys(obj);
    for (var fldidx in arr) {
        var fld = arr[fldidx];
        var pvobj = vobj ? vobj[fld] : undefined;
        if (Array.isArray(obj[fld])) {
            ret[fld] = [];
            for (var idx = 0; idx < obj[fld].length; idx++) {
                var item = obj[fld][idx];
                var vitem = fromPlainToVersionned(item, pvobj ? pvobj[idx] : null, `${level}/${fld}[${idx}]`);
                ret[fld].push(vitem);
                if (vitem.boper) {
                    ret[fld].push({
                        boper: vitem.boper
                    });
                    delete vitem.boper;
                }
            }
        } else if (typeof obj[fld] == 'object') {
            ret[fld] = fromPlainToVersionned(obj[fld], pvobj, `${level}/${fld}`);
        } else {
            if ((fld === "boper") || (fld === "operator") || (fld === "otype") || (fld === "value") || (fld === "name")) {
                ret[fld] = obj[fld];
            } else {
                ret[fld] = {
                    version: pvobj === undefined ? 0 : obj[fld] === pvobj.value ? pvobj.version : pvobj.version + 1,
                    value: obj[fld]
                }
            }
        }
    }
    return ret;
}

const getCellValue = (tr, idx) => tr.children[idx].innerText || tr.children[idx].textContent;

const comparer = (idx, asc) => (a, b) => ((v1, v2) =>
    v1 !== '' && v2 !== '' && !isNaN(v1) && !isNaN(v2) ? v1 - v2 : v1.toString().localeCompare(v2)
)(getCellValue(asc ? a : b, idx), getCellValue(asc ? b : a, idx));
class BoolInput extends React.Component {
    constructor(props) {
        super(props);
        this.id = this.props.id || genUUID();
        if (this.props.onOn && this.props.initialState){
            this.props.onOn()
        }
        if (this.props.onOff && !this.props.initialState){
            this.props.onOff()
        }
    }

    toggleChange = (elem) => {
        elem.target.checked ?
            this.props.onOn ? this.props.onOn(elem.target) : null :
            this.props.onOff ? this.props.onOff(elem.target) : null;
        if (this.props.onChange) {
            this.props.onChange(elem.target.checked);
        }
    }

    render() {
        return e("label", { key: genUUID(), className: `editable ${this.props.blurred ? "loading":""}`, id: `lbl${this.id}`, key: this.id },
            e("div", { key: genUUID(), className: "label", id: `dlbl${this.id}` }, this.props.label),
            e("input", { key: genUUID(), type: "checkbox", onChange: this.toggleChange, id: `in${this.id}`, checked: this.props.initialState}));
    }
}
class CmdButton extends React.Component {
    runIt() {
        fetch(`${httpPrefix}/status/cmd`, {
            method: this.props.HTTP_METHOD,
            body: JSON.stringify({command: this.props.command,name: this.props.name, param1: this.props.param1, param2: this.props.param2})
        }).then(data => data.text())
          .then(this.props.onSuccess ? this.props.onSuccess : console.log)
          .catch(this.props.onError ? this.props.onError : console.error);
    }

    render() {
        return e("button", { key: genUUID(), onClick: this.runIt.bind(this) }, this.props.caption)
    }
}
class DeviceList extends React.Component {
    getDevices() {
        fetch(`${httpPrefix}/files/lfs/config`, {method: 'post'})
            .then(data => data.json())
            .then(json => json.filter(fentry => fentry.ftype == "file"))
            .then(devFiles => {
                var devices = devFiles.map(devFile=> devFile.name.substr(0,devFile.name.indexOf(".")));
                this.setState({ devices: devices });
                if (this.props?.onGotDevices)
                    this.props.onGotDevices(devices);
            })
            .catch(err => {console.error(err);this.setState({error: err})});
    }

    render() {
        if (!this.state?.devices && !this.props.devices && !this.state?.error) {
            this.getDevices();
        }
        return this.state?.error ? null :
            this.state?.devices || this.props?.devices ?
                e("select", {
                    key: genUUID(),
                    value: this.props.selectedDeviceId,
                    onChange: (elem) => this.props?.onSet(elem.target.value)
                }, (this.state?.devices||this.props.devices).map(device => e("option", { key: genUUID(), value: device }, device))):
                e("p",{jey:genUUID()},"Loading...")
    }
}
class IntInput extends React.Component {
    toggleChange = (elem) => {
        if (this.props.onChange) {
            this.props.onChange(elem.target.value);
        }
    }

    render() {
        return e("label", { key: genUUID(), className: "editable", id: `lbl${this.id}`, key: this.id },
            e("div", { key: genUUID(), className: "label", id: `div${this.id}` }, this.props.label),
            e("input", { key: genUUID(), type: "number", value: this.props.value, onChange: this.toggleChange.bind(this), id: `${this.id}` }));
    }
}
class ROProp extends React.Component {
    constructor(props) {
        super(props);
        this.id = this.props.id || genUUID();
    }

    getValue() {
        var val = this.props.value && this.props.value.version ? this.props.value.value : this.props.value;
        if (IsNumberValue(val)) {
            if (isFloat(val)) {
                if ((this.props.name.toLowerCase() != "lattitude") &&
                    (this.props.name.toLowerCase() != "longitude") &&
                    (this.props.name != "lat") && (this.props.name != "lng")) {
                    val = parseFloat(val).toFixed(2);
                } else {
                    val = parseFloat(val).toFixed(8);
                }
            }
        }
        if (IsBooleanValue(val)) {
            val = ((val == "true") || (val == "yes") || (val === true)) ? "Y" : "N"
        }
        return val;
    }

    componentDidMount() {
        if (IsDatetimeValue(this.props.name)) {
            this.renderTime(document.getElementById(`vel${this.id}`), this.props.name, this.getValue());
        }
    }

    renderTime(input, fld, val) {
        if (input == null) {
            return;
        }
        var now = fld.endsWith("_us") ? new Date(val / 1000) : fld.endsWith("_sec") ? new Date(val*1000) : new Date(val);

        if (now.getFullYear() <= 1970) 
            now.setTime(now.getTime() + now.getTimezoneOffset() * 60 * 1000);
            
        var today = now.toLocaleDateString('en-US',{dateStyle:"short"});
        var time = now.toLocaleTimeString('en-US',{hour12:false});
        var hrs = now.getHours();
        var min = now.getMinutes();
        var sec = now.getSeconds();
        var mil = now.getMilliseconds();
        var smoothsec = sec + (mil / 1000);
        var smoothmin = min + (smoothsec / 60);

        if (now.getFullYear() <= 1970) {
            today =  (now.getDate()-1) + ' Days';
            if (hrs == 0)
                time = (min ? min + ":" : "") + ('0'+sec).slice(-2) + "." + mil;
            else
                time = ('0'+hrs).slice(-2) + ":" + ('0'+min).slice(-2) + ":" + ('0'+sec).slice(-2);
        }


        var canvas = input.querySelector(`canvas`) || input.appendChild(document.createElement("canvas"));
        canvas.height = 100;
        canvas.width = 110;
        var ctx = canvas.getContext("2d");
        ctx.strokeStyle = '#00ffff';
        ctx.lineWidth = 4;
        ctx.shadowBlur = 2;
        ctx.shadowColor = '#00ffff'
        var rect = input.getBoundingClientRect();

        //Background
        var gradient = ctx.createRadialGradient(rect.width / 2, rect.height / 2, 5, rect.width / 2, rect.height / 2, rect.height + 5);
        gradient.addColorStop(0, "#03303a");
        gradient.addColorStop(1, "black");
        ctx.fillStyle = gradient;
        ctx.clearRect(0, 0, rect.width, rect.height);

        ctx.beginPath();
        ctx.arc(rect.width / 2, rect.height / 2, rect.height * 0.44, degToRad(270), degToRad(270+((hrs>12?hrs-12:hrs)*30)));
        ctx.stroke();

        ctx.beginPath();
        ctx.arc(rect.width / 2, rect.height / 2, rect.height * 0.38, degToRad(270), degToRad(270+(smoothmin * 6)));
        ctx.stroke();

        //Date
        ctx.font = "12px Helvetica";
        ctx.fillStyle = 'rgba(00, 255, 255, 1)'
        var txtbx = ctx.measureText(today);
        ctx.fillText(today, (rect.width / 2) - txtbx.width / 2, rect.height * .65);

        //Time
        ctx.font = "12px Helvetica";
        ctx.fillStyle = 'rgba(00, 255, 255, 1)';
        txtbx = ctx.measureText(time);
        ctx.fillText(time, (rect.width * 0.50) - txtbx.width / 2, rect.height * 0.45);

    }

    render() {
        return e('label', { className: "readonly", id: `lbl${this.id}`, key: this.id }, [
            e("div", { key: genUUID(), className: "label", id: `dlbl${this.id}` }, this.props.label),
            e("div", { key: genUUID(), className: "value", id: `vel${this.id}` }, IsDatetimeValue(this.props.name) ? "" : this.getValue())
        ]);
    }
}
class StateTable extends React.Component {
    constructor(props) {
        super(props);
        this.id = this.props.id || genUUID();
    }
    SortTable(th) {
        var table,tbody;
        Array.from((tbody=(table=th.target.closest("table")).querySelector('tbody')).querySelectorAll('tr:nth-child(n)'))
                 .sort(comparer(Array.from(th.target.parentNode.children).indexOf(th.target), this.asc = !this.asc))
                 .forEach(tr => tbody.appendChild(tr));
    }

    BuildHead(json) {
        if (json) {
            this.cols=[];
            return [e("thead", { key: genUUID(), onClick: this.SortTable.bind(this) }, e("tr", { key: genUUID() },
                json.flatMap(row => Object.keys(row))
                    .filter((val, idx, arr) => (val !== undefined) && (arr.indexOf(val) === idx))
                    .map(fld => {
                        if (!this.cols.some(col => fld == col)) {
                            this.cols.push(fld);
                            if (!this.sortedOn && json[0] && isNaN(json[0][fld])) {
                                this.sortedOn = fld;
                            }
                        }
                        return e("th", { key: genUUID() }, fld);
                    }))), e("caption", { key: genUUID() }, this.props.label)];
        } else {
            return null;
        }
    }

    getValue(fld, val) {
        if (fld.endsWith("_sec") && (val > 10000000)) {
            return new Date(val * 1000).toLocaleString();
        } else if (IsNumberValue(val)) {
            if (isFloat(val)) {
                if ((this.props.name.toLowerCase() != "lattitude") &&
                    (this.props.name.toLowerCase() != "longitude") &&
                    (this.props.name != "lat") && (this.props.name != "lng")) {
                    val = parseFloat(val).toFixed(2);
                } else {
                    val = parseFloat(val).toFixed(8);
                }
            }
        }
        if (IsBooleanValue(val)) {
            val = ((val == "true") || (val == "yes") || (val === true)) ? "Y" : "N"
        }
        return val;
    }

    BuildBody(json) {
        if (json) {
            return e("tbody", { key: genUUID() },
                json.sort((e1,e2)=>e1[this.sortedOn].localeCompare(e2[this.sortedOn]))
                    .map(line => e("tr", { key: genUUID() },
                                         this.cols.map(fld => e("td", { key: genUUID(), className: "readonly" }, 
                                                                             line[fld] !== undefined ? e("div", { key: genUUID(), className: "value" }, this.getValue(fld, line[fld])) : null)))));
        } else {
            return null;
        }
    }

    render() {
        if (!this.props?.json) {
            return e("div", { key: genUUID(), id: `loading${this.id}` }, "Loading...");
        }

        return e("label", { key: genUUID(), id: this.id, className: "table" }, 
               e("table", { key: genUUID(), className: "greyGridTable" }, 
               [this.BuildHead(this.props.json), 
               this.BuildBody(this.props.json)]));
    }
}
class StateCommands extends React.Component {
    render() {
        return e("div",{key:genUUID(),name:"commands", className:"commands"},this.props.commands.map(cmd => e(CmdButton,{
            key:genUUID(),
            caption:cmd.caption || cmd.command,
            command:cmd.command,
            name:this.props.name,
            onSuccess:this.props.onSuccess,
            onError:this.props.onError,
            param1:cmd.param1,
            param2:cmd.param2,
            HTTP_METHOD:cmd.HTTP_METHOD
        })));
    }
}

class AppState extends React.Component {
    Parse(json) {
        if (json) {
            return e("div",{key: genUUID(),className:"statusclass"},this.getSortedProperties(json).map(fld => {
                                                    if (Array.isArray(json[fld])) {
                                                        if (fld == "commands"){
                                                            return {fld:fld,element:e(StateCommands,{key:genUUID(),name:json["name"],commands:json[fld],onSuccess:this.props.updateAppStatus})};
                                                        }
                                                        return {fld:fld,element:e(StateTable, { key: genUUID(), name: fld, label: fld, json: json[fld] })};
                                                    } else if (typeof json[fld] == 'object') {
                                                        return {fld:fld,element:e(AppState, { key: genUUID(), name: fld, label: fld, json: json[fld],updateAppStatus:this.props.updateAppStatus })};
                                                    } else if ((fld != "class") && !((fld == "name") && (json["name"] == json["class"])) ) {
                                                        return {fld:fld,element:e(ROProp, { key: genUUID(), value: json[fld], name: fld, label: fld })};
                                                    }
                                                }).reduce((pv,cv)=>{
                                                    if (cv){
                                                        var fc = this.getFieldClass(json,cv.fld);
                                                        var item = pv.find(it=>it.fclass == fc);
                                                        if (item) {
                                                            item.elements.push(cv.element);
                                                        } else {
                                                            pv.push({fclass:fc,elements:[cv.element]});
                                                        }
                                                    }
                                                    return pv;
                                                },[]).map(item =>e("div",{key: genUUID(),className: `fieldgroup ${item.fclass}`},item.elements)))
        } else {
            return e("div", { id: `loading${this.id}` }, "Loading...");
        }
    }

    getSortedProperties(json) {
        return Object.keys(json)
            .sort((f1, f2) => { 
                var f1s = this.getFieldWeight(json, f1);
                var f2s = this.getFieldWeight(json, f2);
                if (f1s == f2s) {
                    return f1.localeCompare(f2);
                }
                return f1s > f2s ? 1 : f2s > f1s ? -1 : 0;
            }).filter(fld => !(typeof(json[fld]) == "object" && Object.keys(json[fld]).filter(fld=>fld != "class" && fld != "name").length==0));
    }

    getFieldWeight(json, f1) {
        return Array.isArray(json[f1]) ? 5 : typeof json[f1] == 'object' ? json[f1]["commands"] ? 3 : 4 : IsDatetimeValue(f1) ? 1 : 2;
    }

    getFieldClass(json, f1) {
        return Array.isArray(json[f1]) ? "array" : typeof json[f1] == 'object' ? json[f1]["commands"] ? "commandable" : "object" : "field";
    }

    render() {
        if (this.props.json === null || this.props.json === undefined) {
            return e("div", { id: `loading${this.id}` }, "Loading...");
        }
        if (this.props.label != null) {
            return e("fieldset", { id: `fs${this.id}` }, [e("legend", { key: genUUID() }, this.props.label), this.Parse(this.props.json)]);
        } else {
            return e("fieldset", { key: genUUID(), id: `fs${this.id}` }, this.Parse(this.props.json));
        }
    }
}

class MainAppState extends React.Component {
    updateAppStatus() {
        this.updateStatuses([{ url: "/status/" }, { url: "/status/app" }, { url: "/status/tasks", path: "tasks" }], {});
    }

    componentDidMount() {
        if (this.props.active) {
            document.getElementById("Status").scrollIntoView()
        }
    }

    refreshStatus(stat) {
        if (stat){
            const flds = Object.keys(stat);
            var status = this.state?.status || {};
            for (const fld in flds) {
                status[flds[fld]] = stat[flds[fld]];    
            }
            this.setState({status:status});
        } else {
            this.updateAppStatus();
        }
    }

    updateStatuses(requests, newState) {
        var abort = new AbortController()
        var timer = setTimeout(() => abort.abort(), 4000);
        if (this.props.selectedDeviceId == "current") {
            Promise.all(requests.map(request => {
                return new Promise((resolve, reject) => {
                    fetch(`${httpPrefix}${request.url}`, {
                        method: 'post',
                        signal: abort.signal
                    }).then(data => data.json())
                      .then(fromVersionedToPlain)
                      .then(jstats => {
                            requests = requests.filter(req => req != request);
                            if (request.path) {
                                newState[request.path] = Object.values(jstats);
                            } else {
                                Object.assign(newState, jstats);
                            }
                            resolve({ path: request.path, stat: jstats });
                        }).catch(err => {
                            request.retryCnt = (request.retryCnt | 0) + 1;
                            request.waitFor = 1000;
                            request.error = err;
                            reject(err);
                        });
                });
            })).then(results => {
                clearTimeout(timer);
                document.getElementById("Status").style.opacity = 1;
                this.setState({
                    error: null,
                    status: this.orderResults(newState)
                });
            }).catch(err => {
                clearTimeout(timer);
                if (err.code != 20) {
                    var errors = requests.filter(req => req.error);
                    document.getElementById("Status").style.opacity = 0.5
                    if (errors[0].waitFor) {
                        setTimeout(() => {
                            if (err.message != "Failed to fetch")
                                console.error(err);
                            this.updateStatuses(requests, newState);
                        }, errors[0].waitFor);
                    } else {
                        this.updateStatuses(requests, newState);
                    }
                }
            });
        } else if (this.props.selectedDeviceId) {
            fetch(`${httpPrefix}/lfs/status/${this.props.selectedDeviceId}.json`, {
                method: 'get',
                signal: abort.signal
            }).then(data => data.json()).then(fromVersionedToPlain)
                .then(status => {
                    this.setState({ status: this.orderResults(status) });
                })
        }
    }

    orderResults(res) {
        var ret = {};
        Object.keys(res).filter(fld => (typeof res[fld] != 'object') && !Array.isArray(res[fld])).sort((a, b) => a.localeCompare(b)).forEach(fld => ret[fld] = res[fld]);
        Object.keys(res).filter(fld => (typeof res[fld] == 'object') && !Array.isArray(res[fld])).sort((a, b) => a.localeCompare(b)).forEach(fld => ret[fld] = res[fld]);
        Object.keys(res).filter(fld => Array.isArray(res[fld])).forEach(fld => ret[fld] = res[fld]);
        return ret;
    }

    render() {
        if (this.state?.status){
            if (this.props.registerStateCallback) {
                this.props.registerStateCallback(this.refreshStatus.bind(this));
            }
            return [
                e("button", { key: genUUID(), onClick: elem => this.updateAppStatus() }, "Refresh"),
                e(AppState, {
                    key: genUUID(), 
                    json: this.state.status, 
                    selectedDeviceId: this.props.selectedDeviceId,
                    updateAppStatus: this.updateAppStatus.bind(this)
                })
            ];
        } else {
            this.updateAppStatus();
            return e("div",{key:genUUID()},"Loading...");
        }
    }
}
class ConfigEditor extends React.Component {
    componentDidMount() {
        if (this.props.deviceConfig) {
            this.jsonEditor = new JSONEditor(this.container, {
                onChangeJSON: json => Object.assign(this.props.deviceConfig, json)
            });
            this.jsonEditor.set(this.props.deviceConfig);
        } else {
            this.container.innerText = "Loading...";
        }
    }

    render() {
        return e("div", { key: genUUID(), ref: (elem) => this.container = elem, id: `${this.props.id || genUUID()}`, className: "column col-md-12", "data-theme": "spectre" })
    }
}

class ConfigPage extends React.Component {
    componentDidMount() {
        if (this.props.active) {
            document.getElementById("Config").scrollIntoView()
        }
    }

    getJsonConfig(devid) {
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => this.props.pageControler.abort(), 3000);
            fetch(`${httpPrefix}/config${devid&&devid!="current"?`/${devid}`:""}`, {
                method: 'post',
                signal: this.props.pageControler.signal
            }).then(data => {
                clearTimeout(timer);
                resolve(data.json());
            }).catch((err) => {
                clearTimeout(timer);
                reject(err);
            });
        });
    }

    SaveForm(form) {
        this.getJsonConfig(this.props.selectedDeviceId).then(vcfg => fromPlainToVersionned(this.state.config, vcfg))
            .then(cfg => fetch(form.target.action.replace("file://", httpPrefix) + "/" + (this.props.selectedDeviceId == "current" ? "" : this.props.selectedDeviceId), {
                method: 'put',
                body: JSON.stringify(cfg)
            }).then(res => alert(JSON.stringify(res)))
              .catch(res => alert(JSON.stringify(res))));
        form.preventDefault();
    }

    render() {
        if (this.state?.config) {
            return e("form", { onSubmit: form => this.SaveForm(form), key: `${this.props.id || genUUID()}`, action: "/config", method: "post" }, [
                e(ConfigEditor, { key: genUUID(), deviceId: this.props.selectedDeviceId, deviceConfig: this.state.config }),
                e("button", { key: genUUID() }, "Save"),
                e("button", { key: genUUID(), type: "button", onClick:(elem) => this.getJsonConfig(this.props.selectedDeviceId).then(config => this.setState({config:config}))} , "Refresh")
            ]);
        } else {
            this.getJsonConfig(this.props.selectedDeviceId).then(config => this.setState({config:config}));
            return e("div",{key:genUUID()},"Loading...");
        }
    }
}
class ControlPanel extends React.Component {
    componentDidUpdate(prevProps, prevState, snapshot) {
        if ((prevState?.periodicRefresh != this.state?.periodicRefresh) || (prevState?.refreshFrequency != this.state?.refreshFrequency)) {
            this.periodicRefresh(this.state?.periodicRefresh,this.state.refreshFrequency || 10)
        }
    }

    periodicRefresh(enabled, interval) {
        if (enabled && interval) {
            if (this.refreshTimeer) {
                clearTimeout(this.refreshTimeer);
            }
            this.refreshTimeer=setInterval(() => this.UpdateState(null),interval*1000);
        }
        if (!enabled && this.refreshTimeer) {
            clearTimeout(this.refreshTimeer);
            this.refreshTimeer = null;
        }
    }
    
    closeWs() {
        if (this.state?.autoRefresh)
            this.setState({autoRefresh:false});
        if (this.ws) {
            this.ws.close();
        }
    }

    openWs() {
        if (this.ws) {
            return;
        }
        if (!this.state?.autoRefresh)
            this.setState({autoRefresh:true});
        this.ws = new WebSocket("ws://" + (httpPrefix == "" ? window.location.hostname : httpPrefix.substring(7)) + "/ws");
        var stopItWithThatShit = setTimeout(() => { console.log("Main timeout"); if (this.ws.OPEN ) this.ws.close();}, 3000);
        this.ws.onmessage = (event) => {
            clearTimeout(stopItWithThatShit);

            if (!this.state.running || this.state.disconnected) {
                console.log("Connected");
                this.setState({ running: true, disconnected: false, error: null });
            }

            if (event && event.data) {
                if (event.data[0] == "{") {
                    if (event.data.startsWith('{"eventBase"')) {
                        this.ProcessEvent(fromVersionedToPlain(JSON.parse(event.data)));
                    } else {
                        this.UpdateState(fromVersionedToPlain(JSON.parse(event.data)));
                    }
                } else if (event.data.match(/.*\) ([^:]*)/g)) {
                    this.AddLogLine(event.data);
                }
            }
            stopItWithThatShit = setTimeout(() => { this.state.timeout="Message"; this.ws.close(); },3000)
        };
        this.ws.onopen = () => {
            clearTimeout(stopItWithThatShit);
            this.setState({ connected: true, disconnected: false });
            this.ws.send("Connect");
            stopItWithThatShit = setTimeout(() => { this.state.timeout="Connect"; this.ws.close(); },3000)
        };
        this.ws.onerror = (err) => {
            console.error(err);
            clearTimeout(stopItWithThatShit);
            this.state.error= err ;
            this.ws.close();
        };
        this.ws.onclose = (evt => {
            console.log("Closed");
            clearTimeout(stopItWithThatShit);
            this.setState({ connected: false, running: false });
            this.ws = null;
            if (this.state.autoRefresh) {
                setTimeout(() => {this.openWs();}, 1000);
            }
        });
    }

    AddLogLine(ln) {
      if (ln && this.props.logCBFn) {
        this.props.logCBFn.forEach(logCBFn=>logCBFn(ln));
      }
    }
    
    UpdateState(state) {
      if (this.props.stateCBFn) {
        this.props.stateCBFn.forEach(stateCBFn=>stateCBFn(state));
      }
    }
  
    ProcessEvent(event) {
        if (this.props.eventCBFn) {
            this.props.eventCBFn.forEach(eventCBFn=>eventCBFn(event));
        }
    }

    render() {
     return e('fieldset', { key: genUUID(), className:`slides`, id: "controls", key: this.id }, [
        e(BoolInput, { 
            key: genUUID(), 
            label: "Periodic Refresh", 
            initialState: this.state?.periodicRefresh,
            onChange: val=>this.setState({periodicRefresh:val})
        }),
        e(IntInput, {
            key: genUUID(),
            label: "Freq(sec)", 
            value: this.state?.refreshFrequency || 10, 
            onChange: val=>this.setState({refreshFrequency:val})
        }),
        e(BoolInput, {
            key: genUUID(),
            label: "Auto Refresh",
            onOn: this.openWs.bind(this),
            onOff: this.closeWs.bind(this),
            blurred: this.state?.autoRefresh && this.state.disconnected,
            initialState: this.state ? this.state.autoRefresh : true
        }),
        e(DeviceList, {
            key: genUUID(),
            selectedDeviceId: this.props.selectedDeviceId,
            devices: this.state?.devices,
            onSet: this.props.onSelectedDeviceId,
            onGotDevices: devices=>this.setState({devices:devices})
        })
     ]);
    }
}
class TypedField extends React.Component {
    render(){
        return [
            e("div",{key: genUUID(),className:this.props.type},this.props.type),
            this.props.path ? e("div",{key: genUUID(),className:"path"},this.props.path):null
        ];
    }
}

class LitteralField extends React.Component {
    render(){
        return e("div",{key: genUUID(),className:"litteral"},this.props.value);
    }
}

class EventField extends React.Component {
    render(){
        return e("div",{key: genUUID(),className:"conditionField"},this.props.name ? 
            e(TypedField,{key: genUUID(),type:this.props.name, value:this.props.value,path:this.props.path}):
            e(LitteralField,{key: genUUID(),value:this.props.value}) 
        );
    }
}

class EventCondition extends React.Component {
    render(){
        return e("div",{key: genUUID(),className:"condition"},[
            e(EventField,{key: genUUID(),...this.props.src}),
            e("div",{key: genUUID(),className:"operator"},this.props.operator),
            e(EventField,{key: genUUID(),...this.props.comp})
        ]);
    }
}

class EventConditions extends React.Component {
    render(){
        return e("details",{key: genUUID(), className:`conditions ${this.props.conditions.length == 0 ? "hidden":""}`},[
                e("summary",{key: genUUID()},"Conditions"),
                this.props.conditions.map(cond=>e(EventCondition,{key: genUUID(),...cond}))
            ]);
    }
}

class EventLine extends React.Component {
    render() {
        if (this.props.name == "conditions") {
            return e(EventConditions,{key: genUUID(),className:this.props.name,conditions:this.props.value});
        }
        return Array.isArray(this.props.value) ? 
            this.props.value.length > 0 ? 
                e("details",{key: genUUID(),className:`arrayfield ${this.props.name}`},
                    e("summary",{key: genUUID(),className:"name"}, this.props.name),
                        this.props.value.map(progl => e("div",{key: genUUID(),className:`arrayItem`},
                        Object.keys(progl).map(fld => e(EventLine,{key: genUUID(),name:fld,value:progl[fld]}))))
                ): null :
            typeof this.props.value === 'object' ?
                Object.keys(this.props.value).length > 0 ?
                    e("details",{key: genUUID(),className:`object ${this.props.name}`},[
                        e("summary",{key: genUUID(),className:`object name`},this.props.name),
                            Object.keys(this.props.value).map(fld => 
                                e("details",{key: genUUID(),className:"name"},[
                                    e("summary",{key: genUUID()}, fld),
                                    e("div",{key: genUUID(),className:fld},this.props.value[fld])
                                ]))
                    ]):null:
                e("details",{key: genUUID(),className:this.props.name},[e("summary",{key: genUUID()},this.props.name), this.props.value]);
    }
}


class Event extends React.Component {
    render() {
        return e("details",{key: genUUID() ,className: "event"},[
            e("summary",{key: genUUID(),className:"name"},[
                e("div",{key: genUUID(),className:"eventBase"},this.props.eventBase),
                e("div",{key: genUUID(),className:"eventId"},this.props.eventId)
            ]),Object.keys(this.props).filter(fld => fld != "name").map(fld => e(EventLine,{key: genUUID(),name:fld,value:this.props[fld]}))
        ]);
    }
}class LiveEvent extends React.Component {
    render() {
        if (this.props?.event?.dataType) {
            return this.renderComponent(this.props.event);
        } else {
            return e("summary",{key: genUUID() ,className: "liveEvent"},
                    e("div",{key: genUUID() ,className: "description"},[
                        e("div",{key: genUUID() ,className: "eventBase"},"Loading"),
                        e("div",{key: genUUID() ,className: "eventId"},"...")
                    ]));
        }
    }

    renderComponent(event) {
        return e("summary",{key: genUUID() ,className: "liveEvent"},[
            e("div",{key: genUUID() ,className: "description"},[
                e("div",{key: genUUID() ,className: "eventBase"},event.eventBase),
                e("div",{key: genUUID() ,className: "eventId"},event.eventId)
            ]), event.data ? e("details",{key: genUUID() ,className: "data"},this.parseData(event)): null
        ]);
    }

    parseData(props) {
        if (props.dataType == "Number") {
            return e("div", { key: genUUID(), className: "description" }, 
                        e("div", { key: genUUID(), className: "propNumber" }, props.data)
                    );
        }
        return Object.keys(props.data)
            .filter(prop => typeof props.data[prop] != 'object' && !Array.isArray(props.data[prop]))
            .map(prop => e("div", { key: genUUID(), className: "description" }, [
                e("div", { key: genUUID(), className: "propName" }, prop),
                e("div", { key: genUUID(), className: prop }, props.data[prop])
            ]));
    }
}

class LiveEventPannel extends React.Component {
    constructor(props) {
        super(props);
        this.eventTypes=[];
        this.eventTypeRequests=[];
        if (this.props.registerEventCallback) {
            this.props.registerEventCallback(this.ProcessEvent.bind(this));
        }
    }

    ProcessEvent(evt) {
        var lastEvents = this.state?.lastEvents||[];
        while (lastEvents.length > 100) {
            lastEvents.shift();
        }
        this.setState({lastEvents:lastEvents.concat(evt)});
    }

    parseEvent(event) {
        var eventType = this.eventTypes.find(eventType => (eventType.eventBase == event.eventBase) && (eventType.eventName == event.eventId))
        if (eventType) {
            return {dataType:eventType.dataType,...event};
        } else {
            var req = this.eventTypeRequests.find(req=>(req.eventBase == event.eventBase) && (req.eventId == event.eventId));
            if (!req) {
                this.getEventDescriptor(event)
                    .then(eventType => this.setState({lastState: this.state.lastEvents
                        .map(event => event.eventBase == eventType.eventBase && event.eventId == eventType.eventName ? {dataType:eventType.dataType,...event}:{event})}));
            }
            return {...event};
        }
    }

    getEventDescriptor(event) {
        var curReq;
        this.eventTypeRequests.push((curReq={waiters:[],...event}));
        return new Promise((resolve,reject) => {
            var eventType = this.eventTypes.find(eventType => (eventType.eventBase == event.eventBase) && (eventType.eventName == event.eventId))
            if (eventType) {
                resolve({dataType:eventType.dataType,...event});
            } else {
                var toControler = new AbortController();
                const timer = setTimeout(() => toControler.abort(), 3000);
                fetch(`${httpPrefix}/eventDescriptor/${event.eventBase}/${event.eventId}`, {
                    method: 'post',
                    signal: toControler.signal
                }).then(data => {
                    clearTimeout(timer);
                    return data.json();
                }).then( eventType => {
                    this.eventTypes.push(eventType);
                    resolve(eventType);
                }).catch((err) => {
                    console.error(err);
                    clearTimeout(timer);
                    curReq.waiters.forEach(waiter => {
                        waiter.reject(err);
                    });
                    reject(err);
                });
            }
        })
    }

    render() {
        return  e("div", { key: genUUID() ,className: "eventPanel" }, [
            e("div", { key: genUUID(), className:"control" }, [
                e("div",{ key: genUUID()}, `${this.state?.lastEvents?.length || "Waiting on "} event${this.state?.lastEvents?.length?'s':''}`),
                e("button",{ key: genUUID(), onClick: elem => this.setState({lastEvents:[]})},"Clear")
            ]),
            e("div",{ key: genUUID(), className:"eventList"},this.state?.lastEvents?.map(event => e(LiveEvent,{ key: genUUID(), event:this.parseEvent(event)})).reverse())
        ])

    }
}

class EventsPage extends React.Component {
    componentDidMount() {
        if (this.props.active) {
            document.getElementById("Events").scrollIntoView()
        }
    }

    getJsonConfig() {
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => this.props.pageControler.abort(), 3000);
            fetch(`${httpPrefix}/config${this.props.selectedDeviceId == "current"?"":`/${this.props.selectedDeviceId}`}`, {
                method: 'post',
                signal: this.props.pageControler.signal
            }).then(data => {
                clearTimeout(timer);
                return data.json();
            }).then( data => resolve(fromVersionedToPlain(data))).catch((err) => {
                clearTimeout(timer);
                reject(err);
            });
        });
    }

    render() {
        if (this.state?.events){
            return [
                e("div", { key: genUUID() ,className: "designer" },[
                    e("details",{ key: genUUID() ,className: "configuredEvents", onClick:elem=>elem.target.parentElement.nextSibling.removeAttribute("open"), open:true}, [e("summary",{ key: genUUID()},`${this.state.events?.length} Events`), e("div",{key:genUUID(),className:"content"},this.state.events?.map(event => e(Event,{ key: genUUID(),...event})))]),
                    e("details",{ key: genUUID() ,className: "programs", onClick:elem=>elem.target.parentElement.previousSibling.removeAttribute("open")},[e("summary",{ key: genUUID()},`${this.state.programs?.length} Programs`), e("div",{key:genUUID(),className:"content"},this.state.programs?.map(program => e(Program,{ key: genUUID(),...program})))])
                ]),
                e(LiveEventPannel,{ key: genUUID(),registerEventCallback:this.props.registerEventCallback})
            ];
        } else {
            this.getJsonConfig().then(cfg => this.setState({events: cfg.events,programs:cfg.programs}));
            return e("div",{key: genUUID()},"Loading...");
        }
    }
}
class FirmwareUpdater extends React.Component {
    UploadFirmware(form) {
        form.preventDefault();
        this.setState({ loaded: `Sending ${this.state.len} firmware bytes` })
        if (this.state.fwdata && this.state.md5) {
            return fetch(`${httpPrefix}/ota/flash?md5=${this.state.md5}&len=${this.state.len}`, {
                method: 'post',
                body: this.state.fwdata
            }).then(res => res.text())
                .then(res => this.setState({ loaded: res }));
        }
    }

    waitForDevFlashing() {
        var abort = new AbortController();
        var stopAbort = setTimeout(() => { abort.abort() }, 1000);
        fetch(`${httpPrefix}/status/app`, {
            method: 'post',
            signal: abort.signal
        }).then(res => {
            clearTimeout(stopAbort);
            return res.json()
        })
        .then(state => this.setState({ waiter: null, loaded: `Loaded version ${state.build.ver} built on ${state.build.date}` }))
        .catch(err => {
            clearTimeout(stopAbort);
            this.setState({ waiter: setTimeout(() => this.waitForDevFlashing(), 500) });
        });
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if ((this.state.loaded == "Flashing") && (this.state.waiter == null)) {
            this.waitForDevFlashing();
        }

        if (this.state.firmware && !this.state.md5) {
            this.state.md5 = "loading";
            var reader = new FileReader();
            reader.onload = () => {
                var res = reader.resultString || reader.result;
                var md5 = CryptoJS.algo.MD5.create();
                md5.update(CryptoJS.enc.Latin1.parse(reader.result));
                this.state.fwdata = new Uint8Array(this.state.firmware.size);
                for (var i = 0; i < res.length; i++) {
                    this.state.fwdata[i] = res.charCodeAt(i);
                }

                this.setState({
                    md5: md5.finalize().toString(CryptoJS.enc.Hex),
                    fwdata: this.state.fwdata,
                    len: this.state.firmware.size
                });
            };
            reader.onprogress = (evt) => this.setState({
                loaded: `${((this.state.firmware.size * 1.0) / (evt.loaded * 1.0)) * 100.0}% loaded`
            });
            reader.onerror = (evt) => this.setState({
                md5: null,
                firmware: null,
                error: evt.textContent
            });
            reader.onabort = (evt) => this.setState({
                md5: null,
                firmware: null,
                error: "Aborted"
            });
            reader.readAsBinaryString(this.state.firmware);
        }
    }

    render() {
        return e("form", { key: genUUID(), onSubmit: this.UploadFirmware.bind(this) },
            e("fieldset", { key: genUUID() }, [
                e("input", { key: genUUID(), type: "file", name: "firmware", onChange: event => this.setState({ firmware: event.target.files[0] }) }),
                e("button", { key: genUUID() }, "Upload"),
                e("div", { key: genUUID() }, this.state?.loaded)
            ])
        );
    }
}
class ProgramLine extends React.Component {
    render() {
        return Array.isArray(this.props.value) ? 
            e("details",{key: genUUID(),className:`arrayfield ${this.props.name}`},
                e("summary",{key: genUUID(),className:"name"}, this.props.name),
                    this.props.value.map(progl => e("div",{key: genUUID(),className:`arrayItem`},
                    Object.keys(progl).map(fld => e(ProgramLine,{key: genUUID(),name:fld,value:progl[fld]}))))
            ): 
            typeof this.props.value === 'object' ? 
                e("details",{key: genUUID(),className:`object ${this.props.name}`},[
                    e("summary",{key: genUUID(),className:`object name`},this.props.name),
                        Object.keys(this.props.value).map(fld => 
                            e("details",{key: genUUID(),className:"name"},[
                                e("summary",{key: genUUID()}, fld),
                                e("div",{key: genUUID(),className:fld},this.props.value[fld])
                            ]))
                ]):
                e("details",{key: genUUID(),className:this.props.name},[e("summary",{key: genUUID()},this.props.name), this.props.value]);
    }
}

class Program extends React.Component {
    render() {
        return e("details",{key: genUUID() ,className: "program"},[
            e("summary",{key: genUUID(),className:"name"},this.props.name),
            Object.keys(this.props).filter(fld => fld != "name").map(fld => e(ProgramLine,{key: genUUID(),name:fld,value:this.props[fld]}))
        ]);
    }
}
class SFile extends React.Component {
    render() {
        return  e("tr", { key: genUUID(), className: this.props.file.ftype }, [
            e("td", { key: genUUID() }, this.props.getFileLink(this.props.file)),
            e("td", { key: genUUID() }, this.props.file.ftype != "file" ? "" : this.props.file.size),
            e("td", { key: genUUID() }, this.props.path == "/" || this.props.file.name == ".." ? null : e("a", {
                key: genUUID(),
                href: "#",
                onClick: () => {
                    fetch(`${httpPrefix}/stat${this.props.path === "/" ? "" : this.props.path}/${this.props.file.name}`, {
                        method: 'post',
                        headers: {
                            ftype: this.props.file.ftype == "file" ? "file" : "directory",
                            operation: "delete"
                        }
                    }).then(this.props.OnDelete ? this.props.OnDelete():null)
                      .catch(err => {
                        console.error(err);
                      });
                }
            }, "Del"))]);
    }
}

class StorageViewer extends React.Component {
    constructor(props) {
        super(props);
        this.state = { loaded: false, path: "/", files: null };
        this.id = this.props.id || genUUID();
    }

    componentDidMount() {
        if (this.props.active) {
            document.getElementById("Storage").scrollIntoView()
        }
    }

    getSystemFolders() {
        return [
        this.state.path != "/" ?
            {
                ftype: "folder",
                name: "..",
                folder: dirname(this.state.path)
            }: null
        ];
    }

    GetFileStat(fileStatsToFetch) {
        if (fileStatsToFetch.length) {
            var fileToFetch = fileStatsToFetch.pop()
            var quitItNow = setTimeout(() => this.props.pageControler.abort(), 3000);
            fetch(`${httpPrefix}/stat${fileToFetch.folder}/${fileToFetch.name}`, {
                method: 'post',
                signal: this.props.pageControler.signal
            }).then(data => {
                clearTimeout(quitItNow);
                data.json().then(jdata => {
                    fileToFetch.size = jdata.size;
                    this.setState({ loaded: true, files: this.state.files, total: this.state?.total + jdata.size });
                });
                if (fileStatsToFetch.length && !this.props.pageControler.signal.aborted) {
                    this.GetFileStat(fileStatsToFetch);
                }        
            }).catch(ex => {
                clearTimeout(quitItNow);
                console.err(ex);
            });
        }
    }

    fetchFiles() {
        var quitItNow = setTimeout(() => this.props.pageControler.abort(), 3000);
        fetch(`${httpPrefix}/files` + this.state.path, {
            method: 'post',
            signal: this.props.pageControler.signal
        }).then(data => {
            clearInterval(quitItNow);
            data.json()
                .then(files => files.sort((f1, f2) => {
                    if (f1.ftype != f2.ftype) {
                        return f1.ftype == "folder" ? 1 : -1;
                    }
                    return f1.name == f2.name ? 0 : f1.name > f2.name ? 1 : -1;
                })).then(files => {
                    this.setState({ loaded: true, files: files, total: files.reduce((e1,e2) => e1 + e2.size,0) });
                    var fileStatsToFetch = files.filter(file => file.ftype == "file" && !file.size);
                    for (var idx = 0; idx < Math.min(3, fileStatsToFetch.length); idx++) {
                        if (fileStatsToFetch.length) {
                            this.GetFileStat(fileStatsToFetch);
                        }
                    }
                });
        }).catch(console.error);
    }

    getFileLink(file) {
        if (file.ftype == "folder") {
            return e("a", {
                key: genUUID(),
                href: "#",
                onClick: () => {
                    this.setState({ total: 0, loaded: false, path: `${file.folder || "/"}${file.name == ".." ? "" : "/" + file.name}`.replaceAll("//", "/") });
                }
            }, file.name);
        } else {
            return e("a", { href: `${httpPrefix}${this.state.path}/${file.name}` }, file.name);
        }
    }

    componentDidUpdate(prevProps, prevState) {
        if (!this.state.loaded) {
            this.state.loaded = true;
        }

        if (prevState && (prevState.path != this.state.path)) {
            this.fetchFiles();
        }
    }

    SortTable(th) {
        var table,tbody;
        Array.from((tbody=(table=th.target.closest("div.file-table")).querySelector('tbody')).querySelectorAll('tr:nth-child(n+2)'))
                 .sort(comparer(Array.from(th.target.parentNode.children).indexOf(th.target), this.asc = !this.asc))
                 .forEach(tr => tbody.appendChild(tr));
                }

    getTableHeader() {
        return e("thead", { key: genUUID() }, e("tr", { key: genUUID() }, ["Name", "Size"].map(col => e("th", { key: genUUID(), onClick: this.SortTable.bind(this) }, col)).concat(e("th", { key: genUUID() }, "Op"))));
    }

    render() {
        if (!this.state?.files) {
            this.fetchFiles();
            return e("div", { key: genUUID() }, "Loading...");
        } else {
            return e("div", { key: genUUID(), 
                id: this.id, 
                className: `file-table ${this.state.loaded?"":"loading"}` }, e("table", { key: genUUID(), className: "greyGridTable" }, [
                                            e("caption", { key: genUUID() }, this.state.path),
                                            this.getTableHeader(),
                                            e("tbody", { key: genUUID() }, 
                                                this.getSystemFolders().concat(this.state.files).filter(file => file).map(file => e(SFile,{ 
                                                    key: genUUID(), 
                                                    file:file, 
                                                    path:this.state.path, 
                                                    getFileLink:this.getFileLink.bind(this),
                                                    OnDelete: ()=>this.fetchFiles()
                                                }))
                                                ),
                                            e("tfoot", { key: genUUID() }, e("tr", { key: genUUID() }, [
                                                e("td", { key: genUUID() }, "Total"), 
                                                e("td", { key: genUUID() }, this.state.total)
                                            ]))
                                        ]));
        }
    }
}
class LogLine extends React.Component {
    isScrolledIntoView()
    {
        var rect = this.logdiv.getBoundingClientRect();
        var elemTop = rect.top;
        var elemBottom = rect.bottom;
    
        return (elemTop >= 0) && (elemBottom <= window.innerHeight);
    }

    render() {
        var msg = this.props.logln.match(/^[^IDVEW]*(.+)/)[1];
        var lvl = msg.substr(0, 1);
        var func = msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");
        var logLn = this.props.logln.substr(this.props.logln.indexOf(func) + func.length + 2).replaceAll(/^[\r\n]*/g, "").replaceAll(/.\[..\n$/g, "");

        return e("div", { key: genUUID() , className: `log LOG${lvl}` }, [
            e("div", { key: genUUID(), ref: ref => this.logdiv = ref, className: "LOGLEVEL" }, lvl),
            e("div", { key: genUUID(), className: "LOGDATE" }, msg.substr(3, 12)),
            e("div", { key: genUUID(), className: "LOGFUNCTION" }, func),
            e("div", { key: genUUID(), className: "LOGLINE" }, logLn)
        ]);
    }
}

class LogLines extends React.Component {
    AddLogLine(logln) {
        var logLines = (this.state?.logLines||[]);
        logLines.push(logln);
        this.setState({ logLines: logLines });
    }

    render() {
        if (this.props.registerLogCallback) {
            this.props.registerLogCallback(this.AddLogLine.bind(this));
        }
        return e("div", { key: genUUID(), className: "loglines" }, this.state?.logLines ? this.state.logLines.map(logln => e(LogLine,{ key: genUUID(), logln:logln})):null)
    }
}

class SystemPage extends React.Component {
    componentDidMount() {
        if (this.props.active) {
            document.getElementById("Logs").scrollIntoView()
        }
    }

    SendCommand(body) {
        return fetch(`${httpPrefix}/status/cmd`, {
            method: 'PUT',
            body: JSON.stringify(body)
        }).then(res => res.text().then(console.log))
          .catch(console.error)
          .catch(console.error);
    }

    render() {
        return [
            e("div", { key: genUUID() }, [
                e("button", { key: genUUID(), onClick: elem => this.setState({ logLines: [] }) }, "Clear Logs"),
                e("button", { key: genUUID(), onClick: elem => this.SendCommand({ 'command': 'reboot' }) }, "Reboot"),
                e("button", { key: genUUID(), onClick: elem => this.SendCommand({ 'command': 'parseFiles' }) }, "Parse Files"),
                e("button", { key: genUUID(), onClick: elem => this.SendCommand({ 'command': 'factoryReset' }) }, "Factory Reset"),
                e(FirmwareUpdater, { key: genUUID() })
            ]),
            e(LogLines, { key: genUUID(), registerLogCallback:this.props.registerLogCallback })
        ];
    }
}
class MainApp extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      pageControler: this.GetPageControler(),
      selectedDeviceId: "current",
      tabs: { 
        Storage: {active: true}, 
        Status:  {active: false}, 
        Config:  {active: false}, 
        Logs:    {active: false},
        Events:  {active: false}
        }
      };
    this.callbacks={stateCBFn:[],logCBFn:[],eventCBFn:[]};
  }

  GetPageControler() {
    var ret = new AbortController();
    ret.onabort = this.OnAbort;
    return ret;
  }

  OnAbort() {
    console.log("Abort!!!!");
    this.state.pageControler= this.GetPageControler();
  }

  setActiveTab(tab) {
    this.state.tabs[tab].active=true;
    Object.keys(this.state.tabs).filter(ttab => ttab != tab).forEach(ttab => this.state.tabs[ttab].active=false);
    this.refreshActiveTab();
  }

  componentDidMount() {
    this.refreshActiveTab();
  }
  
  refreshActiveTab() {
    Object.keys(this.state.tabs).forEach(ttab => {
      var section = document.getElementById(`${ttab}`);
      var link = document.getElementById(`a${ttab}`);
      if (this.state.tabs[ttab].active){
        link.classList.add("active")
        if (section)
          section.classList.add("active")
        if ((ttab == "Config") || (ttab == "Status") || (ttab == "Events")){
          document.getElementById("controls").classList.remove("hidden");
          document.querySelector("div.slides").classList.remove("expanded");
        } else {
          document.getElementById("controls").classList.add("hidden");
          document.querySelector("div.slides").classList.add("expanded");
        }
      } else{
        link.classList.remove("active")
        section.classList.remove("active")
      }
    });
  }

  registerStateCallback(stateCBFn) {
    if (!this.callbacks.stateCBFn.find(fn => fn.name == stateCBFn.name))
      this.callbacks.stateCBFn.push(stateCBFn);
  }

  registerLogCallback(logCBFn) {
    if (!this.callbacks.logCBFn.find(fn => fn.name == logCBFn.name))
      this.callbacks.logCBFn.push(logCBFn);
  }

  registerEventCallback(eventCBFn) {
    if (!this.callbacks.eventCBFn.find(fn => fn.name == eventCBFn.name))
      this.callbacks.eventCBFn.push(eventCBFn);
  }

  render() {
    return [
      e("div",{ key: genUUID(), className:"tabs"}, Object.keys(this.state.tabs).map(tab => e("a",{key: genUUID(),
                                                     id: `a${tab}`,
                                                     className: this.state.tabs[tab].active ? "active" : "",
                                                     onClick: () => this.setActiveTab(tab),href: `#${tab}`},tab))),
      e("div", { key: genUUID(), className:"slide"},[
      e(ControlPanel, { key: genUUID(), 
                        selectedDeviceId: this.state?.selectedDeviceId, 
                        onSelectedDeviceId: deviceId=>this.setState({selectedDeviceId:deviceId}), 
                        stateCBFn: this.callbacks.stateCBFn,
                        logCBFn: this.callbacks.logCBFn,
                        eventCBFn: this.callbacks.eventCBFn
                       }),
      e("div", { key: genUUID(), className: `slides` }, [
        e("div",{ className: `${this.state.tabs["Storage"].active ? "active":""} file_section`, id: "Storage", key: genUUID() },e(StorageViewer, { pageControler: this.state.pageControler, active: this.state.tabs["Storage"].active})),
        e("div",{ className: `${this.state.tabs["Status"].active ? "active":""} system-config`, id: "Status",  key: genUUID() },e(MainAppState,  { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Status"].active, registerStateCallback:this.registerStateCallback.bind(this) })),
        e("div",{ className: `${this.state.tabs["Config"].active ? "active":""} system-config`, id: "Config",  key: genUUID() },e(ConfigPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Config"].active })),
        e("div",{ className: `${this.state.tabs["Logs"].active ? "active":""} logs`,            id: "Logs",    key: genUUID() },e(SystemPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Logs"].active, registerLogCallback:this.registerLogCallback.bind(this) })),
        e("div",{ className: `${this.state.tabs["Events"].active ? "active":""} events`,        id: "Events",  key: genUUID() },e(EventsPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Events"].active, registerEventCallback:this.registerEventCallback.bind(this) }))
      ])])
    ];
  }
}

ReactDOM.render(
  e(MainApp, {
    key: genUUID(),
    className: "slider"
  }),
  document.querySelector(".slider")
);
