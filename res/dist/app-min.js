'use strict';

const e = React.createElement;
var httpPrefix = "http://irtracker";

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
        wfetch(`${httpPrefix}/status/cmd`, {
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
        if (window.location.host || httpPrefix){
            wfetch(`${httpPrefix}/files/lfs/config`, {method: 'post'})
            .then(data => data.json())
            .then(json => json.filter(fentry => fentry.ftype == "file"))
            .then(devFiles => {
                var devices = devFiles.map(devFile=> devFile.name.substr(0,devFile.name.indexOf(".")));
                this.setState({ devices: devices, httpPrefix:httpPrefix });
                if (this.props?.onGotDevices)
                    this.props.onGotDevices(devices);
            })
            .catch(err => {console.error(err);this.setState({error: err})});
        }
    }

    componentDidMount() {
        this.getDevices();
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (prevProps?.httpPrefix != this.props.httpPrefix) {
            this.getDevices(this.props.httpPrefix);
        }
    }

    render() {
        return this.state?.devices?.length > 1 ?
            e("select", {
                key: genUUID(),
                value: this.props.selectedDeviceId,
                onChange: (elem) => this.props?.onSet(elem.target.value)
            }, (this.state?.devices||this.props.devices).map(device => e("option", { key: genUUID(), value: device }, device))):
            null
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

    getValue(fld,val) {
        if (val?.value !== undefined) {
            return this.getValue(fld,val.value);
        } else {
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

            if ((this.props.name == "name") && (val.match(/\/.*\.[a-z]{3}$/))) {
                return e("a", { href: `${httpPrefix}${val}` }, val);
            }

            return val;
        }
    }

    componentDidMount() {
        if (IsDatetimeValue(this.props.name)) {
            this.renderTime(document.getElementById(`vel${this.id}`), this.props.name, this.getValue(this.props.name,this.props.value));
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
            e("div", { key: genUUID(), className: "value", id: `vel${this.id}` }, IsDatetimeValue(this.props.name) ? "" : this.getValue(this.props.name,this.props.value))
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
                            var val = this.getValue(fld,json[0][fld]);
                            if (!this.sortedOn && !Array.isArray(val) && typeof val != 'object' && isNaN(this.getValue(fld,val))) {
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
        if (val?.value !== undefined) {
            return this.getValue(fld,val.value);
        } else {
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
    
            if ((fld == "name") && (val.match(/\/.*\.[a-z]{3}$/))) {
                return e("a", { href: `${httpPrefix}${val}` }, val);
            }
            return val;
        }
    }

    BuildBody(json) {
        if (json) {
            return e("tbody", { key: genUUID() },
                json.sort((e1,e2)=>(this.getValue(this.sortedOn,e1[this.sortedOn])+"").localeCompare((this.getValue(this.sortedOn,e2[this.sortedOn])+"")))
                    .map(line => e("tr", { key: genUUID() },
                        this.cols.map(fld => e("td", { key: genUUID(), className: "readonly" }, 
                            typeof this.getValue(fld, line[fld]) != 'object' ? 
                                e("div", { key: genUUID(), className: "value" }, this.getValue(fld, line[fld])) : 
                                Array.isArray(line[fld]) ? 
                                    e(StateTable,{key: genUUID(), name:fld, label:fld, json:line[fld]}):
                                    e(AppState,{key: genUUID(),json:line[fld]})
                        )))
                    ));
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
    constructor(props) {
        super(props);
        this.state = props.json;
    }

    componentWillUnmount() {
        this.mounted=false;
    }

    componentDidMount() {
        this.mounted=true;
    }

    Parse(json) {
        if (json) {
            return e("div",{key: genUUID(),className:"statusclass"},this.getSortedProperties(json).map(fld => {
                if (Array.isArray(json[fld])) {
                    if (fld == "commands"){
                        return {fld:fld,element:e(StateCommands,{key:genUUID(),name:json["name"],commands:json[fld],onSuccess:this.props.updateAppStatus})};
                    }
                    return {fld:fld,element:e(StateTable, { key: genUUID(), name: fld, label: fld, json: json[fld] })};
                } else if ((typeof json[fld] == 'object') && !Object.keys(json[fld]).find(fld => fld=="value")) {
                    return {fld:fld,element:e(AppState, { key: genUUID(), name: fld, label: fld, json: json[fld],updateAppStatus:this.props.updateAppStatus,registerEventInstanceCallback: this.props.registerEventInstanceCallback })};
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

    ProcessEvent(evt) {
        if (this.mounted && evt?.data){
            if ((evt.data?.class == this.state.class) && (evt.data?.name == this.state.name)){
                this.setState({...evt.data});
            }
        }
    }

    render() {
        if (this.state === null || this.state === undefined) {
            return e("div", { id: `loading${this.id}` }, "Loading...");
        }
        if (this.props.label != null) {
            if (this.props.registerEventInstanceCallback) {
                this.props.registerEventInstanceCallback(this.ProcessEvent.bind(this),`${this.state.className}-${this.state.name}`);
            }
            return e("fieldset", { id: `fs${this.props.label}` }, [e("legend", { key: genUUID() }, this.props.label), this.Parse(this.state)]);
        } else {
            return e("fieldset", { key: genUUID(), id: `fs${this.id}` }, this.Parse(this.state));
        }
    }
}

class MainAppState extends React.Component {
    constructor(props) {
        super(props);
        this.mounted=false;
    }

    componentWillUnmount() {
        this.mounted=false;
    }

    componentDidMount() {
        this.mounted=true;
        this.updateAppStatus();
        if (this.props.registerStateCallback) {
            this.props.registerStateCallback(this.refreshStatus.bind(this));
        }
    }

    updateAppStatus() {
        this.updateStatuses([{ url: "/status/" }, { url: "/status/app" }, { url: "/status/tasks", path: "tasks" }], {});
    }

    refreshStatus(stat) {
        if (this.mounted){
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
    }

    updateStatuses(requests, newState) {
        if (window.location.host || httpPrefix){
            var abort = new AbortController()
            var timer = setTimeout(() => abort.abort(), 4000);
            if (this.props.selectedDeviceId == "current") {
                Promise.all(requests.map(request => {
                    return new Promise((resolve, reject) => {
                        wfetch(`${httpPrefix}${request.url}`, {
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
                    if (this.mounted){
                        this.setState({
                            error: null,
                            status: this.orderResults(newState)
                        });
                    }
                }).catch(err => {
                    clearTimeout(timer);
                    if (err.code != 20) {
                        var errors = requests.filter(req => req.error);
                        document.getElementById("Status").style.opacity = 0.5
                        if (errors[0].waitFor) {
                            setTimeout(() => {
                                if (err.message != "Failed to wfetch")
                                    console.error(err);
                                this.updateStatuses(requests, newState);
                            }, errors[0].waitFor);
                        } else {
                            this.updateStatuses(requests, newState);
                        }
                    }
                });
            } else if (this.props.selectedDeviceId) {
                wfetch(`${httpPrefix}/lfs/status/${this.props.selectedDeviceId}.json`, {
                    method: 'get',
                    signal: abort.signal
                }).then(data => data.json()).then(fromVersionedToPlain)
                    .then(status => {
                        if (this.mounted)
                            this.setState({ status: this.orderResults(status) });
                    })
            }
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
            return [
                e("button", { key: genUUID(), onClick: elem => this.updateAppStatus() }, "Refresh"),
                e(AppState, {
                    key: genUUID(), 
                    json: this.state.status, 
                    selectedDeviceId: this.props.selectedDeviceId,
                    updateAppStatus: this.updateAppStatus.bind(this),
                    registerEventInstanceCallback: this.props.registerEventInstanceCallback
                })
            ];
        } else {
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
        if (window.location.hostname || httpPrefix)
            this.getJsonConfig(this.props.selectedDeviceId).then(config => this.setState({config:config}));
    }

    getJsonConfig(devid) {
        return new Promise((resolve, reject) => {
            if (window.location.host || httpPrefix){
                const timer = setTimeout(() => this.props.pageControler.abort(), 3000);
                wfetch(`${httpPrefix}/config${devid&&devid!="current"?`/${devid}`:""}`, {
                    method: 'post',
                    signal: this.props.pageControler.signal
                }).then(data => {
                    clearTimeout(timer);
                    resolve(data.json());
                }).catch((err) => {
                    clearTimeout(timer);
                    reject(err);
                });
            } else {
                reject({error:"Not connected"});
            }
        });
    }

    SaveForm(form) {
        this.getJsonConfig(this.props.selectedDeviceId).then(vcfg => fromPlainToVersionned(this.state.config, vcfg))
            .then(cfg => wfetch(form.target.action.replace("file://", httpPrefix) + "/" + (this.props.selectedDeviceId == "current" ? "" : this.props.selectedDeviceId), {
                method: 'put',
                body: JSON.stringify(cfg)
            }).then(res => alert(JSON.stringify(res)))
              .catch(res => alert(JSON.stringify(res))));
        form.preventDefault();
    }

    render() {
        if (this.state?.config) {
            return e("form", { onSubmit: form => this.SaveForm(form), key: `${this.props.id || genUUID()}`, action: "/config", method: "post" }, [
                //e(ConfigEditor, { key: genUUID(), deviceId: this.props.selectedDeviceId, deviceConfig: this.state.config }),
                e(AppState, {
                    key: genUUID(), 
                    json: this.state.config, 
                    selectedDeviceId: this.props.selectedDeviceId
                }),
                e("button", { key: genUUID(), type: "button", onClick:(elem) => this.getJsonConfig(this.props.selectedDeviceId).then(config => this.setState({config:config}))} , "Refresh")
            ]);
        } else {
            return e("div",{key:genUUID()},"Loading....");
        }
    }
}

class ControlPanel extends React.Component {
    constructor(props) {
        super(props);
        this.anims=[];
        this.state = {
            autoRefresh:props.autoRefresh
        }

        if (!this.props?.OnLineDevices?.length && (!httpPrefix && !window.location.hostname)){
            this.lookForDevs();
        } else if (this.props?.autoRefresh) {
            console.log(`from mount ${JSON.stringify(this.state)}`);
            this.openWs();
        }
    }

    componentDidMount() {
        this.mountWidget();
        window.requestAnimationFrame(this.drawDidget.bind(this));
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        this.mountWidget();
    }


    lookForDevs() {
        if (this.state?.scanning) {
            return
        }
        this.setState({scanning:true});
        var devices = [];
        for (var idx = 254; idx > 0; idx--) {
            devices.push(`192.168.1.${idx}`);
        }
        console.log(`Scanning for ${devices.length} devices`);
        var foundDevices=[];
        for (var idx = 0; idx < Math.min(10, devices.length); idx++) {
            if (devices.length) {
                this.scanForDevices(devices,foundDevices);
            }
        }
    }

    scanForDevices(devices,foundDevices) {
        if (devices.length) {
            var device = devices.pop();
            var abort = new AbortController()
            var timer = setTimeout(() => abort.abort(), 1000);
            var onLine=false;
            wfetch(`http://${device}/config/`, {
                method: 'post',
                signal: abort.signal
            }).then(data => {clearTimeout(timer); onLine=true; return data.json()})
              .then(fromVersionedToPlain)
              .then(dev => {
                  if (dev?.deviceid) {
                    foundDevices.push(dev);
                  }
                  if (devices.length) {
                    this.scanForDevices(devices,foundDevices);
                  } else {
                    if (!this.props?.OnLineDevices?.length) {
                        httpPrefix=`http://${foundDevices[0].devName}`
                        if (this.state?.autoRefresh && !this.state.connecting && !this.state.connected)
                            console.log("from scan finished good");
                            this.props.OnToggleAutoRefresh(true);
                        this.props.OnLiveDevChange(foundDevices);
                    }
                  }
                })
              .catch(err => {
                if (devices.length) {
                    this.scanForDevices(devices,foundDevices);
                } else {
                    if (!this.props?.OnLineDevices?.length) {
                        httpPrefix=`http://${foundDevices[0].devName}`
                        if (this.state?.autoRefresh && !this.state.connecting && !this.state.connected)
                            console.log("from scan finished bad");
                            this.props.OnToggleAutoRefresh(true);
                        this.props.OnLiveDevChange(foundDevices);
                    }
                }
              });
        }
    }

    mountWidget() {
        this.widget.addEventListener("click", (event) => {
            if ((event.offsetX > 2) &&
                (event.offsetX < 32) &&
                (event.offsetY > 2) &&
                (event.offsetY < 32)) {
                this.props.OnToggleAutoRefresh(!this.state?.autoRefresh);
            }
        });
    }

    closeWs() {
        this.props.OnToggleAutoRefresh(off);
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
    }

    openWs() {
        if (!window.location.hostname && !httpPrefix) {
            console.log("Not ws as not connected");
            return;
        }
        if (this.state?.connecting || this.state?.connected) {
            return;
        }
        console.log(`Connecting to ${httpPrefix || window.location.hostname } ${JSON.stringify(this.state)}...`);
        this.setState({connecting:true,running:false});
        console.log(`Connecting to ${httpPrefix || window.location.hostname } ${JSON.stringify(this.state)}`);
        var ws = this.ws = new WebSocket("ws://" + (httpPrefix == "" ? window.location.hostname : httpPrefix.substring(7)) + "/ws");
        var stopItWithThatShit = setTimeout(() => { console.log("Main timeout"); ws.close(); this.setState({connecting:false});}, 3000);
        ws.onmessage = (event) => {
            clearTimeout(stopItWithThatShit);
            if (!this.state.running || this.state.timeout) {
                console.log("Running");
                this.setState({ running: true, error: null, timeout: null });
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
            stopItWithThatShit = setTimeout(() => { this.setState({timeout:"Message"}); ws.close();console.log("Message timeout")},3000)
        };
        ws.onopen = () => {
            clearTimeout(stopItWithThatShit);
            this.setState({ connected: true, connecting:false });
            ws.send("Connected");
            console.log("Connected");
            stopItWithThatShit = setTimeout(() => { this.setState({timeout:"Connect"}); ws.close();console.log("Connect timeout")},3000)
        };
        ws.onerror = (err) => {
            console.error(err);
            clearTimeout(stopItWithThatShit);
            this.setState({error: err});
            ws.close();
        };
        ws.onclose = (evt => {
            console.log("Closed");
            clearTimeout(stopItWithThatShit);
            this.setState({connected:false,connecting:false});
        });
    }

    drawDidget(){
        if (!this.widget)
            return;
            
        var canvas = this.widget.getContext("2d");
        canvas.clearRect(0,0,100,40);

        this.browser(canvas, 2, 2, 30, 30, 10);
        this.chip(canvas, 100-20-10, 2, 20, 30, 5);

        if (this.anims.length){
            var animGroups = this.anims.reduce((pv,cv)=>{
                (pv[cv.type]=pv[cv.type]||[]).push(cv);
                return pv
            },{});
            for (var agn in animGroups) {
                canvas.beginPath();
                animGroups[agn].forEach(anim => {
                    this.drawSprite(anim, canvas);
                });
            }

            this.anims = this.anims.filter(anim => anim.state != 2);
        }
        window.requestAnimationFrame(this.drawDidget.bind(this));
    }

    drawSprite(anim, canvas) {
        if (!anim.state) {
            anim.state = 1;
            anim.x = anim.startX;
            anim.y = anim.startY;
        } else {
            anim.x -= 3+anim.weight;
        }
        canvas.strokeStyle = anim.lineColor;
        canvas.lineWidth = 1;
        canvas.shadowBlur = 1;
        canvas.shadowColor = anim.shadowColor;
        canvas.fillStyle = anim.color;
        canvas.moveTo(anim.x, anim.y);
        canvas.arc(anim.x, anim.y, 4 + (anim.weight), 0, 2 * Math.PI);
        canvas.fill();
        if (anim.x < 30) {
            anim.state = 2;
        }
        return anim;
    }

    browser(canvas, startX, startY, boxWidht, boxHeight, cornerSize) {
        canvas.beginPath();
        canvas.strokeStyle = '#00ffff';
        canvas.lineWidth = 2;
        canvas.shadowBlur = 2;
        canvas.shadowColor = '#00ffff';
        canvas.fillStyle = this.state?.autoRefresh ? (this.state?.error || this.state?.timeout ? "#f27c7c" : this.state?.connected?"#00ffff59":"#0396966b") : "#000000"
        this.roundedRectagle(canvas, startX, startY, boxWidht, boxHeight, cornerSize);
        canvas.fill();
        canvas.stroke();
    }

    chip(canvas, startX, startY, boxWidht, boxHeight, cornerSize) {
        canvas.beginPath();
        const pinWidth = 4;
        const pinHeight = 2;
        const pinVCount = 3;

        canvas.strokeStyle = '#00ffff';
        canvas.lineWidth = 2;
        canvas.shadowBlur = 2;
        canvas.shadowColor = '#00ffff';
        canvas.fillStyle = "#00ffff";
        this.roundedRectagle(canvas, startX, startY, boxWidht, boxHeight, cornerSize);

        for (var idx = 0; idx < pinVCount; idx++) {
            canvas.moveTo(startX,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
            canvas.lineTo(startX-pinWidth,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
            canvas.lineTo(startX-pinWidth,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
            canvas.lineTo(startX,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));

            canvas.moveTo(startX+boxWidht,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
            canvas.lineTo(startX+boxWidht+pinWidth,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
            canvas.lineTo(startX+boxWidht+pinWidth,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
            canvas.lineTo(startX+boxWidht,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        }
        canvas.stroke();
    }

    roundedRectagle(canvas, startX, startY, boxWidht, boxHeight, cornerSize) {
        canvas.moveTo(startX + cornerSize, startY);
        canvas.lineTo(startX + boxWidht - (2 * cornerSize), startY);
        canvas.arcTo(startX + boxWidht, startY, startX + boxWidht, startY + cornerSize, cornerSize);
        canvas.lineTo(startX + boxWidht, startY + boxHeight - cornerSize);
        canvas.arcTo(startX + boxWidht, startY + boxHeight, startX + boxWidht - cornerSize, startY + boxHeight, cornerSize);
        canvas.lineTo(startX + cornerSize, startY + boxHeight);
        canvas.arcTo(startX, startY + boxHeight, startX, startY + boxHeight - cornerSize, cornerSize);
        canvas.lineTo(startX, startY + cornerSize);
        canvas.arcTo(startX, startY, startX + cornerSize, startY, cornerSize);
    }

    AddLogLine(ln) {
        var anims = this.anims.filter(anim => anim.type == "log");
        var curAnims = anims.filter(anim => anim.level == ln[0]);
        if ((anims.length > 4) && (curAnims.length>1)) {
            (curAnims.find(anim => anim.weight < 3) || curAnims[0]).weight++;
        } else {
            this.anims.push({
                type:"log",
                level:ln[0],
                color:ln[0] == 'D' ? "green" : ln[0] == 'W' ? "yellow" : "red",
                weight: 1,
                lineColor: '#00ffff',
                shadowColor: '#00ffff',
                startX: 70,
                startY: 25,
                renderer: this.drawSprite
            })
        }
        if (ln && this.props.logCBFn) {
            this.props.logCBFn.forEach(logCBFn=>logCBFn(ln));
        }
    }
    
    UpdateState(state) {
        var anims = this.anims.filter(anim => anim.type == "state");
        if (anims.length > 4) {
            (anims.find(anim => anim.weight < 3) || anims[0]).weight++;
        } else {
            this.anims.push({
                type:"state",
                color:"#00ffff",
                weight: 1,
                lineColor: '#00ffff',
                shadowColor: '#00ffff',
                startX: 70,
                startY: 15,
                renderer: this.drawSprite
            });
        }
      if (this.props.stateCBFn) {
        this.props.stateCBFn.forEach(stateCBFn=>stateCBFn(state));
      }
    }
  
    ProcessEvent(event) {
        var anims = this.anims.filter(anim => anim.type == "event");
        var curAnims = anims.filter(anim => anim.eventBase == event.eventBase);
        if ((anims.length > 4) && (curAnims.length>1)) {
            (curAnims.find(anim => anim.weight < 3) || curAnims[0]).weight++;
        } else {
            this.anims.push({
                type:"event",
                eventBase: event.eventBase,
                color:"#7fffd4",
                weight: 1,
                lineColor: '#00ffff',
                shadowColor: '#00ffff',
                startX: 70,
                startY: 5,
                renderer: this.drawSprite
            });
        }

        if (this.props.eventCBFn) {
            this.props.eventCBFn.forEach(eventCBFn=>eventCBFn(event));
        }
    }

    render() {
     return e('fieldset', { key: genUUID(), className:`slides`, id: "controls", key: this.id }, [
        this.props?.OnLineDevices?.length && !window.location.hostname ?
            e("select",{
                key: genUUID(),
                className: "landevices",
                value: httpPrefix.substring(7),
                onChange: elem=>{httpPrefix=`http://${elem.target.value}`;this.props.ReloadPage()}
            },this.props.OnLineDevices.map(lanDev=>e("option",{
                key:genUUID(),
                className: "landevice"
            },lanDev.devName))):null,
        e("canvas",{
            key: genUUID(),
            height: 40,
            width:100,
            ref: (elem) => this.widget = elem
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
        this.mounted=false;
    }

    componentDidMount() {
        this.mounted=true;
    }

    componentWillUnmount() {
        this.mounted=false;
    }

    ProcessEvent(evt) {
        if (this.mounted){
            var lastEvents = this.state?.lastEvents||[];
            while (lastEvents.length > 100) {
                lastEvents.shift();
            }
            this.setState({lastEvents:lastEvents.concat(evt)});
        }
    }

    parseEvent(event) {
        var eventType = this.eventTypes.find(eventType => (eventType.eventBase == event.eventBase) && (eventType.eventName == event.eventId))
        if (eventType) {
            return {dataType:eventType.dataType,...event};
        } else {
            var req = this.eventTypeRequests.find(req=>(req.eventBase == event.eventBase) && (req.eventId == event.eventId));
            if (!req && this.mounted) {
                this.getEventDescriptor(event)
                    .then(eventType => this.setState({lastState: this.state.lastEvents
                        .map(event => event.eventBase == eventType.eventBase && event.eventId == eventType.eventName ? {dataType:eventType.dataType,...event}:{event})}
                    ));
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
                wfetch(`${httpPrefix}/eventDescriptor/${event.eventBase}/${event.eventId}`, {
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

    componentWillUnmount() {
        this.mounted=false;
    }

    componentDidMount(){
        this.mounted=true;
        if (window.location.hostname || httpPrefix)
            this.getJsonConfig().then(cfg => this.mounted?this.setState({events: cfg.events,programs:cfg.programs}):null);
    }

    getJsonConfig() {
        return new Promise((resolve, reject) => {
            if (window.location.hostname || httpPrefix){
                const timer = setTimeout(() => this.props.pageControler.abort(), 3000);
                wfetch(`${httpPrefix}/config${this.props.selectedDeviceId == "current"?"":`/${this.props.selectedDeviceId}`}`, {
                    method: 'post',
                    signal: this.props.pageControler.signal
                }).then(data => {
                    clearTimeout(timer);
                    return data.json();
                }).then( data => resolve(fromVersionedToPlain(data)
                )).catch((err) => {
                    clearTimeout(timer);
                    reject(err);
                });
            } else {
                reject({error:"Not connected"});
            }
        });
    }

    render() {
        if (this.state?.events){
            return [
                e("div", { key: genUUID() ,className: "designer" },[
                    e("details",{ key: genUUID() ,className: "configuredEvents", onClick:elem=>elem.target.parentElement.nextSibling.removeAttribute("open"), open:true}, [
                        e("summary",{ key: genUUID()},`${this.state.events?.length} Events`), 
                        e("div",{key:genUUID(),className:"content"},
                            this.state.events?.map(event => e(Event,{ key: genUUID(),...event})))
                    ]),
                    e("details",{ key: genUUID() ,className: "programs", onClick:elem=>elem.target.parentElement.previousSibling.removeAttribute("open")},[
                        e("summary",{ key: genUUID()},`${this.state.programs?.length} Programs`), 
                        e("div",{key:genUUID(),className:"content"},
                            this.state.programs?.map(program => e(Program,{ key: genUUID(),...program})))
                    ])
                ]),
                e(LiveEventPannel,{ key: genUUID(),registerEventCallback:this.props.registerEventCallback})
            ];
        } else {
            return e("div",{key: genUUID()},"Loading...");
        }
    }
}
class FirmwareUpdater extends React.Component {
    UploadFirmware(form) {
        form.preventDefault();
        this.setState({ loaded: `Sending ${this.state.len} firmware bytes` })
        if (this.state.fwdata && this.state.md5) {
            return wfetch(`${httpPrefix}/ota/flash?md5=${this.state.md5}&len=${this.state.len}`, {
                method: 'post',
                body: this.state.fwdata
            }).then(res => res.text())
                .then(res => this.setState({ loaded: res }));
        }
    }

    waitForDevFlashing() {
        var abort = new AbortController();
        var stopAbort = setTimeout(() => { abort.abort() }, 1000);
        wfetch(`${httpPrefix}/status/app`, {
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
}class UnparsedCVS extends React.Component {
    renderTrip(){
        if (!this.state?.points){
            wfetch(`${httpPrefix}${this.props.folder}/${this.props.name}`)
            .then(resp => resp.text())
            .then(content =>{
                var lns = content.split(/\n|\r\n/);
                var cols=lns[0].split(",");
                this.setState({
                    points:lns.splice(1).map(ln => {
                        var ret={};
                        ln.split(",").forEach((it,idx) => ret[cols[idx]] = isNaN(it)?it:parseFloat(it));
                        return ret;
                    }).filter(item => item.timestamp && item.timestamp.match(/[0-9]{4}\/[0-9]{2}\/[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}/))
                });
            });
        } else {
            this.setState({points:this.state.points});
        }
    }

    getIconColor() {
        if (!this.state?.points) {
            return "#00ff00";
        } else if (this.state.points.length) {
            return "aquamarine";
        } else {
            return "#ff0000";
        }
    }

    getIcon() {
        return e("svg", { key: genUUID(), xmlns: "http://www.w3.org/2000/svg", viewBox: "0 0 64 64",onClick:this.renderTrip.bind(this) }, [
            e("path", { key: genUUID(), style: { fill: this.getIconColor() }, d: "M17.993 56h20v6h-20zm-4.849-18.151c4.1 4.1 9.475 6.149 14.85 6.149 5.062 0 10.11-1.842 14.107-5.478l.035.035 1.414-1.414-.035-.035c7.496-8.241 7.289-20.996-.672-28.957A20.943 20.943 0 0 0 27.992 2a20.927 20.927 0 0 0-14.106 5.477l-.035-.035-1.414 1.414.035.035c-7.496 8.243-7.289 20.997.672 28.958zM27.992 4.001c5.076 0 9.848 1.976 13.437 5.563 7.17 7.17 7.379 18.678.671 26.129L15.299 8.892c3.493-3.149 7.954-4.891 12.693-4.891zm12.696 33.106c-3.493 3.149-7.954 4.892-12.694 4.892a18.876 18.876 0 0 1-13.435-5.563c-7.17-7.17-7.379-18.678-.671-26.129l26.8 26.8z" }),
            e("path", { key: genUUID(), style: { fill: this.getIconColor() }, d: "M48.499 2.494l-2.828 2.828c4.722 4.721 7.322 10.999 7.322 17.678s-2.601 12.957-7.322 17.678S34.673 48 27.993 48s-12.957-2.601-17.678-7.322l-2.828 2.828C12.962 48.983 20.245 52 27.993 52s15.031-3.017 20.506-8.494c5.478-5.477 8.494-12.759 8.494-20.506S53.977 7.97 48.499 2.494z" })
        ]);
    }

    render() {
        return e("div",{key:genUUID(),className:"rendered trip"},[
            this.getIcon(),
            this.state?.points?e(TripViewer,{key:genUUID(),points:this.state.points}):null
        ]
        );
    }
}
class FileViewer extends React.Component {
    constructor(props) {
        super(props);
        this.buildRenderers();
    }

    buildRenderers() {
        if (this.props.name.endsWith(".csv")) {
            wfetch(`${httpPrefix}${this.props.folder}/${this.props.name}`)
                .then(resp => resp.text())
                .then(content =>{
                    if (content.startsWith("timestamp,longitude,latitude,speed,altitude,course,RAM,Battery,Satellites") &&
                        !this.state?.renderes?.some("unparsedcsv")){
                        this.setState({renderes:["unparsedcsv",...(this.state?.renderes||[])]})
                    }
                })
        }
    }

    getRenderers() {
        return this.state?.renderes.map(renderer => {
            switch (renderer) {
                case "unparsedcsv":
                    return e(UnparsedCVS,{key:genUUID(),...this.props});
                    break;
            
                default:
                    return null;
                    break;
            }
        });
    }

    render() {
        return [e("a", { key:genUUID(), href: `${httpPrefix}${this.props.folder}/${this.props.name}` }, this.props.name),this.getRenderers()];
    }
}

class SFile extends React.Component {
    render() {
        return  e("tr", { key: genUUID(), className: this.props.file.ftype }, [
            e("td", { key: genUUID() }, this.getLink(this.props.file)),
            e("td", { key: genUUID() }, this.props.file.ftype != "file" ? "" : this.props.file.size),
            e("td", { key: genUUID() }, this.getDeleteLink())]);
    }

    getDeleteLink() {
        return this.props.path == "/" || this.props.file.name == ".." ? null : e("a", {
            key: genUUID(),
            href: "#",
            onClick: () => {
                wfetch(`${httpPrefix}/stat${this.props.path === "/" ? "" : this.props.path}/${this.props.file.name}`, {
                    method: 'post',
                    headers: {
                        ftype: this.props.file.ftype == "file" ? "file" : "directory",
                        operation: "delete"
                    }
                }).then(this.props.OnDelete ? this.props.OnDelete() : null)
                    .catch(err => {
                        console.error(err);
                    });
            }
        }, "Del");
    }

    getLink(file) {
        if (file.ftype == "folder") {
            return e("a", {
                key: genUUID(),
                href: "#",
                onClick: () => this.props.onChangeFolder ? this.props.onChangeFolder(`${file.folder || "/"}${file.name == ".." ? "" : "/" + file.name}`.replaceAll("//", "/")):null
            }, file.name);
        } else {
            return e(FileViewer,{key:genUUID(),...file});
        }
    }
}

class StorageViewer extends React.Component {
    constructor(props) {
        super(props);
        this.state = { loaded: false, path: "/", files: null };
        this.id = this.props.id || genUUID();
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
            wfetch(`${httpPrefix}/stat${fileToFetch.folder}/${fileToFetch.name}`, {
                method: 'post',
                signal: this.props.pageControler.signal
            }).then(data => {
                clearTimeout(quitItNow);
                data.json().then(jdata => {
                    fileToFetch.size = jdata.size;
                    if (this.mounted)
                        this.setState({ loaded: true, files: this.state.files, total: this.state?.total + jdata.size });
                });
                if (fileStatsToFetch.length && !this.props.pageControler.signal.aborted) {
                    if (this.mounted)
                        this.GetFileStat(fileStatsToFetch);
                }        
            }).catch(ex => {
                clearTimeout(quitItNow);
                console.err(ex);
            });
        }
    }

    fetchFiles() {
        if (window.location.host || httpPrefix){
            var quitItNow = setTimeout(() => this.props.pageControler.abort(), 3000);
            wfetch(`${httpPrefix}/files` + this.state.path, {
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
                        if (this.mounted){
                            this.setState({ loaded: true, files: files, total: files.reduce((e1,e2) => e1 + e2.size,0) });
                            var fileStatsToFetch = files.filter(file => file.ftype == "file" && !file.size);
                            for (var idx = 0; idx < Math.min(3, fileStatsToFetch.length); idx++) {
                                if (fileStatsToFetch.length) {
                                    this.GetFileStat(fileStatsToFetch);
                                }
                            }
                        }
                    });
            }).catch(console.error);
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

    componentDidMount(){
        this.mounted=true;
        if (!this.state?.files) {
            this.fetchFiles();
        }
    }

    componentWillUnmount() {
        this.mounted=false;
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
            return e("div", { key: genUUID() }, "Loading...");
        } else {
            return e("div", { key: genUUID(), 
                id: this.id, 
                className: `file-table ${this.state.loaded?"":"loading"}` }, 
                    e("table", { key: genUUID(), className: "greyGridTable" }, [
                        e("caption", { key: genUUID() }, this.state.path),
                        this.getTableHeader(),
                        e("tbody", { key: genUUID() }, 
                            this.getSystemFolders().concat(this.state.files).filter(file => file).map(file => e(SFile,{ 
                                key: genUUID(), 
                                file:file, 
                                path:this.state.path,
                                onChangeFolder: (folder) => this.setState({path:folder}),
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
    constructor(props) {
        super(props);
        this.mounted=false;
        this.state={logLines:[]};
    }

    componentDidMount() {
        this.mounted=true;
    }

    AddLogLine(logln) {
        if (this.mounted){
            this.setState({ logLines: [...(this.state?.logLines||[]),logln]});
        }
    }

    render() {
        if (this.props.registerLogCallback) {
            this.props.registerLogCallback(this.AddLogLine.bind(this));
        }
        return e("div", { key: genUUID(), className: "loglines" }, this.state?.logLines ? this.state.logLines.map(logln => e(LogLine,{ key: genUUID(), logln:logln})):null)
    }
}

class SystemPage extends React.Component {
    SendCommand(body) {
        return wfetch(`${httpPrefix}/status/cmd`, {
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
class TripViewer extends React.Component {
    constructor(props) {
        super(props);
        this.zoomlevel=15;
        this.state={images:{}};
    }
    componentDidMount() {
        this.mounted=true;
        this.drawMap();
    }
    

    drawMap() {
        return new Promise((resolve,reject) => {
            if (!this.mounted || !this.widget || !this.props.points || !this.props.points.length){
                reject({});
            } else {
                this.canvas = this.widget.getContext("2d");                    
        
                this.getTripTiles().then(this.drawTripTiles.bind(this))
                                   .then(this.drawTrip.bind(this))
                                   .catch(reject);
            }
        });
    }

    getTripTiles() {
        return new Promise((resolve,reject) => this.props.points?.length?resolve(this.props.points.reduce((ret, point) => {
            ret.trip.leftTile = this.lon2tile(ret.left = Math.min(ret.left, point.longitude));
            ret.trip.rightTile = this.lon2tile(ret.right = Math.max(ret.right, point.longitude));
            ret.trip.topTile = this.lat2tile(ret.top = Math.min(ret.top, point.latitude));
            ret.trip.bottomTile = this.lat2tile(ret.bottom = Math.max(ret.bottom, point.latitude));
            ret.trip.XTiles = (ret.trip.rightTile - ret.trip.leftTile) + 1;
            ret.trip.YTiles = (ret.trip.topTile - ret.trip.bottomTile) + 1;
            ret.margin.bottom = Math.floor((ret.maxYTiles-ret.trip.YTiles)/2);
            ret.margin.top = ret.maxYTiles-ret.trip.YTiles-ret.margin.bottom;
            ret.margin.left = Math.floor((ret.maxXTiles-ret.trip.XTiles)/2);
            ret.margin.right = ret.maxXTiles-ret.trip.XTiles-ret.margin.left;
            ret.leftTile=ret.trip.leftTile-Math.abs(ret.margin.left);
            ret.rightTile=Math.max(ret.maxXTiles+ret.trip.leftTile, ret.trip.rightTile+Math.abs(ret.margin.right));
            ret.bottomTile=ret.trip.bottomTile-Math.abs(ret.margin.bottom);
            ret.topTile=Math.max(ret.maxYTiles+ret.trip.bottomTile, ret.trip.bottomTile+Math.abs(ret.margin.top));
            ret.points.push(this.pointToCartesian(point));
            return ret;
        }, {
            left: this.props.points[0].longitude,
            right: this.props.points[0].longitude,
            top: this.props.points[0].latitude,
            bottom: this.props.points[0].latitude,
            trip:{},
            margin:{},
            points: [],
            margin: [],
            maxXTiles:Math.ceil(window.innerWidth/256),
            maxYTiles: Math.ceil(window.innerHeight/256)
        })):reject({"error":"no points"}));
    }

    setupListeners(canvas,tiles) {
        this.widget.addEventListener("mousemove", (event) => {
            var pt = tiles.points.find(point => (this.getClientX(point,tiles) >= (event.offsetX - 15)) &&
                (this.getClientX(point,tiles) <= (event.offsetX + 15)) &&
                (this.getClientY(point,tiles) >= (event.offsetY - 15)) &&
                (this.getClientY(point,tiles) <= (event.offsetY + 15)));
            var needsRefresh = false;
            tiles.points.filter(point => point != pt).forEach(point => {
                if (point.focused) {
                    point.focused = false;
                    needsRefresh = true;
                }
            });
            if (needsRefresh) {
                this.drawMap().then(res => {
                    if (pt && !pt.focused) {
                        pt.focused = true;
                        this.drawPointPopup(canvas, pt, tiles);
                    }
                });
            } else if (pt && !pt.focused) {
                pt.focused = true;
                this.drawPointPopup(canvas, pt, tiles);
            }
        });
    }

    drawPointPopup(canvas,pt,tiles){
        canvas.font = "10px Helvetica";
        var txtSz = canvas.measureText(new Date(`${pt.timestamp} UTC`).toLocaleString());
        var boxHeight=60;
        var boxWidth=txtSz.width+10;
        canvas.strokeStyle = '#00ffff';
        canvas.shadowColor = '#00ffff';
        canvas.fillStyle = "#ffffff";
        canvas.lineWidth = 1;
        canvas.shadowBlur = 2;
        canvas.beginPath();
        canvas.rect(this.getClientX(pt,tiles),this.getClientY(pt,tiles)-boxHeight,boxWidth,boxHeight);
        canvas.fill();
        canvas.stroke();

        canvas.fillStyle = 'rgba(00, 0, 0, 1)';
        canvas.strokeStyle = '#000000';

        canvas.beginPath();
        var ypos = this.getClientY(pt,tiles)-boxHeight+txtSz.actualBoundingBoxAscent+3;
        var xpos = this.getClientX(pt,tiles)+3;
        canvas.fillText(new Date(`${pt.timestamp} UTC`).toLocaleString(),xpos,ypos);
        ypos+=txtSz.actualBoundingBoxAscent+3;
        canvas.fillText(`Bat: ${Math.floor(pt.Battery)}`,xpos,ypos);
        ypos+=txtSz.actualBoundingBoxAscent+3;
        canvas.fillText(`Sats: ${pt.Satellites}`,xpos,ypos);
        ypos+=txtSz.actualBoundingBoxAscent+3;
        canvas.fillText(`Speed:${Math.floor(1.852*pt.speed/100.0)} km/h`,xpos,ypos);
        ypos+=txtSz.actualBoundingBoxAscent+3;
        canvas.fillText(`Alt:${Math.floor(pt.altitude/100)} m`,xpos,ypos);

        canvas.stroke();
    }

    drawTrip(tiles) {
        var firstPoint = tiles.points[0];
        this.canvas.strokeStyle = '#00ffff';
        this.canvas.shadowColor = '#00ffff';
        this.canvas.fillStyle = "#00ffff";
        this.canvas.lineWidth = 2;
        this.canvas.shadowBlur = 2;

        this.canvas.beginPath();
        this.canvas.moveTo(this.getClientX(firstPoint,tiles), this.getClientY(firstPoint,tiles));
        tiles.points.forEach(point => {
            this.canvas.lineTo(this.getClientX(point,tiles), this.getClientY(point,tiles));
        });
        this.canvas.stroke();
        this.canvas.strokeStyle = '#0000ff';
        this.canvas.shadowColor = '#0000ff';
        this.canvas.fillStyle = "#0000ff";
        tiles.points.forEach(point => {
            this.canvas.beginPath();
            this.canvas.arc(((point.tileX - tiles.leftTile) * 256) + (256 * point.posTileX), ((point.tileY - tiles.bottomTile) * 256) + (256 * point.posTileY), 5, 0, 2 * Math.PI);
            this.canvas.stroke();
        });
    }

    getClientY(firstPoint,tiles) {
        return ((firstPoint.tileY - tiles.bottomTile) * 256) + (256 * firstPoint.posTileY);
    }

    getClientX(firstPoint,tiles) {
        return ((firstPoint.tileX - tiles.leftTile) * 256) + (256 * firstPoint.posTileX);
    }

    drawTripTiles(tiles) {
        this.widget.width=(tiles.trip.XTiles+Math.abs(tiles.margin.left+tiles.margin.right))*256;
        this.widget.height=(tiles.trip.YTiles+Math.abs(tiles.margin.bottom+tiles.margin.top))*256;
        this.canvas.fillStyle = "black";
        this.canvas.fillRect(0,0,window.innerWidth,window.innerHeight);
        this.setupListeners(this.canvas,tiles);
        var promises = [];
        for (var tileX = tiles.leftTile; tileX <= tiles.rightTile; tileX++) {
            for (var tileY = tiles.bottomTile; tileY <= tiles.topTile; tileY++) {
                promises.push(this.drawTile(tileX, tileY, 0, tiles));
            }
        }
        return new Promise((resolve,reject)=>Promise.all(promises).then(res=>resolve(tiles)).catch(err=>resolve(tiles)));
    }

    drawTile(tileX, tileY, numTries, tiles) {
        return new Promise((resolve,reject) => {
            if (!this.state.images[tileX] || !this.state.images[tileX][tileY]){
                var tile = new Image();
                tile.posX = tileX - tiles.leftTile;
                tile.posY = tileY - tiles.bottomTile;
                tile.onload = (elem) => {
                    if (!this.state.images[tileX]) {
                        this.state.images[tileX]={};
                    }
                    this.state.images[tileX][tileY]=elem.currentTarget;
                    resolve(this.canvas.drawImage(elem.currentTarget, elem.currentTarget.posX * elem.currentTarget.width, elem.currentTarget.posY * elem.currentTarget.height));
                };
                tile.onerror = (err) => {
                    if (numTries > 3) {
                        reject(`Failed to fetch tile ${tileX},${tileY}`);
                    } else {
                        this.drawTile(tileX, tileY, numTries+1,tiles).then(resolve).catch(reject);
                    }
                };
                tile.src = `${httpPrefix}/sdcard/web/tiles/${tileX}/${tileY}.png`;
            } else {
                resolve(this.canvas.drawImage(this.state.images[tileX][tileY], 
                                         this.state.images[tileX][tileY].posX * this.state.images[tileX][tileY].width, 
                                         this.state.images[tileX][tileY].posY * this.state.images[tileX][tileY].height));
            }
        });
    }

    lon2tile(lon) { 
        return Math.floor(this.lon2x(lon)); 
    }
    
    lat2tile(lat)  { 
        return Math.floor(this.lat2y(lat));
    }

    lon2x(lon) { 
        return (lon+180)/360*Math.pow(2,this.zoomlevel); 
    }
    
    lat2y(lat)  { 
        return (1-Math.log(Math.tan(lat*Math.PI/180) + 1/Math.cos(lat*Math.PI/180))/Math.PI)/2 *Math.pow(2,this.zoomlevel); 
    }

    tile2long(x,z) {
        return (x/Math.pow(2,z)*360-180);
    }

    tile2lat(y,z) {
        var n=Math.PI-2*Math.PI*y/Math.pow(2,z);
        return (180/Math.PI*Math.atan(0.5*(Math.exp(n)-Math.exp(-n))));
    }
    
    pointToCartesian(point) {
        return {
            tileX: this.lon2tile(point.longitude, this.zoomlevel),
            tileY: this.lat2tile(point.latitude, this.zoomlevel),
            posTileX: this.lon2x(point.longitude,this.zoomlevel) - this.lon2tile(point.longitude, this.zoomlevel),
            posTileY: this.lat2y(point.latitude,this.zoomlevel) - this.lat2tile(point.latitude, this.zoomlevel),
            ...point
        };
    }

    closeIt(){
        this.setState({closed:true});
    }

    render(){
        return e("div",{key:genUUID(),className:`lightbox ${this.state?.closed?"closed":"opened"}`},[
            e("div",{key:genUUID()},[
                e("div",{key:genUUID(),className:"tripHeader"},[
                    `Trip with ${this.props.points.length} points`,
                    e("br"),
                    `From ${new Date(`${this.props.points[0].timestamp} UTC`).toLocaleString()} `,
                    `To ${new Date(`${this.props.points[this.props.points.length-1].timestamp} UTC`).toLocaleString()}`,
                ]),
                e("span",{key:genUUID(),className:"close",onClick:this.closeIt.bind(this)},"X")
            ]),
            e("div",{key:genUUID(),className:"trip"},e("canvas",{
                onresize:this.drawMap(),
                key:genUUID(),
                ref: (elem) => this.widget = elem
            }))
        ]);
    }
}
var app=null;

function wfetch(requestInfo, params) {
  return new Promise((resolve,reject) => {
    var anims = app.anims.filter(anim => anim.type == "post" && anim.from == "browser");
    var inSpot = getInSpot(anims, "browser");
    var reqAnim = inSpot;

    if (inSpot) {
      inSpot.weight++;
    } else {
      app.anims.push((reqAnim={
          type:"post",
          from: "browser",
          weight: 1,
          lineColor: '#00ffff',
          shadowColor: '#00ffff',
          startY: 5,
          renderer: app.drawSprite
      }));
    }

    fetch(requestInfo,params).then(resp => {
      var anims = app.anims.filter(anim => anim.type == "post" && anim.from == "chip");
      var inSpot = getInSpot(anims, "chip");

      if (inSpot) {
        inSpot.weight++;
      } else {
        app.anims.push({
            type:"post",
            from: "chip",
            weight: 1,
            lineColor: '#00ffff',
            shadowColor: '#00ffff',
            startY: 25,
            renderer: app.drawSprite
        });
      }
      resolve(resp);
    })
    .catch(err => {
      reqAnim.color="red";
      reqAnim.lineColor="red";
      reject(err);
    });
  })
}

class MainApp extends React.Component {
  constructor(props) {
    super(props);
    this.anims=[];
    app=this;
    this.state = {
      pageControler: this.GetPageControler(),
      selectedDeviceId: "current",
      httpPrefix: httpPrefix,
      tabs: { 
        Storage: {active: true}, 
        Status:  {active: false}, 
        Config:  {active: false}, 
        Logs:    {active: false},
        Events:  {active: false}
        },
        autoRefresh: (httpPrefix||window.location.hostname) ? true : false
      };
    if (!httpPrefix && !window.location.hostname){
      this.lookForDevs();
    }
    if (this.state?.autoRefresh && (httpPrefix || window.location.hostname)) {
      this.openWs();
    }
  }
  
  componentDidUpdate(prevProps, prevState, snapshot) {
    //this.mountWidget();
  }

  componentDidMount() {
    app=this;
    this.mountWidget();
    this.mounted=true;
  }

  componentWillUnmount(){
    this.mounted=false;
  }

//#region Control Pannel
  lookForDevs() {
    this.state.lanDevices = [];
    for (var idx = 254; idx > 0; idx--) {
      this.state.lanDevices.push(`192.168.1.${idx}`);
    }
    this.state.lanDevices=this.state.lanDevices.sort( () => .5 - Math.random() );
    var foundDevices=[];
    for (var idx = 0; idx < Math.min(10, this.state.lanDevices.length); idx++) {
        if (this.state.lanDevices.length) {
            this.scanForDevices(this.state.lanDevices,foundDevices);
        }
    }
  }

  scanForDevices(devices,foundDevices) {
    if (devices.length) {
      var device = devices.pop();
      var abort = new AbortController()
      var timer = setTimeout(() => abort.abort(), 1000);
      var onLine=false;
      wfetch(`http://${device}/config/`, {
          method: 'post',
          signal: abort.signal
      }).then(data => {clearTimeout(timer); onLine=true; return data.json()})
        .then(fromVersionedToPlain)
        .then(dev => {
            if (dev?.deviceid) {
              var anims = this.anims.filter(anim => anim.type == "ping" && anim.from=="chip");
              var inSpot = getInSpot(anims, "chip");
              if (inSpot) {
                inSpot.weight++;
              } else {
                  this.anims.push({
                      type:"ping",
                      from: "chip",
                      weight: 1,
                      lineColor: '#00ffff',
                      shadowColor: '#00ffff',
                      startY: 30,
                      renderer: this.drawSprite
                  })
              }
              foundDevices.push(dev);
              if (!httpPrefix){
                httpPrefix=`http://${foundDevices[0].devName}`
                this.state.autoRefresh=true;
                if (!this.state.connecting && !this.state.connected){
                  this.openWs();
                }
              }
              this.setState({OnLineDevices: foundDevices});
            }

            if (devices.length) {
              this.scanForDevices(devices,foundDevices);
            }
          })
        .catch(err => {
          if (devices.length) {
              this.scanForDevices(devices,foundDevices);
          }
        });
    }
  }

  mountWidget() {
    this.widget.addEventListener("click", (event) => {
      if ((event.offsetX > 2) &&
          (event.offsetX < 32) &&
          (event.offsetY > 2) &&
          (event.offsetY < 32)) {
        this.state.autoRefresh=!this.state.autoRefresh;
        this.state.autoRefresh?this.openWs():this.closeWs();
      }
    });
    if (!this.mounted)
      window.requestAnimationFrame(this.drawDidget.bind(this));
  }

  closeWs() {
    if (this.ws) {
        this.state.autoRefresh=false;
        this.ws.close();
        this.ws = null;
    }
  }

  openWs() {
    if (!window.location.hostname && !httpPrefix) {
        return;
    }
    if (this.state?.connecting || this.state?.connected) {
        return;
    }
    this.state.connecting=true;
    this.state.running=false;
    var ws = this.ws = new WebSocket("ws://" + (httpPrefix == "" ? window.location.hostname : httpPrefix.substring(7)) + "/ws");
    var stopItWithThatShit = setTimeout(() => { console.log("Main timeout"); ws.close(); this.state.connecting=false}, 3000);
    ws.onmessage = (event) => {
        clearTimeout(stopItWithThatShit);
        if (!this.state.running || this.state.timeout) {
            this.state.running= true;
            this.state.error= null;
            this.state.timeout= null;
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
        stopItWithThatShit = setTimeout(() => { this.state.timeout="Message"; ws.close();console.log("Message timeout")},3000)
    };
    ws.onopen = () => {
      clearTimeout(stopItWithThatShit);
        this.state.connected=true;
        this.state.connecting=false;
        ws.send("Connected");
        stopItWithThatShit = setTimeout(() => { this.state.timeout="Connect"; ws.close();console.log("Connect timeout")},3000)
    };
    ws.onerror = (err) => {
        console.error(err);
        clearTimeout(stopItWithThatShit);
        this.state.error= err;
        ws.close();
    };
    ws.onclose = (evt => {
        clearTimeout(stopItWithThatShit);
        this.state.connected=false;
        this.state.connecting=false;
        if (this.state.autoRefresh)
          this.openWs();
    });
  }

  drawDidget(){
    if (!this.widget)
        return;
        
    var canvas = this.widget.getContext("2d");
    canvas.clearRect(0,0,100,40);

    this.browser(canvas, 2, 2, 30, 30, 10);
    this.chip(canvas, 70, 2, 20, 30, 5);

    if (this.anims.length){
        var animGroups = this.anims.reduce((pv,cv)=>{
            (pv[cv.type]=pv[cv.type]||[]).push(cv);
            return pv
        },{});
        for (var agn in animGroups) {
            canvas.beginPath();
            animGroups[agn].forEach(anim => {
                this.drawSprite(anim, canvas);
            });
        }

        this.anims = this.anims.filter(anim => anim.state != 2);
    }
    window.requestAnimationFrame(this.drawDidget.bind(this));
  }

  drawSprite(anim, canvas) {
    if (!anim.state) {
        anim.state = 1;
        anim.x = anim.from == "chip" ? this.chipX : this.browserX;
        anim.endX = anim.from == "chip" ? this.browserX : this.chipX;
        anim.direction=anim.from=="browser"?1:-1;
        anim.y = anim.startY;
    } else {
        anim.x += (anim.direction)*(2);
    }
    var width=Math.min(15,4 + (anim.weight));
    if ((anim.y-(width/2)) <= 0) {
      anim.y = width;
    }
    canvas.strokeStyle = anim.lineColor;
    canvas.lineWidth = 1;
    canvas.shadowBlur = 1;
    canvas.shadowColor = anim.shadowColor;
    canvas.fillStyle = anim.color;
    canvas.moveTo(anim.x+width, anim.y);
    canvas.arc(anim.x, anim.y, width, 0, 2 * Math.PI);

    if ((anim.from == "browser") || (anim.type == "log"))
      canvas.fill();

    if (anim.weight > 1) {
      var today = ""+anim.weight
      canvas.font = Math.min(20,8+anim.weight)+"px Helvetica";
      var txtbx = canvas.measureText(today);
      if ((anim.from == "browser") || (anim.type == "log")){
        canvas.strokeStyle = "black";
        canvas.fillStyle = "black"
      }
      canvas.fillText(today, anim.x - txtbx.width / 2, anim.y + txtbx.actualBoundingBoxAscent / 2);
      if ((anim.from == "browser") || (anim.type == "log")){
        canvas.strokeStyle=anim.lineColor;
        canvas.fillStyle=anim.color;
      }
    }    

    canvas.stroke();
    
    if (anim.direction==1?(anim.x > anim.endX):(anim.x < anim.endX)) {
        anim.state = 2;
    }
    return anim;
  }

  browser(canvas, startX, startY, boxWidht, boxHeight, cornerSize) {
    this.browserX=startX+boxWidht;
    canvas.beginPath();
    canvas.strokeStyle = '#00ffff';
    canvas.lineWidth = 2;
    canvas.shadowBlur = 2;
    canvas.shadowColor = '#00ffff';
    canvas.fillStyle = this.state?.autoRefresh ? (this.state?.error || this.state?.timeout ? "#f27c7c" : this.state?.connected?"#00ffff59":"#0396966b") : "#000000"
    this.roundedRectagle(canvas, startX, startY, boxWidht, boxHeight, cornerSize);
    canvas.fill();
    if (this.state?.lanDevices?.length) {
      this.roundedRectagle(canvas, startX, startY, boxWidht, boxHeight * (this.state.lanDevices.length/254), cornerSize);
    }

    canvas.stroke();
  }

  chip(canvas, startX, startY, boxWidht, boxHeight, cornerSize) {
    this.chipX=startX;
    canvas.beginPath();
    const pinWidth = 4;
    const pinHeight = 2;
    const pinVCount = 3;

    canvas.strokeStyle = '#00ffff';
    canvas.lineWidth = 2;
    canvas.shadowBlur = 2;
    canvas.shadowColor = '#00ffff';
    canvas.fillStyle = "#00ffff";
    this.roundedRectagle(canvas, startX, startY, boxWidht, boxHeight, cornerSize);

    for (var idx = 0; idx < pinVCount; idx++) {
        canvas.moveTo(startX,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        canvas.lineTo(startX-pinWidth,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        canvas.lineTo(startX-pinWidth,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        canvas.lineTo(startX,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));

        canvas.moveTo(startX+boxWidht,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        canvas.lineTo(startX+boxWidht+pinWidth,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        canvas.lineTo(startX+boxWidht+pinWidth,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        canvas.lineTo(startX+boxWidht,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
    }
    canvas.stroke();
  }

  roundedRectagle(canvas, startX, startY, boxWidht, boxHeight, cornerSize) {
    canvas.moveTo(startX + cornerSize, startY);
    canvas.lineTo(startX + boxWidht - (2 * cornerSize), startY);
    canvas.arcTo(startX + boxWidht, startY, startX + boxWidht, startY + cornerSize, cornerSize);
    canvas.lineTo(startX + boxWidht, startY + boxHeight - cornerSize);
    canvas.arcTo(startX + boxWidht, startY + boxHeight, startX + boxWidht - cornerSize, startY + boxHeight, cornerSize);
    canvas.lineTo(startX + cornerSize, startY + boxHeight);
    canvas.arcTo(startX, startY + boxHeight, startX, startY + boxHeight - cornerSize, cornerSize);
    canvas.lineTo(startX, startY + cornerSize);
    canvas.arcTo(startX, startY, startX + cornerSize, startY, cornerSize);
  }

  AddLogLine(ln) {
    var anims = this.anims.filter(anim => anim.type == "log" && anim.level == ln[0]);
    var inSpot = getInSpot(anims, "chip");
    if (inSpot) {
      inSpot.weight++;
    } else {
        this.anims.push({
            type:"log",
            from: "chip",
            level:ln[0],
            color:ln[0] == 'D' ? "green" : ln[0] == 'W' ? "yellow" : "red",
            weight: 1,
            lineColor: '#00ffff',
            shadowColor: '#00ffff',
            startY: 25,
            renderer: this.drawSprite
        })
    }
    this.callbacks.logCBFn.forEach(logCBFn=>logCBFn(ln));
  }

  UpdateState(state) {
    var anims = this.anims.filter(anim => anim.type == "state");
    var inSpot = getInSpot(anims, "chip");
    if (inSpot) {
      inSpot.weight++;
    } else {
      this.anims.push({
        type:"state",
        from: "chip",
        color:"#00ffff",
        weight: 1,
        lineColor: '#00ffff',
        shadowColor: '#00ffff',
        startY: 5,
        renderer: this.drawSprite
      });
    }
    this.callbacks.stateCBFn.forEach(stateCBFn=>stateCBFn(state));
  }

  ProcessEvent(event) {
    var anims = this.anims.filter(anim => anim.type == "event");
    var inSpot = getInSpot(anims, "chip");
    if (inSpot) {
      inSpot.weight++;
    } else {
      this.anims.push({
          type:"event",
          from: "chip",
          eventBase: event.eventBase,
          color:"#7fffd4",
          weight: 1,
          lineColor: '#00ffff',
          shadowColor: '#00ffff',
          startY: 15,
          renderer: this.drawSprite
      });
    }
    this.callbacks.eventCBFn.forEach(eventCBFn=>eventCBFn.fn(event));
  }
//#endregion

//#region Page Tabulation
  GetPageControler() {
    var ret = new AbortController();
    ret.onabort = this.OnAbort;
    return ret;
  }

  OnAbort() {
    this.state.pageControler= this.GetPageControler();
  }

  setActiveTab(tab) {
    this.state.tabs[tab].active=true;
    Object.keys(this.state.tabs).filter(ttab => ttab != tab).forEach(ttab => this.state.tabs[ttab].active=false);
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
//#endregion

//#region Event Management
  registerStateCallback(stateCBFn) {
    if (!this.callbacks.stateCBFn.find(fn => fn.name == stateCBFn.name))
      this.callbacks.stateCBFn.push(stateCBFn);
  }

  registerLogCallback(logCBFn) {
    if (!this.callbacks.logCBFn.find(fn => fn.name == logCBFn.name))
      this.callbacks.logCBFn.push(logCBFn);
  }

  registerEventInstanceCallback(eventCBFn,instance) {
    var cur = null;
    if (!(cur=this.callbacks.eventCBFn.find(fn => fn.fn.name == eventCBFn.name && fn.instance === instance)))
      this.callbacks.eventCBFn.push({fn:eventCBFn,instance:instance});
    else {
      cur.fn=eventCBFn;
    }
  }

  registerEventCallback(eventCBFn) {
    if (!this.callbacks.eventCBFn.find(fn => fn.fn.name == eventCBFn.name && fn.instance === undefined))
      this.callbacks.eventCBFn.push({fn:eventCBFn});
  }
//#endregion

  getPage(name) {
    if (name == "Storage") {
      return e(StorageViewer, { pageControler: this.state.pageControler, active: this.state.tabs["Storage"].active});
    }
    if (name == "Status") {
      return e(MainAppState,  { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Status"].active, registerEventInstanceCallback:this.registerEventInstanceCallback.bind(this), registerStateCallback:this.registerStateCallback.bind(this) });
    }
    if (name == "Config") {
      return e(ConfigPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Config"].active });
    }
    if (name == "Logs") {
      return e(SystemPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Logs"].active, registerLogCallback:this.registerLogCallback.bind(this) });
    }
    if (name == "Events") {
      return e(EventsPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Events"].active, registerEventCallback:this.registerEventCallback.bind(this) });
    }
    return null;
  }

  ReloadPage(){
    this.setState({pageControler: this.GetPageControler()});
  }

  render() {
    this.callbacks={stateCBFn:[],logCBFn:[],eventCBFn:[]};
    return e("div",{key:genUUID(),className:"mainApp"}, [
      e('fieldset', { key: genUUID(), className:`slides`, id: "controls"}, [
        this.state?.OnLineDevices?.length && !window.location.hostname ?
            e("select",{
              key: genUUID(),
              className: "landevices",
              value: httpPrefix.substring(7),
              onChange: elem=>{httpPrefix=`http://${elem.target.value}`;this.state?.httpPrefix!=httpPrefix?this.setState({httpPrefix:httpPrefix}):null;this.ws?.close();}
              //onChange: elem=>{window.location=`http://${elem.target.value}`}
              },this.state.OnLineDevices.map(lanDev=>e("option",{
                  key:genUUID(),
                  className: "landevice"
              },lanDev.devName))
            ):null,
        e("canvas",{
            key: genUUID(),
            height: 40,
            width:100,
            ref: (elem) => this.widget = elem
        }),
        e(DeviceList, {
            key: genUUID(),
            selectedDeviceId: this.state.selectedDeviceId,
            httpPrefix: httpPrefix,
            onSet: val=>this.state?.selectedDeviceId!=val?this.setState({selectedDeviceId:val}):null
        })
      ]),
      Object.keys(this.state.tabs).map(tab => 
        e("details",{key:genUUID(),id:tab, className:"appPage slides", open: this.state.tabs[tab].active, onClick:elem=>{
            elem.target.parentElement.setAttribute("open",true);
            Object.keys(this.state.tabs).forEach(ttab => ttab == tab ? this.state.tabs[ttab].active=true : this.state.tabs[ttab].active=false );
            [].slice.call(elem.target.parentElement.parentElement.children).filter(ttab => ttab != elem).forEach(ttab => ttab.removeAttribute("open"))
          }},
          [
            e("summary",{key:genUUID(),className:"appTab"},tab),
            e("div",{key:genUUID(),className: tab == "Status" ? "pageContent system-config" : "pageContent"}, this.getPage(tab))
          ]
        )
      )
    ]);
  }
}

ReactDOM.render(
  e(MainApp, {
    key: genUUID(),
    className: "slider"
  }),
  document.querySelector(".slider")
);
function getInSpot(anims, origin) {
  return anims
        .filter(anim => anim.from == origin)
        .find(anim => (anim.x === undefined) || (origin == "browser" ? anim.x <= (app.browserX + 4 + anim.weight) : anim.x >= (app.chipX - 4- anim.weight)));
}

