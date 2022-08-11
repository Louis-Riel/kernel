'use strict';

const e = React.createElement;
var httpPrefix = "";//"http://localhost:81";

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
            textColor: '#00ffff',
            shadowColor: '#000000',
            fillColor: '#004444',
            startY: 5,
            renderer: app.drawSprite
        }));
      }
  
      try{
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
                textColor: '#00ffff',
                shadowColor: '#000000',
                fillColor: '#004444',
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
      } catch(e) {
        reqAnim.color="red";
        reqAnim.lineColor="red";
        reject(err);
      }
    })
  }
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
    constructor(props) {
        super(props);
        this.state = {
            param1: this.props.param1,
            param2: this.props.param2,
            param3: this.props.param3,
            param4: this.props.param4,
            param5: this.props.param5,
            param6: this.props.param6
        };
    }
    runIt() {
        wfetch(`${httpPrefix}/status/cmd`, {
            method: this.props.HTTP_METHOD,
            body: JSON.stringify({command: this.props.command, className: this.props.className ,name: this.props.name, ...this.state})
        }).then(data => data.text())
          .then(this.props.onSuccess ? this.props.onSuccess : console.log)
          .catch(this.props.onError ? this.props.onError : console.error);
    }

    simpleCommand() {
        return e("button", { key: "simpleCommand", onClick: this.runIt.bind(this) }, this.props.caption)
    }

    GetPropertyEditor(param) {
        return e( MaterialUI.FormControl,{key: param},[
                e(MaterialUI.InputLabel,{
                    key: "label",
                    className: "label",
                    id: `${param}-label`
                },this.props[`${param}_label`]||param),
                e(MaterialUI.Input,{
                    key:"input",
                    id: `${param}-input`,
                    type: typeof this.state[param] === "number" || !isNaN(this.state[param]) ? 'number' : 'text',
                    label: param,
                    value: this.state[param],
                    onChange: elem => {this.state[param] = elem.target.type === 'number' ? parseInt(elem.target.value) : elem.target.value; this.setState(this.state)}
                })]);
    }

    complexCommand() {
        return e("div",{key: "complexCommand", className: "complex-command"},[Object.keys(this.props)
                                                                                   .filter(k => k.startsWith("param") && this.props[k+"_editable"])
                                                                                   .map(k => this.GetPropertyEditor(k)),
               e("button", { key: genUUID(), onClick: this.runIt.bind(this) }, this.props.caption)]);
    }

    hasComplexCommand() {
        return Object.keys(this.props).filter(k => k.endsWith("_editable") && this.props[k]).length > 0
    }

    render() {
        return this.hasComplexCommand() ? this.complexCommand() : this.simpleCommand()
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
            e("input", { key: genUUID(), type: "number", value: this.props.defaultValue, onChange: this.toggleChange.bind(this), id: `${this.id}` }));
    }
}
class StateCommands extends React.Component {
    render() {
        return e("div",{key:'commands',name:"commands", className:"commands"},this.props.commands.map(cmd => e(CmdButton,{
            key: `${cmd.command}-${cmd.param1}`,
            name:this.props.name,
            onSuccess:this.props.onSuccess,
            onError:this.props.onError,
            ...cmd
        })));
    }
}

class EditableLabel extends React.Component {
    constructor(props) {
        super(props);
        this.state ={
            editing: false
        }
    }

    getLabel() {
        return e("div",{onClick: elem=>this.setState({"editing":true}), className:"label"},this.props.label);
    }

    updateLabel(elem){
        this.newLabel= elem.target.value;
    }

    cancelUpdate(elem) {
        this.newLabel=undefined;
        this.setState({editing:false});
    }

    getEditable() {
        return [
            e("input",{key: "edit", defaultValue: this.props.label, onChange:this.updateLabel.bind(this)}),
            e("div",{key: "ok", onClick: elem=>this.setState({"editing":false}), className:"ok-button", dangerouslySetInnerHTML: {__html: "&check;"}}),
            e("div",{key: "cancel", onClick: this.cancelUpdate.bind(this), className:"cancel-button", dangerouslySetInnerHTML: {__html: "&Chi;"}})
        ]
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (this.props.onChange && this.newLabel && (prevState.editing !== this.state.editing)) {
            this.props.onChange(this.newLabel, this.props.label);
        }
    }

    render() {
        return this.state.editing ? this.getEditable() : this.getLabel();
    }
}

class LocalJSONEditor extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            json: props.json
        };
        if (this.props.registerEventInstanceCallback && this.props.name) {
            this.props.registerEventInstanceCallback(this.ProcessEvent.bind(this),`${this.props.class}-${this.props.name}`);
        }
    }

    componentWillUnmount() {
        this.mounted=false;
    }

    componentDidMount() {
        this.mounted=true;
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (prevProps && prevProps.json && (prevProps.json != this.props.json)){
            this.setState({json: this.props.json});
        }
    }

    ProcessEvent(evt) {
        if (this.mounted && evt?.data){
            if (evt.data?.name == this.props.name){
                this.setState({json: evt.data});
            }
        }
    }

    removeField(fld){
        delete this.state.json[fld];
        this.setState({value:this.state.json});
    }

    changeLabel(newLabel, oldLabel) {
        if (newLabel !== oldLabel) {
            Object.defineProperty(this.state.json, newLabel,
                Object.getOwnPropertyDescriptor(this.state.json, oldLabel));
            delete this.state.json[oldLabel];
        }
    }

    fieldControlPannel(fld){
        if (!this.props.editable){
            return e("div", { key: `fcpnolabel${fld}`, className: "label", id: `dlbl${this.id}` }, fld);
        }

        return e("div",{key:`fcpnolabel${fld}header`,className:"fieldHeader"},[
            e(EditableLabel,{key:`flabel${fld}`,label: fld, onChange: this.changeLabel.bind(this)}),
            e("div",{key:`fcpnolabel${fld}popupmenu`,className:"popupMenu"},[
                e("div",{key:`removefield`, onClick:elem=> this.removeField(fld)},"Remove")
            ])
        ]);
    }

    Parse(json) {
        if ((json !== undefined) && (json != null)) {
            if ((typeof json == "object") && (json.version === undefined)){
                return e("div",{key: `jsonobject`,className:"statusclass jsonNodes"},this.getSortedProperties(json)
                                                                                         .map(fld => this.renderField(json, fld))
                                                                                         .reduce((pv,cv)=>{
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
                },[]).map((item) =>e("div",{key: `fg-${item.fclass}`,className: `fieldgroup ${item.fclass}`},item.elements)))
            } else {
                return this.renderVersioned(json);
            }
        } else {
            return null;
        }
    }

    renderField(json, fld) {
        if (Array.isArray(json[fld])) {
            if (fld == "commands"){
                return this.renderCommands(fld, json);
            }
            return this.renderArray(fld, json);
        } else if (json[fld] && (typeof json[fld] == 'object') && (json[fld].version === undefined)) {
            return this.renderObject(fld, json);
        } else if ((fld != "class") && !((fld == "name") && (json["name"] == json["class"])) ) {
            return this.renderFieldValue(fld, json);
        }
    }

    renderFieldValue(fld, json) {
        return {
            fld: fld,
            element: this.props.editable ?
                e("label", { key: `${fld}label` }, [
                    this.fieldControlPannel(fld),
                    e("input", { key: `input`, defaultValue: json[fld].value === undefined ? json[fld] : json[fld].value, onChange: this.processUpdate.bind(this) })
                ]) :
                e(ROProp, {
                    key: `rofld${fld}`,
                    value: json[fld],
                    name: fld,
                    label: fld
                })
        };
    }

    renderObject(fld, json) {
        return {
            fld: fld, element: e(LocalJSONEditor, {
                key: `JE-${this.props.path}/${fld}`,
                path: `${this.props.path}/${fld}`,
                name: fld,
                label: fld,
                editable: this.props.editable,
                json: json[fld],
                updateAppStatus: this.props.updateAppStatus,
                registerEventInstanceCallback: this.props.registerEventInstanceCallback
            })
        };
    }

    renderArray(fld, json) {
        return {
            fld: fld, element: e(Table, {
                key: `Table-Object-${this.props.path}/${fld}`,
                name: fld,
                path: `${this.props.path}/${fld}`,
                label: fld,
                json: json[fld],
                registerEventInstanceCallback: this.props.registerEventInstanceCallback,
                editable: this.props.editable,
                sortable: this.props.sortable
            })
        };
    }

    renderCommands(fld, json) {
        return { fld: fld, element: e(StateCommands, { key: `${fld}cmds`, name: json["name"], commands: json[fld], onSuccess: this.props.updateAppStatus }) };
    }

    renderVersioned(json) {
        return (json.version !== undefined) || (this.props.editable && (typeof json != "object")) ?
            e("input", { key: `input`, defaultValue: json.value === undefined ? json : json.value, onChange: this.processUpdate.bind(this) }) :
            e(ROProp, {
                key: 'rofield',
                value: json,
                name: ""
            });
    }

    processUpdate(elem,fld) {
        if (fld) {
            if (this.state.json[fld] === undefined) {
                this.state.json[fld]={version:-1};
            }
            this.state.json[fld].value=elem.target.value;
            this.state.json[fld].version++;
        } else {
            this.state.json.value=elem.target.value;
            this.state.json.version++;
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
            }).filter(fld => !json[fld] || !(typeof(json[fld]) == "object" && Object.keys(json[fld]).filter(fld=>fld != "class" && fld != "name").length==0));
    }

    getFieldWeight(json, f1) {
        if (!IsDatetimeValue(f1) && (!json || (json[f1] === undefined))) {
            return 2;
        }
        return Array.isArray(json[f1]) ? 6 : typeof json[f1] == 'object' ? json[f1] && json[f1]["commands"] ? 4 : 5 : IsDatetimeValue(f1) ? 1 : 3;
    }

    getFieldClass(json, f1) {
        if (!json || !json[f1]) {
            return "field";
        }
        return Array.isArray(json[f1]) ? "array" : typeof json[f1] == 'object' && json[f1].version === undefined ? json[f1]["commands"] ? "commandable" : "object" : "field";
    }

    addNewProperty(){
        this.state.json[`prop${Object.keys(this.state.json).filter(prop => prop.match(/^prop.*/)).length+1}`]=null;
        this.setState({value:this.state.json});
    }

    objectControlPannel(){
        return [
            e("div",{key:'ocplabel',className:"jsonlabel"},this.props.label),
            this.props.editable?e("div",{key:genUUID(),className:"popupMenu"},
                e("div",{key:`removeprop`, onClick:elem=> this.addNewProperty()},"Add Column")):null
        ];

    }

    render() {
        if (this.state.json === null || this.state.json === undefined) {
            return e("div", { id: `loading${this.id}` }, "Loading...");
        } else if (this.props.label != null) {
            return e("fieldset", { id: `fs${this.props.label}`,className:"jsonNodes" }, [
                e("legend", { key: 'legend' }, this.objectControlPannel()),
                this.Parse(this.state.json)
            ]);
        } else {
            return this.Parse(this.state.json);
        }
    }
}class ROProp extends React.Component {
    constructor(props) {
        super(props);
        this.id = this.props.id || genUUID();
        if (IsNumberValue(this.props.value)) {
            this.state = {
                maxLastStates: 50,
                lastStates:[
                    {
                        value:this.props.value,
                        ts: Date.now()
                    }
                ]
            };
        }
    }

    getValue(fld,val) {
        if (val?.value !== undefined) {
            val = this.getValue(fld,val.value);
        }
        
        if (IsNumberValue(val) && isFloat(val)) {
            val =  parseFloat(val).toFixed(this.isGeoField() ? 8 : 2).replace(/0+$/,'');
        }
        
        if (IsBooleanValue(val)) {
            val = ((val === "true") || (val === "yes") || (val === true)) ? "Y" : "N"
        }
        
        if ((this.props.name === "name") && (val.match(/\/.*\.[a-z]{3}$/))) {
            val = e("a", { href: `${httpPrefix}${val}` }, val.split('/').reverse()[0]);
        }
        return val;
    }

    isGeoField() {
        return (this.props.name.toLowerCase() === "lattitude") ||
            (this.props.name.toLowerCase() === "longitude") ||
            (this.props.name === "lat") || 
            (this.props.name === "lng");
    }

    componentDidMount() {
        if (IsDatetimeValue(this.props.name)) {
            this.renderTime(document.getElementById(`vel${this.id}`), this.props.name, this.getValue(this.props.name,this.props.value));
        }
    }
    componentDidUpdate(prevProps, prevState, snapshot) {
        if (IsDatetimeValue(this.props.name)) {
            this.renderTime(document.getElementById(`vel${this.id}`), this.props.name, this.getValue(this.props.name,this.props.value));
        }
        if (this.state?.lastStates && (this.props.value !== prevProps?.value)) {
            this.setState({lastStates: [...this.getActiveLastStates(), {
                value: this.getValue(this.props.name,this.props.value),
                ts: Date.now()
            }]});
        }
    }

    getActiveLastStates() {
        if (this.state.lastStates.length > (this.state.maxLastStates - 1)) {
            return this.state.lastStates.splice(this.state.lastStates.length - this.state.maxLastStates);
        }
        return this.state.lastStates;
    }

    renderTime(input, fld, val) {
        if (input == null) {
            return;
        }
        var { hrs, smoothmin, today, time } = this.getTimeComponents(fld, val);

        var canvas = input.querySelector(`canvas`) || input.appendChild(document.createElement("canvas"));
        canvas.height = 100;
        canvas.width = 110;
        var ctx = canvas.getContext("2d");
        ctx.strokeStyle = '#00ffff';
        ctx.lineWidth = 4;
        ctx.shadowBlur = 2;
        ctx.shadowColor = '#00ffff'
        var rect = input.getBoundingClientRect();
        rect.height = canvas.height;
        rect.width = canvas.width;

        this.drawBackground(ctx, rect, hrs, smoothmin);
        this.drawClock(ctx, today, rect, time);
    }

    getTimeComponents(fld, val) {
        var now = fld.endsWith("_us") ? new Date(val / 1000) : fld.endsWith("_sec") ? new Date(val * 1000) : new Date(val);

        if (now.getFullYear() <= 1970)
            now.setTime(now.getTime() + now.getTimezoneOffset() * 60 * 1000);

        var today = now.toLocaleDateString('en-US', { dateStyle: "short" });
        var time = now.toLocaleTimeString('en-US', { hour12: false });
        var hrs = now.getHours();
        var min = now.getMinutes();
        var sec = now.getSeconds();
        var mil = now.getMilliseconds();
        var smoothsec = sec + (mil / 1000);
        var smoothmin = min + (smoothsec / 60);

        if (now.getFullYear() <= 1970) {
            today = (now.getDate() - 1) + ' Days';
            if (hrs == 0)
                time = (min ? min + ":" : "") + ('0' + sec).slice(-2) + "." + mil;

            else
                time = ('0' + hrs).slice(-2) + ":" + ('0' + min).slice(-2) + ":" + ('0' + sec).slice(-2);
        }
        return { hrs, smoothmin, today, time };
    }

    drawClock(ctx, today, rect, time) {
        //Date
        ctx.font = "12px Helvetica";
        ctx.fillStyle = 'rgba(00, 255, 255, 1)';
        var txtbx = ctx.measureText(today);
        ctx.fillText(today, (rect.width / 2) - txtbx.width / 2, rect.height * .65);

        //Time
        ctx.font = "12px Helvetica";
        ctx.fillStyle = 'rgba(00, 255, 255, 1)';
        txtbx = ctx.measureText(time);
        ctx.fillText(time, (rect.width * 0.50) - txtbx.width / 2, rect.height * 0.45);
    }

    drawBackground(ctx, rect, hrs, smoothmin) {
        var gradient = ctx.createRadialGradient(rect.width / 2, rect.height / 2, 5, rect.width / 2, rect.height / 2, rect.height + 5);
        gradient.addColorStop(0, "#03303a");
        gradient.addColorStop(1, "black");
        ctx.fillStyle = gradient;
        ctx.clearRect(0, 0, rect.width, rect.height);

        ctx.beginPath();
        ctx.arc(rect.width / 2, rect.height / 2, rect.height * 0.44, degToRad(270), degToRad(270 + ((hrs > 12 ? hrs - 12 : hrs) * 30)));
        ctx.stroke();

        ctx.beginPath();
        ctx.arc(rect.width / 2, rect.height / 2, rect.height * 0.38, degToRad(270), degToRad(270 + (smoothmin * 6)));
        ctx.stroke();
    }

    getGraphButton() {
        var ret = null;
        if (this.hasStats()) {
            ret = e(MaterialUI.Tooltip,{
                      className: "graphtooltip",
                      key:"tooltip",
                      width: "200",
                      height: "100",
                      title: this.state.graph ? "Close" : this.getReport(true)
                    },e('i',{className: "reportbtn fa fa-line-chart", key: "graphbtn", onClick: elem=>this.setState({"graph":this.state.graph?false:true})})
                  );
        }
        return ret;
    }

    hasStats() {
        return !IsDatetimeValue(this.props.name) &&
            IsNumberValue(this.props.value) &&
            this.state?.lastStates &&
            this.state.lastStates.reduce((ret, state) => ret.find(it => it === state.value) ? ret : [state.value, ...ret], []).length > 2;
    }

    getReport(summary) {
        return [ !summary && this.state?.lastStates?.length ? e('i',{className: "reportbtn fa fa-eraser", key: "clear data", onClick: elem=>this.setState({"lastStates":[],"graph":false})}) : null,
                e(Recharts.ResponsiveContainer,{key:"chartcontainer", className: "chartcontainer"},
                e(Recharts.LineChart,{key:"chart", data: this.state.lastStates, className: "chart", margin: {left:20}},[
                    e(Recharts.Line, {key:"line", dot: !summary, type:"monotone", dataKey:"value", stroke:"#8884d8", isAnimationActive: false}),
                    summary?null:e(Recharts.CartesianGrid, {key:"grid", hide:summary, strokeDasharray:"5 5", stroke:"#ccc"}),
                    e(Recharts.XAxis, {key:"thexs", hide:summary, dataKey:"ts",type: 'number', domain: ['auto', 'auto'],name: 'Time', tickFormatter: (unixTime) => new Date(unixTime).toLocaleTimeString(), type: "number"}),
                    e(Recharts.YAxis, {key:"theys", hide:summary, dataKey:"value", domain: ['auto', 'auto']}),
                    e(Recharts.Tooltip, {key:"tooltip", contentStyle: {backgroundColor: "black"}, labelStyle: {backgroundColor: "black"}, className:"tooltip", labelFormatter: t => new Date(t).toLocaleString()})]))];
    }

    getLabeledField() {
        return e('label', { className: `readonly ${ this.state?.graph ? 'graph' : '' }`, id: `lbl${this.id}`, key: this.id }, [
            e("div", { key: 'label', className: "label", id: `dlbl${this.id}` }, [this.props.label, this.getGraphButton()]),
            e("div", { key: 'value', className: "value", id: `vel${this.id}` }, IsDatetimeValue(this.props.name) ? "" : this.getValue(this.props.name, this.props.value)),
            this.state?.graph && this.hasStats() ? this.getReport(false) : null
        ]);
    }

    getTableField() {
        return e("div", { key: 'timevalue', className: "value", id: `vel${this.id}` }, IsDatetimeValue(this.props.name) ? "" : [this.getValue(this.props.name, this.props.value),this.getGraphButton()]);
    }

    render() {
        return this.props.label ? this.getLabeledField() : this.getTableField();
    }
}
class Table extends React.Component {
    constructor(props) {
        super(props);
        this.id = this.props.id || genUUID();
        this.state = {
            keyColumn: this.getKeyColumn()
        }
    }

    componentDidUpdate(prevProps, prevState) {
        var keycol = this.getKeyColumn();
        if (this.state.keyColumn != keycol){
            this.setState({keyColumn:keycol});
        }
    }
    getKeyColumn() {
        if (this.props.json && this.props.json.length > 0) {
            if (typeof this.props.json[0] === "object") {
                var keyCol = Object.keys(this.props.json[0])[0]
                return this.props.json.reduce((acc, cur) => acc.find(row=>row[keyCol] === cur[keyCol]) ? acc : [...acc,cur],[]).length === this.props.json.length ? keyCol : null;
            }
        }
        return undefined;
    }

    SortTable(th) {
        if (this.props.sortable){
            var table,tbody;
            Array.from((tbody=(table=th.target.closest("table")).querySelector('tbody')).querySelectorAll('tr:nth-child(n)'))
                    .sort(comparer(Array.from(th.target.parentNode.children).indexOf(th.target), this.asc = !this.asc))
                    .forEach(tr => tbody.appendChild(tr));
        }
    }

    BuildHeaderField(fld) {
        return e("th", { 
            key: `header-col-${fld}`,
            onClick: this.SortTable.bind(this)
        }, fld);
    }

    addRow(e){
        e.stopPropagation();
        e.preventDefault();
        if ((this.props.json.length == 0) || (typeof this.props.json[0] == "object")){
            this.props.json.push({});
        } else if (Array.isArray(this.props.json[0])) {
            this.props.json.push([]);
        } else {
            this.props.json.push("");
        }
        this.setState({json:this.props.json});
    }

    BuildTablePannel(){
        if (this.props.editable) {
            return e("div",{key:genUUID(),className:"popupMenu"},
                    e("div",{key:`removeprop`, onClick:elem=> this.addRow(elem)},"Add Row"));
        }
        return null;
    }

    BuildCaption(){
        if (this.props.editable) {
            return e("caption", { key: `caption` },[
                    e("div",{key:'table-panel',className:"objectButtons"}, this.BuildTablePannel()),
                    e("div",{key:`label`,className:"jsonlabel"},this.props.label)
                   ]);
        }
        return e("caption", { key: 'caption' }, `${this.props.label} - ${this.state.keyColumn}`);
    }

    getValue(fld, val) {
        if (fld === undefined || val === undefined) {
            return "";
        }
        if (val?.value !== undefined) {
            return val.value;
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

    DeleteLine(line,e) {
        e.stopPropagation();
        e.preventDefault();
        this.props.json.splice(line,1);
        this.setState({json:this.props.json});
    }

    DuplicateLine(line,e) {
        e.stopPropagation();
        e.preventDefault();
        this.props.json.push(this.props.json[line]);
        this.setState({json:this.props.json});
    }

    BuildLinePannel(idx){
        if (this.props.editable) {
            return e("div",{key:`linepanel`,className:"stackedMenu"},[
                e("div",{key:genUUID(),className:"popupMenu"},
                    e("div",{key:`del`, onClick:elem=> this.DeleteLine(idx,elem)},"Delete")),
                e("div",{key:genUUID(),className:"popupMenu"},
                    e("div",{key:`dup`, onClick:elem=> this.DuplicateLine(idx,elem)},"Duplicate"))
            ])
        }
        return null;
    }

    BuildLine(line,idx) {
        return e("tr", { key: this.getRowKey(line, idx) },[
            [
                this.props.editable?e("td", { key: `${this.getRowKey(line, idx)}-panel`, className: "readonly" },this.BuildLinePannel(idx)):null,
                this.cols.map(fld => e("td", { key: `${this.getRowKey(line, idx)}-${fld}-cell-panel`, className: "readonly" },this.BuildCell(line, fld)))
            ]
        ]);
    }

    getRowKey(line, idx) {
        return `row-${this.state.keyColumn ? line[this.state.keyColumn] : idx}`;
    }

    addString(line,fld) {
        line[fld] = {value:"",version:0};
        this.setState({json:this.props.json});
    }

    clearCell(line,fld) {
        line[fld].value?line[fld].value=null:line[fld]=null;
        this.setState({json:this.props.json});
    }

    BuildCellControlPannel(line,fld){
        if (this.props.editable) {
            var val = line[fld];
            return e("div",{key:genUUID(),className:"popupMenu"},
                        val?
                            e("div",{key:`addpropr`, onClick:elem=> this.clearCell(line,fld)},"Set Null"):
                            e("div",{key:`addpropr`, onClick:elem=> this.addString(line,fld)},"Add String"));
        }
        return null;
    }

    BuildCell(line, fld) {
        return [Array.isArray(line[fld]) ?
                      line[fld].length ? 
                        e(Table, { key: `Table-Array-Line-${this.props.path}/${fld}`, 
                                   path: `${this.props.path}/${fld}`,
                                   editable: this.props.editable, 
                                   sortable: this.props.sortable, 
                                   name: fld, 
                                   json: line[fld], 
                                   name: fld 
                                 }) : 
                        null :
                e(LocalJSONEditor, { key: `JE-${this.props.path}/${fld}`, 
                                path: `${this.props.path}/${fld}`,
                                editable: this.props.editable, 
                                json: line[fld], 
                                name: fld, 
                                registerEventInstanceCallback: this.props.registerEventInstanceCallback 
                              }),
            this.BuildCellControlPannel(line,fld)
        ];
    }

    render() {
        if (this.props.json === undefined) {
            return null;
        }
        this.cols=[];
        return e("label", { key: `label`, id: this.id, className: "table" }, 
                e("table", { key: `table`, className: "greyGridTable" }, [
                    [e("thead", { key: `head` }, 
                        e("tr", { key: `headrow` },
                        [this.props.editable?e("th", { key: `header` }):null,
                            ...this.props.json.flatMap(row => Object.keys(row))
                            .filter((val, idx, arr) => (val !== undefined) && (arr.indexOf(val) === idx))
                            .map(fld => {
                                if (!this.cols.some(col => fld == col)) {
                                    this.cols.push(fld);
                                    var val = this.getValue(fld,this.props.json[0][fld]);
                                    if (!this.sortedOn && !Array.isArray(val) && typeof val != 'object' && isNaN(this.getValue(fld,val))) {
                                        this.sortedOn = fld;
                                    }
                                }
                                return this.BuildHeaderField(fld);
                            })]
                        )), this.props.label !== undefined ? this.BuildCaption():null], 
                    e("tbody", { key: 'body' },
                        this.props.sortable ? 
                        this.props.json.sort((e1,e2)=>(this.getValue(this.sortedOn,e1[this.sortedOn])+"").localeCompare((this.getValue(this.sortedOn,e2[this.sortedOn])+"")))
                            .map(this.BuildLine.bind(this)):
                        this.props.json.map(this.BuildLine.bind(this)))
                ])
            );
    }
}
class AnalogPinConfig extends React.Component {
    constructor(props) {
        super(props);
        this.state = {pin: props.item,errors:[]};
        this.state.pin.channel = this.state.pin.channel ? this.state.pin.channel : this.pinNoToChannel(this.state.pin.pinNo);
        this.state.pin.channel_width = this.state.pin.channel_width ? this.state.pin.channel_width : 9;
        this.state.pin.channel_atten = this.state.pin.channel_atten ? this.state.pin.channel_atten : 0.0;
        this.state.pin.waitTime = this.state.pin.waitTime ? this.state.pin.waitTime : 10000;
        this.state.pin.minValue = this.state.pin.minValue ? this.state.pin.minValue : 0;
        this.state.pin.maxValue = this.state.pin.maxValue ? this.state.pin.maxValue : 4096;
    }

    componentDidUpdate(prevProps, prevState) {
        if ((this.state != prevState) && this.props.onChange) {
            this.props.onChange(this.state);
        }
        
        if (prevProps?.item != this.props?.item) {
            this.setState({pin: this.props.item});
        }
    }

    getErrors() {
        return this.state.errors.map((val,idx) => e(MaterialUI.Snackbar,{
            key:`error${idx}`, 
            className:"popuperror", 
            anchorOrigin: {vertical: "top", horizontal: "right"}, 
            autoHideDuration: 3000,
            onClose: (event, reason) => {val.visible=false; this.setState(this.state);},
            open: val.visible},e(MaterialUI.Alert,{key:`error${idx}`, severity: "error"}, val.error)));
    }

    onChange(name, value) {
        this.state.pin[name] = name == "name" ? value : name == "channel_atten" ? parseFloat(value) : parseInt(value); 
        if (name == "pinNo") {
            this.state.pin.channel = this.pinNoToChannel(this.state.pin.pinNo);
        }
        this.setState(this.state);
        if ((this.state.pin.channel_width < 9) || (this.state.pin.channel_width > 12)) {
            setTimeout(() => {this.state.errors.push({visible:true, error:`Invalid channel width for ${this.state.pin.name}, needs to bebetween 9 and 12`});this.setState(this.state)}, 300);
        }
    }

    pinNoToChannel(pinNo) {
        switch(pinNo) {
            case 32:
                return 4;
            case 33:
                return 5;
            case 34:
                return 6;
            case 35:
                return 7;
            case 36:
                return 0;
            case 37:
                return 1;
            case 38:
                return 2;
            case 39:
                return 3;
            default:
                this.state.errors.push({visible:true, error:`Invalid pin number, ${pinNo} cannot be an analog pin`});
                setTimeout(() => {this.state.errors.push({visible:true, error:`Invalid pin number, ${pinNo} cannot be an analog pin`});this.setState(this.state)}, 300);
                return -1;
        }
    }

    render() {
        return e( MaterialUI.Card, { key: this.state.pin.pinName, className: "pin-config" },[
            e( MaterialUI.CardHeader, {key:"header", title: this.state.pin.name }),
            e( MaterialUI.CardContent, {key:"details"},  e(MaterialUI.List,{key: "items"},
                [
                    e( MaterialUI.ListItem, { key: "pinName" },  
                        e( MaterialUI.TextField, { key: "pinName", autoFocus:true, value: this.state.pin.name, label: "Name", type: "text", onChange: event => this.onChange("name", event.target.value)})),
                    e( MaterialUI.ListItem, { key: "pinNo" },  
                        e( MaterialUI.TextField, { key:"pinNo", value: this.state.pin.pinNo, label: "PinNo", type: "number", onChange: event => this.onChange("pinNo", parseInt(event.target.value) )})),
                    e( MaterialUI.ListItem, { key: "channel" },  
                        e( MaterialUI.TextField, { key:"channel", inputProps:{ readOnly: true },value: this.state.pin.channel, label: "Channel", type: "number", onChange: event => this.onChange("channel", parseInt(event.target.value)) })),
                    e( MaterialUI.ListItem, { key: "channel_width" },  
                        e( MaterialUI.TextField, { key:"channel_width", value: this.state.pin.channel_width, label: "Channel Width", type: "number", min:9, max:12, onChange: event => this.onChange("channel_width", parseInt(event.target.value)) })),
                    e( MaterialUI.ListItem, { key: "channel_atten" },
                        [
                            e(MaterialUI.InputLabel,{key:"attenLabel", id:"channel_atten_label", className: "ctrllabel"}, "Channel Attennuation"),
                            e(MaterialUI.Select,{key:"attenSelect", value: this.state.pin.channel_atten, label: "Channel Attennuation", onChange: event => this.onChange("channel_atten", parseFloat(event.target.value))},[
                                e(MaterialUI.MenuItem,{key:"atten0", value: 0.0}, "0 dB - 100 mV ~ 950 mV"),
                                e(MaterialUI.MenuItem,{key:"atten1", value: 2.5}, "2.5 dB - 100 mV ~ 1250 mV"),
                                e(MaterialUI.MenuItem,{key:"atten2", value: 6.0}, "6.0 dB - 100 mV ~ 1750 mV"),
                                e(MaterialUI.MenuItem,{key:"atten3", value: 11.0}, "11.0 dB - 100 mV ~ 2450 mV"),
                            ])
                        ]),
                    e( MaterialUI.ListItem, { key: "waitTime" },  
                        e( MaterialUI.TextField, { key:"waitTime", value: this.state.pin.waitTime , label: "Wait Time", type: "number", onChange: event => this.onChange("waitTime", parseInt(event.target.value) )})),
                    e( MaterialUI.ListItem, { key: "min" },  
                        e( MaterialUI.TextField, { key:"min", value: this.state.pin.minValue , label: "Minimum", type: "number", onChange: event => this.onChange("minValue", parseInt(event.target.value) )})),
                    e( MaterialUI.ListItem, { key: "max" },  
                        e( MaterialUI.TextField, { key:"max", value: this.state.pin.maxValue , label: "Maximum", type: "number", onChange: event => this.onChange("maxValue", parseInt(event.target.value) )})),
                ]
            )),
            this.getErrors()
        ]);
    }
}class ConfigGroup extends React.Component {
    constructor(props) {
        super(props);
        this.supportedTypes = {
            analogPins:{
                caption:"Analog Pins",
                class: "AnalogPin",
                component: AnalogPinConfig
            },pins:{
                caption:"Digital Pins",
                class: "Pin",
                component: DigitalPinConfig
            }
        };
        this.state = {
            currentTab: undefined
        };
        wfetch(`${httpPrefix}/templates/config`,{
            method: 'post'
        }).then(data => data.json())
          .then(this.updateConfigTemplates.bind(this))
          .catch(console.error);
    }

    updateConfigTemplates(configTemplates) {
        this.setState({ 
            configTemplates: configTemplates,
            currentTab: configTemplates[0].collectionName
        });
    }

    render() {
        if (this.props.config && this.state.configTemplates) {
            return this.renderArrayTypes();
        } else {
            return e("div", {key: "loading"}, "Loading...");
        }
    }

    renderArrayTypes() {
        var tabs = this.state.configTemplates
            .filter(configTemplate => configTemplate.isArray);
        return [
            e(MaterialUI.Tabs, {
                value: this.state.currentTab,
                onChange: (_, v) => { this.setState({ currentTab: v }); },
                key: "ConfigTypes"
            }, [...tabs.map(this.renderTypeTab.bind(this)),
            e(MaterialUI.Tab, { key: "full-config", label: "Configuration", value: "Configuration" })]),
            tabs.map(this.renderConfigType.bind(this))
        ];
    }

    renderTypeTab(key) {
        return key.isArray ? 
                e( MaterialUI.Tab, { key: key.collectionName, label: this.supportedTypes[key.collectionName]?.caption || key.collectionName, value: key.collectionName }):
                e( MaterialUI.Tab, { key: key.class, label: this.supportedTypes[key.class]?.caption || key.class, value: key.class });
    }

    renderConfigType(key) {
        return e("div",{key:`${key.class}-control-panel`,className:`edior-pannel ${this.state.currentTab === key.collectionName ? "":"hidden"}`},[
            e("button", { key: "add", onClick: evt=> {this.props.config[key.collectionName] ? this.props.config[key.collectionName].push({}) : this.props.config[key.collectionName] = [{}]; this.props.onChange()} }, e("i",{key:"add", className:"fa fa-plus-square"})),
            e("div",{key:"items", className:`config-cards`}, this.props.config[key.collectionName] ? Object.keys(this.props.config[key.collectionName]).map(idx =>
                this.renderEditor(key,this.props.config[key.collectionName],idx)) : null)
        ]);
    }

    renderConfigItemTab(key, item, idx) {
        return e( MaterialUI.Tab, { key: item, label: idx+1, value: key });
    }

    renderEditor(key, item, idx) {
        return  e("div",{key:`${key.collectionName}-${idx}-control-editor`,className:`control-editor`},[
                    e( this.supportedTypes[key.collectionName]?.component || ConfigItem, { key: key.collectionName + idx, value: key, role: "tabpanel", item: item[idx], onChange: this.props.onChange}),
                    e("button", { key: "dup", onClick: evt=> {this.props.config[key.collectionName].push(JSON.parse(JSON.stringify(this.props.config[key.collectionName][idx]))); this.props.onChange()} }, e("i",{key:"copy", className:"fa fa-clone"})),
                    e("button", { key: "delete", onClick: evt=> {this.props.config[key.collectionName].splice(idx,1); this.props.onChange()} }, e("i",{key:"copy", className:"fa fa-trash-o"})),
                ]);
    }

    isSupported(key) {
        return Object.keys(this.supportedTypes).indexOf(key)>-1;
    }
}class ConfigItem extends React.Component {
    constructor(props) {
        super(props);
        if (JSON.stringify(props.item) === "{}") {
            Object.keys(props.value)
                  .filter(fld => !['collectionName','class','isArray'].find(val=>val===fld))
                  .forEach(fld => props.item[fld] = props.value[fld]);
        }
        this.state = { instance: props.item };
    }

    componentDidUpdate(prevProps, prevState) {
        if ((this.state != prevState) && this.props.onChange) {
            this.props.onChange(this.state.instance);
        }
    }

    getFieldType(name) {
        var obj = this.props.item[name];
        if (obj !== undefined) {
            if (typeof(obj) === 'boolean') {
                return "boolean";
            }
            if (isNaN(obj)) {
                return "text";
            }
            return "number";
        }
        return "text";
    }

    parseFieldValue(value) {
        if (value !== undefined) {
            if (isNaN(value)) {
                return value;
            }
            if (typeof value === "string") {
                if (value.indexOf(".") != -1) {
                    return parseFloat(value);
                }
                return parseInt(value);
            }
        }
        return value;
    }

    onChange(name, value) {
        this.props.item[name] = this.parseFieldValue(value); 
        this.setState(this.state);
    }

    getFieldWeight(field) {
        if (field == this.props.nameField) {
            return 100;
        }

        switch(this.getFieldType(field, this.props.item[field])) {
            case "text":
                return 75;
            case "number":
                return 50;
            default:
                return 25;
        }
    }

    render() {
        return e( MaterialUI.Card, { key: this.props.item[this.props.nameField], className: "config-item" },[
            e( MaterialUI.CardHeader, {key:"header", title: this.props.item[this.props.nameField] }),
            e( MaterialUI.CardContent, {key:"details"},  e(MaterialUI.List,{key: "items"},
                Object.keys(JSON.stringify(this.props.item) === "{}" ? this.props.value : this.props.item)
                      .filter(fld => !['collectionName','class','isArray'].find(val=>val===fld))
                      .sort((a,b) => {
                        var wa = this.getFieldWeight(a);
                        var wb = this.getFieldWeight(b);
                        if (wa == wb) {
                            return a.localeCompare(b);
                        }
                        return wb - wa;
                    }).map(this.getEditor.bind(this))
            ))
        ]);
    }

    getEditor(key) {
        var tp = this.getFieldType(key);
        return e(MaterialUI.ListItem, { key: key },
            tp === "boolean" ? 
            e(MaterialUI.FormControlLabel,{
                key:"label",
                label: key,
                control:e(MaterialUI.Checkbox, {
                    key: "ctrl",
                    checked: this.parseFieldValue(this.props.item[key]),
                    onChange: event => this.onChange(key, event.target.checked)
                })
            }):
            e(MaterialUI.TextField, {
                key: key,
                autoFocus: tp == "text",
                value: this.parseFieldValue(this.props.item[key]),
                label: key,
                type: tp,
                onChange: event => this.onChange(key, event.target.value)
            })
        );
    }
}class DigitalPinConfig extends React.Component {
    constructor(props) {
        super(props);
        this.state = {pin: props.item};
    }

    componentDidUpdate(prevProps, prevState) {
        if ((this.state != prevState) && this.props.onChange) {
            this.props.onChange(this.state);
        }
        
        if (prevProps?.item != this.props?.item) {
            this.setState({pin: this.props.item});
        }
    }

    onChange(name, value) {
        this.state.pin[name] = name == "pinName" ? value : parseInt(value); 
        this.setState(this.state);
    }

    render() {
        return e( MaterialUI.Card, { key: this.state.pin.pinName, className: "pin-config" },[
            e( MaterialUI.CardHeader, {key:"header", title: this.state.pin.pinName }),
            e( MaterialUI.CardContent, {key:"details"},  e(MaterialUI.List,{key: "items"},
                [
                    e( MaterialUI.ListItem, { key: "pinName" },  
                        e( MaterialUI.TextField, { key: "pinName", autoFocus:true, value: this.state.pin.pinName, label: "Name", type: "text", onChange: event => this.onChange("pinName", event.target.value)})),
                    e( MaterialUI.ListItem, { key: "pinNo" },  
                        e( MaterialUI.TextField, { key:"pinNo", value: this.state.pin.pinNo, label: "PinNo", type: "number", onChange: event => this.onChange("pinNo", event.target.value) })),
                    e( PinDriverFlags, { key: "driverFlags", autoFocus:true, value: this.state.pin.driverFlags, onChange: val => this.onChange("driverFlags", val) }),
                ]
            ))
        ]);
    }
}class IRReceiver extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      ...props.config
    };
    if (!this.state.timing_groups) {
      this.state.timing_groups = this.getDefaultTimingGroup();
    }
  }

  buildTimingGroup(
    tag,
    carrier_freq_hz,
    duty_cycle,
    bit_length,
    invert,
    header_mark_us,
    header_space_us,
    one_mark_us,
    one_space_us,
    zero_mark_us,
    zero_space_us
  ) {
    return {
      tag: tag,
      carrier_freq_hz: carrier_freq_hz,
      duty_cycle: duty_cycle,
      bit_length: bit_length,
      invert: invert,
      header_mark_us: header_mark_us,
      header_space_us: header_space_us,
      one_mark_us: one_mark_us,
      one_space_us: one_space_us,
      zero_mark_us: zero_mark_us,
      zero_space_us: zero_space_us,
    };
  }

  getDefaultTimingGroup() {
    return [
      this.buildTimingGroup(
        "HiSense",
        38000,
        33,
        28,
        0,
        9000,
        4200,
        650,
        1600,
        650,
        450
      ),
      this.buildTimingGroup(
        "NEC",
        38000,
        33,
        32,
        0,
        9000,
        4250,
        560,
        1690,
        560,
        560
      ),
      this.buildTimingGroup(
        "LG",
        38000,
        33,
        28,
        0,
        9000,
        4200,
        550,
        1500,
        550,
        550
      ),
      this.buildTimingGroup(
        "samsung",
        38000,
        33,
        32,
        0,
        4600,
        4400,
        650,
        1500,
        553,
        453
      ),
      this.buildTimingGroup(
        "LG32",
        38000,
        33,
        32,
        0,
        4500,
        4500,
        500,
        1750,
        500,
        560
      ),
    ];
  }

  getTimingGroupTableHeader() {
    return e(
      MaterialUI.TableHead,
      { key: "IRTrackerTableHeader" },
      e(
        MaterialUI.TableRow,
        { key: "IRTrackerTableRow" },
        Object.keys(this.state.timing_groups[0]).map((key) =>
          e(MaterialUI.TableCell, { key: key }, key)
        )
      )
    );
  }
  updateTableProperty(event, record, field) {
    if (isNaN(event.target.value)) {
        record[field] = event.target.value;
    } else {
        record[field] = parseInt(event.target.value);
    }
    console.log(this.state);
  }

  getTimingGroupTableBody() {
    return e(
      MaterialUI.TableBody,
      { key: "IRTrackerTableBody" },
      this.state.timing_groups.map((group,idx) =>
        e(
          MaterialUI.TableRow,
          { key: group.tag + idx },
          Object.keys(group).map((key) =>
            e(MaterialUI.TableCell, { key: key },
                e(MaterialUI.TextField, {
                key: key,
                onChange: evt => this.updateTableProperty(evt, group, key),
                defaultValue: group[key]
            })
          )
        )
      )
    ));
  }

  getTimingGroupTable() {
    return e(
      MaterialUI.TableContainer,
      { key: "IRTrackerTableContainer" },
      e(MaterialUI.Table, { key: "IRTrackerTable" }, [
        this.getTimingGroupTableHeader(),
        this.getTimingGroupTableBody()
      ])
    );
  }

  updateProperty(event) {
    if (isNaN(event.target.value)) {
        this.state[event.target.labels[0].outerText] = event.target.value;
    } else {
        this.state[event.target.labels[0].outerText] = parseInt(event.target.value);
    }
  }

  Apply() {
    Object.keys(this.state).forEach((key) => {
        this.props.config[key] = this.state[key]
    });
    this.props.saveChanges();
  }

  render() {
    return e(
      MaterialUI.Card,
      { key: "IRTracker", variant: "outlined" },
      e(MaterialUI.CardContent, { key: "IRTrackerContent" }, [
        e(
          MaterialUI.Typography,
          { key: "IRTrackerTitle", variant: "h5" },
          e("span", { key: "IRTrackerTitleText" }, "IR Receiver"),
          e("span", { key: "filler", className:"filler" }),
          e(MaterialUI.Button, {key:"Apply", variant:"outlined", onClick: this.Apply.bind(this)}, "Apply")
        ),
        e(
          MaterialUI.Typography,
          { key: "IRTrackerSubtitle", variant: "body1" },
          "Configure the IR Receiver"
        ),
        e(MaterialUI.Divider, { key: "IRTrackerDivider" }),
        e(MaterialUI.CardContent, { key: "IRTrackerContent" }, [
          e(
            MaterialUI.Box,
            { key: "IRTrackerStack", className: "config-fields" },
            [
              Object.keys(this.state)
                    .filter((key) => !Array.isArray(this.state[key]))
                    .filter((key) => typeof this.state[key] !== "object")
                    .map((key) => {
                return e(MaterialUI.TextField, {
                  key: key,
                  label: key,
                  type: "number",
                  onChange: this.updateProperty.bind(this),
                  defaultValue: this.state[key],
                });
              }),
            ]
          ),
          e(
            MaterialUI.Box,
            { key: "IRTrackerStackTimingGroups", className: "timing-groups" },
            this.getTimingGroupTable()
          ),
        ]),
      ])
    );
  }
}
class PinDriverFlags extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            digital_in: { value: props.value &  0b00000001 ? true : false, errors:[] },
            digital_out: { value: props.value & 0b00000010 ? true : false, errors:[] },
            pullup: { value: props.value &      0b00000100 ? true : false, errors:[] },
            pulldown: { value: props.value &    0b00001000 ? true : false, errors:[] },
            touch: { value: props.value &       0b00010000 ? true : false, errors:[] },
            wakeonhigh: { value: props.value &  0b00100000 ? true : false, errors:[] },
            wakeonlow: { value: props.value &   0b01000000 ? true : false, errors:[] }
        };
    }

    getValue(state) {
        return state.digital_in.value | 
               state.digital_out.value << 1 | 
               state.pullup.value << 2 | 
               state.pulldown.value << 3 | 
               state.touch.value << 4 | 
               state.wakeonhigh.value << 5 | 
               state.wakeonlow.value << 6;
    }

    getFlagNames(value) {
        return [
            value &  0b00000001 ? "digital_in" : "",
            value & 0b00000010 ? "digital_out" : "",
            value &      0b00000100 ? "pullup" : "",
            value &    0b00001000 ? "pulldown" : "",
            value &       0b00010000 ? "touch" : "",
            value &  0b00100000 ? "wakeonhigh" : "",
            value &   0b01000000 ? "wakeonlow" : ""
        ].filter(x => x.length > 0).join(", ");
    }

    componentDidUpdate(prevProps, prevState) {
        if (this.props.value != this.getValue(this.state)) {
            this.props.onChange(this.getValue(this.state));
        }
    }

    isValidChange(name, value) {
        var newState =  JSON.parse(JSON.stringify(this.state));
        newState[name].value = value;
        if ((name == "digital_in" || name == "digital_out") && value) {
            if ((name == "digital_in") && newState.digital_out.value) {
                this.state.digital_out.value = false;
            }
            if ((name == "digital_out") && newState.digital_in.value) {
                this.state.digital_in.value = false;
                this.state.pullup.value = false;
                this.state.pulldown.value = false;
                this.state.touch.value = false;
                this.state.wakeonhigh.value = false;
                this.state.wakeonlow.value = false;
            }
        }
        if (newState.digital_out.value && this.getValue(newState) & 0b01111100) {
            this.state[name].errors.push({visible: true, error: "Can't set " + this.getFlagNames(this.getValue(newState) & 0b01111100) + " option for output pins"});
            new Promise((resolve,reject) => resolve(this.setState(this.state)));
            return false;
        }

        if (newState.touch.value && (this.getValue(newState) & 0b00001100)) {
            this.state[name].errors.push({visible: true, error: "Can't set " + this.getFlagNames(this.getValue(newState) & 0b00001100) + " option for touch pins"});
            new Promise((resolve,reject) => resolve(this.setState(this.state)));
            return false;
        }
        return true;
    }

    onChange(name, value) {
        if (this.isValidChange(name, value)) {
            this.state[name].value = value;
            this.props.onChange(this.getValue(this.state));
            return true;
        }
        return false;
    }

    getErrors(value) {
        return value.errors.map((val,idx) => e(MaterialUI.Snackbar,{
            key:`error${idx}`, 
            className:"popuperror", 
            anchorOrigin: {vertical: "top", horizontal: "right"}, 
            autoHideDuration: 3000,
            onClose: (event, reason) => {val.visible=false; this.setState(this.state);},
            open: val.visible},e(MaterialUI.Alert,{key:`error${idx}`, severity: "error"}, val.error)));
    }

    renderOption(name, value) {    
        return [
            e(MaterialUI.FormControlLabel,{
                key:name,
                className:"driverflag",
                label: name,
                control:e(MaterialUI.Checkbox, {
                    key: "ctrl",
                    checked: value.value,
                    onChange: event => this.onChange(name, event.target.checked)
                })
            }),
            ...this.getErrors(value)
        ];
    }

    render() {
        return e( MaterialUI.Card, { key: "driver-flags", className: "driver-flags" },[
            e( MaterialUI.CardHeader, {key:"header", subheader: "Flags" }),
            e( MaterialUI.CardContent, {key:"details"},  e(MaterialUI.List,{key: "items"}, Object.keys(this.state).map(name => this.renderOption(name, this.state[name]))))
        ]);
    }
}class StatusPage extends React.Component {
    constructor(props) {
        super(props);
        this.mounted=false;
        this.state={
            refreshRate:"Manual"
        }
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

    getRefreshRate() {
        if (this.state.refreshRate.indexOf("secs")>0) {
            return Number(this.state.refreshRate.replace(/([0-9]+).*/,"$1"))*1000
        } 
        return Number(this.state.refreshRate.replace(/([0-9]+).*/,"$1"))*60000
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (this.state.refreshRate !== prevState.refreshRate) {
            if (this.refreshTimer) {
                clearInterval(this.refreshTimer);
            }

            if (this.state.refreshRate !== "Manual"){
                this.refreshTimer = setInterval(()=>{this.updateAppStatus()},this.getRefreshRate());
            }
        }
    }

    updateAppStatus() {
        this.updateStatuses([{ url: "/status/" }, { url: "/status/app" }, { url: "/status/tasks", path: "tasks" }, { url: "/status/mallocs", path: "mallocs" }, { url: "/status/repeating_tasks", path: "repeating_tasks" }], {});
    }

    refreshStatus(stat) {
        if (this.mounted){
            if (stat){
                this.filterProperties(stat).then(stat=>{
                    const flds = Object.keys(stat);
                    var status = this.state?.status || {};
                    for (const fld in flds) {
                        status[flds[fld]] = stat[flds[fld]];    
                    }
                    this.setState({status:status});
                });
            } else {
                this.updateAppStatus();
            }
        }
    }

    filterProperties(props) {
        return new Promise((resolve,reject)=>{
            resolve(Object.keys(props).filter(fld=> !fld.match(/^MALLOC_.*/) || props[fld]!=0).reduce((ret,fld)=>{ret[fld]=props[fld];return ret},{}));
        });
    }

    updateStatuses(requests, newState) {
        if (window.location.host || httpPrefix){
            var abort = new AbortController()
            var timer = setTimeout(() => abort.abort(), 4000);
            if (this.props.selectedDeviceId == "current") {
                this.updateStatus(requests.pop(), abort, newState).then(res => {
                    clearTimeout(timer);
                    document.getElementById("Status").style.opacity = 1;
                    if (this.mounted){
                        if (requests.length > 0) {
                            this.updateStatuses(requests, newState);
                        } else {
                            this.setState({
                                error: null,
                                status: this.orderResults(newState)
                            });                        }
                        }
                }).catch(err => {
                    document.getElementById("Status").style.opacity = 0.5
                    clearTimeout(timer);
                    if (err.code != 20) {
                        var errors = requests.filter(req => req.error);
                        document.getElementById("Status").style.opacity = 0.5
                        if (errors[0]?.waitFor) {
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

    updateStatus(request, abort, newState) {
        return new Promise((resolve, reject) => wfetch(`${httpPrefix}${request.url}`, {
            method: 'post',
            signal: abort.signal
        }).then(data => data.json())
            .then(this.filterProperties.bind(this))
            .then(jstats => {
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
            }));
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
                e("div",{key: "buttonbar", className: "buttonbar"},[
                    e("button", { key: "refresh", onClick: elem => this.updateAppStatus() }, "Refresh"),
                    e( MaterialUI.FormControl,{key: "refreshRate"},[
                        e(MaterialUI.InputLabel,{
                            key: "label",
                            className: "label",
                            id: "stat-refresh-label"
                        },"Refresh Rate"),
                        e(MaterialUI.Select,{
                            key:"options",
                            id: "stat-refresh",
                            labelId:"stat-refresh-label", 
                            label: "Refresh Rate",
                            value: this.state.refreshRate,
                            onChange: elem => this.setState({refreshRate:elem.target.value})
                        },["Manual", "2 secs", "5 secs", "10 secs","30 secs","1 mins","5 mins","10 mins","30 mins","60 mins"].map((term,idx)=>e(MaterialUI.MenuItem,{key:idx,value:term},term)))
                    ])
                ]),
                e(LocalJSONEditor, {
                    key: 'StateViewer', 
                    path: '/',
                    json: this.state.status, 
                    editable: false,
                    sortable: true,
                    selectedDeviceId: this.props.selectedDeviceId,
                    registerStateCallback: this.props.registerStateCallback,
                    registerEventInstanceCallback: this.props.registerEventInstanceCallback
                })
            ];
        } else {
            return e("div",{key:genUUID()},"Loading...");
        }
    }
}
class ConfigPage extends React.Component {
    componentDidMount() {
        if (this.isConnected()) {
            var abort = new AbortController()
            var timer = setTimeout(() => abort.abort(), 4000);
            wfetch(`${httpPrefix}/config/${this.props.selectedDeviceId=="current"?"":this.props.selectedDeviceId+".json"}`, {
                    method: 'post',
                    signal: abort.signal
                }).then(resp => resp.json())
                .then(config => {
                    clearTimeout(timer);
                    this.setState({
                        config: fromVersionedToPlain(config),
                        newconfig: fromVersionedToPlain(config),
                        original: config
                    });
                });
        }
    }

    buildEditor() {
        try {
            if (!this.jsoneditor) {
                this.jsoneditor = new JSONEditor(this.container, {
                    onChangeJSON: json => this.state.newconfig = json
                }, this.state.config);
            } else {
                this.jsoneditor.set(this.state.config);
            }
        } catch (err) {
            this.nativejsoneditor = e(LocalJSONEditor, {
                key: 'ConfigEditor',
                path: '/',
                json: this.state.config,
                selectedDeviceId: this.props.selectedDeviceId,
                editable: true
            });
        }
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (this.state?.config && !this.nativejsoneditor && !this.jsoneditor) {
            this.buildEditor();
        }
    }

    getEditor() {
        return [
            e("div", { key: 'fancy-editor', ref: (elem) => this.container = elem, id: `${this.props.id || genUUID()}`, "data-theme": "spectre" }),
            this.nativejsoneditor
        ]
    }

    getEditorGroups() {
        return e(ConfigGroup, { key: "configGroups", config: this.state?.newconfig, onChange: (_) => {this.jsoneditor.set(this.state.newconfig); this.setState(this.state) } });
    }

    saveChanges() {
        var abort = new AbortController()
        var timer = setTimeout(() => abort.abort(), 4000);
        wfetch(`${httpPrefix}/config`, {
                method: 'put',
                signal: abort.signal,
                body: JSON.stringify(fromPlainToVersionned(this.state.newconfig, this.state.original))
            }).then(resp => resp.json())
            .then(fromVersionedToPlain)
            .then(config => {
                clearTimeout(timer);
                this.setState({ config: config });
            }).catch(console.err);
    }

    render() {
        if (this.isConnected() && this.state?.config) {
            return [e("div", { key: 'button-bar', className: "button-bar" }, [
                        e("button", { key: "refresh", onClick: elem => this.componentDidMount() }, "Refresh"),
                        e("button", { key: "save", onClick: this.saveChanges.bind(this) }, "Save"),
                    ]),
                    this.getEditorGroups(),
                    this.getEditor()
                   ];
        } else {
            return e("div", { key: genUUID() }, "Loading....");
        }
    }

    isConnected() {
        return window.location.host || httpPrefix;
    }
}class TypedField extends React.Component {
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
            return e("summary",{key: "summary" ,className: "liveEvent"},
                    e("div",{key: "description" ,className: "description"},[
                        e("div",{key: "base" ,className: "eventBase"},"Loading"),
                        e("div",{key: "id" ,className: "eventId"},"...")
                    ]));
        }
    }

    renderComponent(event) {
        return e("summary",{key: "event" ,className: "liveEvent"},[
            e("div",{key: "description" ,className: "description"},[
                e("div",{key: "base" ,className: "eventBase"},event.eventBase),
                e("div",{key: "id" ,className: "eventId"},event.eventId)
            ]), event.data ? e("details",{key: "details" ,className: "data"},this.parseData(event)): null
        ]);
    }

    parseData(props) {
        if (props.dataType == "Number") {
            return e("div", { key: props.data.name, className: "description" }, 
                        e("div", { key: "data", className: "propNumber" }, props.data)
                    );
        }
        return Object.keys(props.data)
            .filter(prop => typeof props.data[prop] != 'object' && !Array.isArray(props.data[prop]))
            .map(prop => e("div", { key: prop, className: "description" }, [
                e("div", { key: "name", className: "propName" }, prop),
                e("div", { key: "data", className: prop }, props.data[prop])
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
        this.state = {filters:{}};
    }

    componentDidMount() {
        this.mounted=true;
    }

    componentWillUnmount() {
        this.mounted=false;
    }

    ProcessEvent(evt) {
        if (evt && this.mounted && this.isEventVisible(evt)) {
            var lastEvents = (this.state?.lastEvents||[]).concat(evt);
            while (lastEvents.length > 100) {
                lastEvents.shift();
            }
            this.setState({
                filters: this.updateFilters(lastEvents),
                lastEvents: lastEvents
            });
        }
    }

    updateFilters(lastEvents) {
        var curFilters = Object.entries(lastEvents
            .filter(evt => evt && evt.eventBase && evt.eventId)
            .reduce((ret, evt) => {
                if (!ret[evt.eventBase]) {
                    ret[evt.eventBase] = { visible: true, eventIds: [{ visible: true, eventId: evt.eventId }] };
                } else if (!ret[evt.eventBase].eventIds.find(vevt => vevt.eventId === evt.eventId)) {
                    ret[evt.eventBase].eventIds.push({ visible: true, eventId: evt.eventId });
                }
                return ret;
            }, {}));
        Object.values(curFilters).forEach(filter => {
            if (!this.state.filters[filter[0]]) {
                this.state.filters[filter[0]] = filter[1];
            } else if (filter[1].eventIds.find(newEvt => !this.state.filters[filter[0]].eventIds.find(eventId => eventId.eventId === newEvt.eventId))) {
                this.state.filters[filter[0]].eventIds = this.state.filters[filter[0]].eventIds.concat(filter[1].eventIds.filter(eventId => !this.state.filters[filter[0]].eventIds.find(eventId2 => eventId.eventId === eventId2.eventId)));
            }
        });
        return this.state.filters;
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

    updateEventIdFilter(eventId, enabled) {
        eventId.visible = enabled;
        this.setState({filters: this.state.filters});
    }

    updateEventBaseFilter(eventBase, enabled) {
        eventBase.visible = enabled;
        this.setState({filters: this.state.filters});
    }

    filterPanel() {
        return e("div",{key: "filterPanel", className:"filterPanel"},[
                e("div",{key:"filters",className:"filters"},[
                    e("div",{ key: "label", className:"header"}, `Filters`),
                    e("div",{key:"filterlist",className:"filterlist"},Object.entries(this.state.filters).map(this.renderFilter.bind(this)))
                ]),
                e("div", { key: "control", className:"control" }, [
                    this.state?.lastEvents ? e("div",{ key: "label", className:"header"}, `${this.state.lastEvents.length}/${this.state.lastEvents.filter(this.isEventVisible.bind(this)).length} event${this.state?.lastEvents?.length?'s':''}`) : "No events",
                    e("button",{ key: "clearbtn", onClick: elem => this.setState({lastEvents:[]})},"Clear")
                ])
               ]);
    }

    renderFilter(filter) {
        return e('div', { key: filter[0], className: `filter ${filter[0]}` },
            e("div",{key:"evbfiltered",className:"evbfiltered"},
                e(MaterialUI.FormControlLabel,{
                    key:"visible",
                    className:"ebfiltered",
                    label: filter[0],
                    control:e(MaterialUI.Checkbox, {
                        key: "ctrl",
                        checked: filter[1].visible,
                        onChange: event => this.updateEventIdFilter(filter[1], event.target.checked)
                    })})
            ),
            e("div", { key: "filterList", className: `eventIds` }, filter[1].eventIds.map(eventId => e("div", { key: eventId.eventId, className: `filteritem ${eventId.eventId}` }, [
                e("div",{key:"evifiltered",className:"evifiltered"},
                    e(MaterialUI.FormControlLabel,{
                        key:eventId.eventId,
                        className:"eifiltered",
                        label: eventId.eventId,
                        control:e(MaterialUI.Checkbox, {
                            key: "ctrl",
                            checked: eventId.visible,
                            onChange: event => this.updateEventIdFilter(eventId, event.target.checked),
                        })}
                    )
                )
            ]))));
    }

    isEventVisible(event) {
        return event && ((Object.keys(this.state.filters).length === 0) || !this.state.filters[event.eventBase]?.eventIds?.some(eventId=> eventId.eventId === event.eventId)) || 
               (Object.keys(this.state.filters).some(eventBase => event.eventBase === eventBase && this.state.filters[eventBase].visible) &&
               Object.keys(this.state.filters).some(eventBase => event.eventBase === eventBase && 
                                                                  this.state.filters[eventBase].eventIds
                                                                    .some(eventId=> eventId.eventId === event.eventId && eventId.visible)));
    }

    getFilteredEvents() {
        return e("div", { key: "eventList", className: "eventList" }, 
                this.state?.lastEvents?.filter(this.isEventVisible.bind(this))
                           .map((event, idx) => e(LiveEvent, { key: idx, event: this.parseEvent(event) })).reverse());
    }

    render() {
        return  e("div", { key: "eventPanel" ,className: "eventPanel" }, [
            this.filterPanel(),
            this.getFilteredEvents()
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
                e(LiveEventPannel,{ key: "eventpannel",registerEventCallback:this.props.registerEventCallback})
            ];
        } else {
            return e("div",{key: "loading"},"Loading.....");
        }
    }
}
class FirmwareUpdater extends React.Component {
    constructor(props) {
        super(props);
        this.state = {};
    }

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
                var md5 = CryptoJS.algo.SHA256.create();
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
                console.log(JSON.stringify(this.state.md5));
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
}class TripWithin extends React.Component {
    getIconColor() {
        if (!this.state?.points) {
            return "#00ff00";
        } else if (this.props.points.length) {
            return "aquamarine";
        } else {
            return "#ff0000";
        }
    }

    getIcon() {
        return e("svg", { key: "svg", xmlns: "http://www.w3.org/2000/svg", viewBox: "0 0 64 64",onClick:elem=>this.setState({viewer:"trip"}) }, [
            e("path", { key: "path1", style: { fill: this.getIconColor() }, d: "M17.993 56h20v6h-20zm-4.849-18.151c4.1 4.1 9.475 6.149 14.85 6.149 5.062 0 10.11-1.842 14.107-5.478l.035.035 1.414-1.414-.035-.035c7.496-8.241 7.289-20.996-.672-28.957A20.943 20.943 0 0 0 27.992 2a20.927 20.927 0 0 0-14.106 5.477l-.035-.035-1.414 1.414.035.035c-7.496 8.243-7.289 20.997.672 28.958zM27.992 4.001c5.076 0 9.848 1.976 13.437 5.563 7.17 7.17 7.379 18.678.671 26.129L15.299 8.892c3.493-3.149 7.954-4.891 12.693-4.891zm12.696 33.106c-3.493 3.149-7.954 4.892-12.694 4.892a18.876 18.876 0 0 1-13.435-5.563c-7.17-7.17-7.379-18.678-.671-26.129l26.8 26.8z" }),
            e("path", { key: "path2", style: { fill: this.getIconColor() }, d: "M48.499 2.494l-2.828 2.828c4.722 4.721 7.322 10.999 7.322 17.678s-2.601 12.957-7.322 17.678S34.673 48 27.993 48s-12.957-2.601-17.678-7.322l-2.828 2.828C12.962 48.983 20.245 52 27.993 52s15.031-3.017 20.506-8.494c5.478-5.477 8.494-12.759 8.494-20.506S53.977 7.97 48.499 2.494z" })
        ]);
    }

    render() {
        return e("div",{key:"renderedtrip",className:"rendered trip"},[
            this.getIcon(),
            this.state?.viewer == "trip"?e(TripViewer,{key:"tripviewer",points:this.props.points,cache:this.props.cache,onClose:()=>this.setState({"viewer":""})}):null
        ]
        );
    }
}
class FileViewer extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            renderers: []
        }
    }

    componentDidMount() {
        if (this.props.registerFileWork) {
            this.props.registerFileWork(this.buildRenderers(0));
        }
    }

    buildRenderers(retryCount) {
        return new Promise((resolve, reject) => {
            if (this.props.name.endsWith(".csv") && !this.state?.renderers?.some("trip")){
                this.setState({renderers:[{name:"loading"}]});
                this.parseCsv(resolve, retryCount, reject);
            } else if (this.props.name.endsWith(".log") && !this.state?.renderers?.some("trip")){
                this.setState({renderers:[{name:"loading"}]});
                this.parseLog(resolve, retryCount, reject);
            } else {
                resolve();
            }
        });
    }

    parseLog(resolve, retryCount, reject) {
        wfetch(`${httpPrefix}${this.props.folder}/${this.props.name}`)
            .then(resp => resp.text())
            .then(content => resolve(this.setState({
                renderers: [{
                    name: "trip",
                    points: content.split("\n")
                        .filter(ln => ln.match(/[ID] \([0-9]{2}:[0-9]{2}:[0-9]{2}[.][0-9]{3}\).* Location:.*/))
                        .map(ln => ln.match(/[ID] \((?<timestamp>.*)\).*Location:\s*(?<latitude>[0-9.-]+),\s*(?<longitude>[0-9.-]+).*speed:\s*(?<speed>[0-9.]+).*altitude:\s*(?<altitude>[0-9.]+).*course:\s*(?<course>[0-9.]+).*bat:\s*(?<Battery>[0-9.]+)/i)?.groups)
                        .filter(point => point)
                        .map(point => {
                            Object.keys(point).forEach(fld => point[fld] = isNaN(point[fld]) ? point[fld] : parseFloat(point[fld]));
                            point.timestamp = `1970-01-01 ${point.timestamp}`;
                            point.altitude *= 100;
                            point.speed *= 100;
                            return point;
                        })
                }]
            }))
            ).catch(err => {
                if (retryCount++ < 3) {
                    this.buildRenderers(retryCount);
                } else {
                    reject(err);
                }
            });
    }

    parseCsv(resolve, retryCount, reject) {
        wfetch(`${httpPrefix}${this.props.folder}/${this.props.name}`)
            .then(resp => resp.text())
            .then(content => {
                var cols = content.split(/\n|\r\n/)[0].split(",");
                resolve(this.setState({
                    renderers: [{
                        name: "trip",
                        points: content.split(/\n|\r\n/)
                            .splice(1).map(ln => {
                                var ret = {};
                                ln.split(",").forEach((it, idx) => ret[cols[idx]] = isNaN(it) ? it : parseFloat(it));
                                return ret;
                            }).filter(item => item.timestamp && item.timestamp.match(/[0-9]{4}\/[0-9]{2}\/[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}/))
                    }]
                }));
            }).catch(err => {
                if (retryCount++ < 3) {
                    this.buildRenderers(retryCount);
                } else {
                    reject(err);
                }
            });
    }

    getRenderers() {
        if (!this.state?.renderers || !this.state.renderers.length) {
            return null;
        }
        return this.state.renderers.map(renderer => {
            switch (renderer.name) {
                case "trip":
                    return renderer.points.length?e(TripWithin,{key:"tripwithin",points:renderer.points,cache:this.props.cache}):null;

                case "loading":
                    return e('i',{className: "rendered reportbtn fa fa-spinner", key: "graphbtn", })
    
                default:
                    return null;
            }
        });
    }

    render() {
        return [e("a", { key:"filelink", href: `${httpPrefix}${this.props.folder}/${this.props.name}` }, this.props.name.split('/').reverse()[0]),this.getRenderers()];
    }
}

class SFile extends React.Component {
    render() {
        return  e("tr", { key: "tr", className: this.props.file.ftype }, [
            e("td", { key: "link" }, this.getLink(this.props.file)),
            e("td", { key: "size" }, this.props.file.ftype != "file" ? "" : this.props.file.size),
            e("td", { key: "delete" }, this.getDeleteLink())]);
    }

    getDeleteLink() {
        return this.props.path == "/" || this.props.file.name == ".." ? null : e("a", {
            key: "delete",
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
                key: file.name,
                href: "#",
                onClick: () => this.props.onChangeFolder ? this.props.onChangeFolder(`${file.folder || "/"}${file.name == ".." ? "" : "/" + file.name}`.replaceAll("//", "/")):null
            }, file.name);
        } else {
            return e(FileViewer,{key:file.name,cache:this.props.cache,registerFileWork:this.props.registerFileWork,...file});
        }
    }
}

class StorageViewer extends React.Component {
    constructor(props) {
        super(props);
        this.state = { 
            loaded: false, 
            path: "/", 
            files: null,
            cache:{images:{}},
            fileWork: [] 
        };
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
        if (this.mounted){
            fileStatsToFetch.forEach(fileToFetch => {
                this.registerFileWork(new Promise((resolve, reject) => {
                    wfetch(`${httpPrefix}/stat${fileToFetch.folder}/${fileToFetch.name}`, {
                        method: 'post'
                    }).then(data => {
                        data.json().then(jdata => {
                            fileToFetch.size = jdata.size;
                            resolve(this.setState({ loaded: true, files: this.state.files, total: this.state?.total + jdata.size }));
                        });
                    }).catch(ex => {
                        reject(ex);
                    });
                }));
            });
        }
    }

    fetchFiles() {
        if (window.location.host || httpPrefix){
            var abort = new AbortController();
        
            var quitItNow = setTimeout(() => abort.abort(), 3000);
            wfetch(`${httpPrefix}/files` + this.state.path, {
                method: 'post',
                signal: abort.signal
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
        return e("thead", { key: "head" }, e("tr", { key: "row" }, ["Name", "Size"].map(col => e("th", { key: col, onClick: this.SortTable.bind(this) }, col)).concat(e("th", { key: "op" }, "Op"))));
    }

    registerFileWork(fn) {
        this.setState({ fileWork: [...this.state.fileWork,fn] });
    }

    render() {
        if (!this.state?.files) {
            return e("div", { key: "Loading" }, "Loading......");
        } else {
            return e("div", { key: "Files", 
                className: `file-table ${this.state.loaded?"":"loading"}` }, 
                    e("table", { key: "table", className: "greyGridTable" }, [
                        e("caption", { key: "caption" }, this.state.path),
                        this.getTableHeader(),
                        e("tbody", { key: "body" }, 
                            this.getSystemFolders().concat(this.state.files).filter(file => file).map(file => 
                                e(SFile,{ 
                                    key: file.name, 
                                    file:file, 
                                    cache: this.state.cache,
                                    path:this.state.path,
                                    registerFileWork: this.registerFileWork.bind(this),
                                    onChangeFolder: (folder) => this.setState({path:folder, files:[]}),
                                    OnDelete: ()=>this.fetchFiles()
                                }))
                            ),
                        e("tfoot", { key: "tfoot" }, e("tr", { key: "trow" }, [
                            e("td", { key: "totallbl" }, "Total"), 
                            e("td", { key: "total" }, this.state.total)
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
        var logLn = this.props.logln.substr(this.props.logln.indexOf(func) + func.length + 2).replaceAll(/^[\r\n]*/g, "").replaceAll(/[^0-9a-zA-Z ]\[..\n.*/g, "");

        return e("div", { key: "logLine" , className: `log LOG${lvl}` }, [
            e("div", { key: "level", ref: ref => this.logdiv = ref, className: "LOGLEVEL" }, lvl),
            e("div", { key: "date", className: "LOGDATE" }, msg.substr(3, 12)),
            e("div", { key: "source", className: "LOGFUNCTION" }, func),
            e("div", { key: "message", className: "LOGLINE" }, logLn)
        ]);
    }
}

class LogLines extends React.Component {
    constructor(props) {
        super(props);
        this.state={logLines:[],logLevels:{}};
        if (this.props.registerLogCallback) {
            this.props.registerLogCallback(this.AddLogLine.bind(this));
        }
    }

    AddLogLine(logln) {
        if (this.state.logLines.some(clogln => clogln === logln)) {
            return;
        }

        var recIdx=-1;
        var msg = logln.match(/^[^IDVEW]*(.+)/)[1];
        var lvl = msg.substr(0, 1);
        var ts = msg.substr(3, 12);
        var func = msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");

        if (this.state.logLines.some((clogln,idx) => {
            var nmsg = clogln.match(/^[^IDVEW]*(.+)/)[1];
            var nlvl = nmsg.substr(0, 1);
            var nts = nmsg.substr(3, 12);
            var nfunc = msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");
            return nlvl == lvl && nts == ts && nfunc == func;
        })) {
            recIdx=this.state.logLines.length;
        }
        while (this.state.logLines.length > 500) {
            this.state.logLines.shift();
        }

        if (this.state.logLevels[lvl] === undefined) {
            this.state.logLevels[lvl] = {visible:true};
        }

        if (this.state.logLevels[lvl][func] === undefined) {
            this.state.logLevels[lvl][func] = true;
        }

        if (recIdx >= 0) {
            this.state.logLines[recIdx] = logln;
        } else {
            this.state.logLines= [...(this.state?.logLines||[]),logln];
        }
        this.setState(this.state);
    }

    renderLogFunctionFilter(lvl,func,logLines) {    
        return e(MaterialUI.FormControlLabel,{
            key:"ffiltered" + lvl + func,
            className:"effiltered",
            label: `${func} (${logLines.filter(logln => logln.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1") == func).length})`,
            control:e(MaterialUI.Checkbox, {
                key: "ctrl",
                checked: this.state.logLevels[lvl][func],
                onChange: event => {this.state.logLevels[lvl][func] = event.target.checked; this.setState(this.state);}
            })});
    }

    renderLogLevelFilterControl(lvl,logLines) {   
        return e(MaterialUI.FormControlLabel,{
            key:"lfiltered" + lvl,
            className:"elfiltered",
            label: `Log Level ${lvl}(${logLines.length})`,
            control:e(MaterialUI.Checkbox, {
                key: "ctrl",
                checked: this.state.logLevels[lvl].visible,
                onChange: event => {this.state.logLevels[lvl].visible = event.target.checked; this.setState(this.state);}
            })});
    }

    renderLogFunctionFilters(lvl, logLines) {
        return Object.keys(this.state.logLevels[lvl])
                     .filter(func => func !== "visible")
                     .map(func => {
            return this.renderLogFunctionFilter(lvl,func,logLines)
        })
    }

    renderLogLevelFilter(lvl,logLines) {
        return e("div", { key: "logLevelFilter" + lvl, className: "logLevelFilter" }, [
            e("div", { key: "logLevelFilterTitle" + lvl, className: "logLevelFilterTitle" }, this.renderLogLevelFilterControl(lvl,logLines)), 
            e("div", { key: "logLevelFilterContent" + lvl, className: "logLevelFilterContent" }, 
                this.renderLogFunctionFilters(lvl, logLines))
        ]);
    }

    renderLogLevelFilters() {
        return Object.keys(this.state.logLevels).map(lvl => {
            return this.renderLogLevelFilter(lvl,this.state.logLines.filter(logln => logln.match(/^[^IDVEW]*(.+)/)[1].substr(0, 1) == lvl))
        })
    }

    renderFilterPannel() {
        return e("div", { key: "filterPannel", className: "filterPannel" }, [
            e("div", { key: "filterPannelTitle",className: "filterPannelTitle" }, "Filters"),
            e("div", { key: "filterPannelContent",className: "filterPannelContent" }, this.renderLogLevelFilters())
        ]);
    }

    renderControlPannel() {
        return e("div", { key: "logControlPannel", className: "logControlPannel" }, [
            e("button", { key: "clear", onClick: elem => this.setState({ logLines: [] }) }, "Clear Logs"),
            this.renderFilterPannel()
        ]);
    }

    isLogVisible(logLine) {
        var msg = logLine.match(/^[^IDVEW]*(.+)/)[1];
        var lvl = msg.substr(0, 1);
        var func = msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");
        if (this.state.logLevels[lvl].visible && this.state.logLevels[lvl][func]) {
            return true;
        }
        return false;
    }

    render() {
        return e("div", { key: "logContainer", className: "logContainer" },[
            this.renderControlPannel(),
            e("div", { key: "logLines", className: "loglines" }, this.state?.logLines ? this.state.logLines.filter(this.isLogVisible.bind(this)).map((logln,idx) => e(LogLine,{ key: idx, logln:logln})):null)            
        ]);
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
            e("div", { key: "logpannel", className: "logpannel" }, [
                e("button", { key: "reboot", onClick: elem => this.SendCommand({ 'command': 'reboot' }) }, "Reboot"),
                e("button", { key: "parseFiles", onClick: elem => this.SendCommand({ 'command': 'parseFiles' }) }, "Parse Files"),
                e("button", { key: "factoryReset", onClick: elem => this.SendCommand({ 'command': 'factoryReset' }) }, "Factory Reset"),
                e(FirmwareUpdater, { key: "firmwareUpdater" })
            ]),
        ];
    }
}
class TripViewer extends React.Component {
    constructor(props) {
        super(props);
        this.state={
            cache:this.props.cache,
            zoomlevel:15,
            latitude:0,
            longitude:0
        };
        this.testing = false;
    }

    componentDidMount() {
        this.getTripTiles();
        this.firstRender = true;
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (!prevProps.points || !prevProps.points.length || !prevProps.points.length || prevProps.points.length != this.props.points.length){
            this.firstRender=true;
        }
        this.mapCanvas = this.mapWidget.getContext("2d");                    
        this.tripCanvas = this.tripWidget.getContext("2d");                    
        this.popupCanvas = this.popupWidget.getContext("2d");                    
        this.popupWidget.addEventListener("mousemove", this.mouseEvent.bind(this));
        window.requestAnimationFrame(this.drawMap.bind(this));
    }

    drawMap() {
        if (!this.mapWidget || !this.props.points || !this.props.points.length || this.state.closed){
            return;
        } else {
            this.drawTripVisibleTiles();
            this.drawTrip();
            this.drawPointPopup();
        }
    }
   
    getTripTiles() {
        var lastPoint=undefined;
        this.state.points=this.props.points.map(this.pointToCartesian.bind(this))
                              .filter(point => point.latitude && point.longitude)
                              .filter(point => point.latitude > -90 && point.latitude < 90)
                              .filter(point => point.longitude > -180 && point.longitude < 180)
                              .filter(point => {
                                  if (lastPoint === undefined) {
                                      lastPoint = point;
                                      return true;
                                  }
                                  if (Math.abs(lastPoint.longitude - point.longitude) > 1.0) {
                                      return false;
                                  }
                                  lastPoint = point;
                                  return true;
                              });
        this.state.trip={
                leftTile: this.state.points.reduce((a,b)=>a<b.tileX?a:b.tileX,99999),
                rightTile: this.state.points.reduce((a,b)=>a>b.tileX?a:b.tileX,-99999),
                bottomTile: this.state.points.reduce((a,b)=>a<b.tileY?a:b.tileY,99999),
                topTile: this.state.points.reduce((a,b)=>a>b.tileY?a:b.tileY,-99999)
        };
        this.state.maxXTiles= Math.ceil(window.innerWidth/256)+1;
        this.state.maxYTiles= Math.ceil(window.innerHeight/256)+1;
        this.state.trip.XTiles = this.state.trip.rightTile-this.state.trip.leftTile+1;
        this.state.trip.YTiles = this.state.trip.topTile-this.state.trip.bottomTile+1;
        this.state.margin = {
            left: this.state.maxXTiles>this.state.trip.XTiles?Math.floor((this.state.maxXTiles-this.state.trip.XTiles)/2):0,
            bottom: this.state.maxYTiles>this.state.trip.YTiles?Math.floor((this.state.maxYTiles-this.state.trip.YTiles)/2):0
        };
        this.state.margin.right=this.state.maxXTiles>this.state.trip.XTiles?this.state.maxXTiles-this.state.trip.XTiles-this.state.margin.left:0;
        this.state.margin.top= this.state.maxYTiles>this.state.trip.YTiles?this.state.maxYTiles-this.state.trip.YTiles-this.state.margin.bottom:0;
        this.state.leftTile=Math.max(0,this.state.trip.leftTile-this.state.margin.left);
        this.state.rightTile=this.state.trip.rightTile+this.state.margin.right;
        this.state.bottomTile=Math.max(0,this.state.trip.bottomTile-this.state.margin.bottom);
        this.state.topTile=this.state.trip.topTile + this.state.margin.top;
        this.state.leftlatitude = this.state.points.reduce((a,b)=>a<b.latitude?a:b.latitude,99999);
        this.state.rightlatitude = this.state.points.reduce((a,b)=>a>b.latitude?a:b.latitude,-99999);
        this.state.toplongitude = this.state.points.reduce((a,b)=>a<b.longitude?a:b.longitude,99999);
        this.state.bottomlongitude = this.state.points.reduce((a,b)=>a>b.longitude?a:b.longitude,-99999);
        this.state.latitude= this.state.leftlatitude+(this.state.rightlatitude-this.state.leftlatitude)/2;
        this.state.longitude= this.state.toplongitude+(this.state.bottomlongitude-this.state.toplongitude)/2;
        this.state.windowTiles = {
            center: {
                tileX: this.lon2tile(this.state.longitude, this.state.zoomlevel),
                tileY: this.lat2tile(this.state.latitude, this.state.zoomlevel),
            }
        }
        this.state.windowTiles.leftTile = this.state.windowTiles.center.tileX - Math.floor(this.state.maxXTiles/2);
        this.state.windowTiles.rightTile = this.state.windowTiles.center.tileX + Math.floor(this.state.maxXTiles/2);
        this.state.windowTiles.bottomTile = this.state.windowTiles.center.tileY - Math.floor(this.state.maxYTiles/2);
        this.state.windowTiles.topTile = this.state.windowTiles.center.tileY + Math.floor(this.state.maxYTiles/2);
        this.state.windowTiles.XTiles = this.state.windowTiles.rightTile-this.state.windowTiles.leftTile+1;
        this.state.windowTiles.YTiles = this.state.windowTiles.topTile-this.state.windowTiles.bottomTile+1;
        this.setState(this.state);
    }

    drawTripVisibleTiles() {
        return new Promise(async (resolve,reject)=>{
            this.popupWidget.width=(this.state.rightTile-this.state.leftTile)*256;
            this.popupWidget.height=(this.state.topTile-this.state.bottomTile)*256;
            this.mapWidget.width=(this.state.rightTile-this.state.leftTile)*256;
            this.mapWidget.height=(this.state.topTile-this.state.bottomTile)*256;
            this.tripWidget.width=(this.state.rightTile-this.state.leftTile)*256;
            this.tripWidget.height=(this.state.topTile-this.state.bottomTile)*256;

            this.mapCanvas.fillStyle = "black";
            this.mapCanvas.fillRect(0,0,window.innerWidth,window.innerHeight);
            var wasFirstRender=this.firstRender;
            
            if (this.firstRender) {
                this.firstRender=false;
                const elementRect = this.mapWidget.getBoundingClientRect();
                this.mapWidget.parentElement.scrollTo((elementRect.width/2)-512, (elementRect.height/2)-256);
            }

            for (var tileX = this.state.windowTiles.leftTile; tileX <= this.state.windowTiles.rightTile; tileX++) {
                for (var tileY = this.state.windowTiles.bottomTile; tileY <= this.state.windowTiles.topTile; tileY++) {
                    if (!this.props.cache.images[this.state.zoomlevel] ||
                        !this.props.cache.images[this.state.zoomlevel][tileX] || 
                        !this.props.cache.images[this.state.zoomlevel][tileX][tileY]){
                        await this.addTileToCache(tileX, tileY).catch(reject);
                    } else {
                        await this.getTileFromCache(tileX, tileY).catch(reject);
                    }        
                }
            }
            if (wasFirstRender) {
                new Promise((resolve,reject)=>{
                    for (var tileX = this.state.trip.leftTile; tileX <= this.state.trip.rightTile; tileX++) {
                        for (var tileY = this.state.trip.bottomTile; tileY <= this.state.trip.topTile; tileY++) {
                            if (!this.props.cache.images[this.state.zoomlevel] ||
                                !this.props.cache.images[this.state.zoomlevel][tileX] || 
                                !this.props.cache.images[this.state.zoomlevel][tileX][tileY]){
                                this.addTileToCache(tileX, tileY).catch(reject);
                            }        
                        }
                    }
                    resolve();
                });
            }
            resolve();
        });
    }

    drawPointPopup(){
        if (this.focused) {
            this.popupCanvas.clearRect(0,0,this.popupWidget.width,this.popupWidget.height);
            this.popupCanvas.fillStyle = "transparent";
            this.popupCanvas.fillRect(0,0,window.innerWidth,window.innerHeight);
    
            this.popupCanvas.font = "12px Helvetica";
            var txtSz = this.popupCanvas.measureText(new Date(`${this.focused.timestamp} UTC`).toLocaleString());
            var props =  Object.keys(this.focused)
                               .filter(prop => prop != "timestamp" && !prop.match(/.*tile.*/i));

            var boxHeight=50 + (9*props.length);
            var boxWidth=txtSz.width+10;
            this.popupCanvas.strokeStyle = 'green';
            this.popupCanvas.shadowColor = '#00ffff';
            this.popupCanvas.fillStyle = "#000000";
            this.popupCanvas.lineWidth = 1;
            this.popupCanvas.shadowBlur = 2;
            this.popupCanvas.beginPath();
            this.popupCanvas.rect(this.getClientX(this.focused),this.getClientY(this.focused)-boxHeight,boxWidth,boxHeight);
            this.popupCanvas.fill();
            this.popupCanvas.stroke();

            this.popupCanvas.fillStyle = 'rgba(00, 0, 0, 1)';
            this.popupCanvas.strokeStyle = '#000000';

            this.popupCanvas.beginPath();
            var ypos = this.getClientY(this.focused)-boxHeight+txtSz.actualBoundingBoxAscent+3;
            var xpos = this.getClientX(this.focused)+3;

            this.popupCanvas.strokeStyle = '#97ea44';
            this.popupCanvas.shadowColor = '#ffffff';
            this.popupCanvas.fillStyle = "#97ea44";
            this.popupCanvas.lineWidth = 1;
            this.popupCanvas.shadowBlur = 2;
            this.popupCanvas.fillText(new Date(`${this.focused.timestamp} UTC`).toLocaleString(),xpos,ypos);
            ypos+=5;

            ypos+=txtSz.actualBoundingBoxAscent+3;
            props.forEach(prop => {
                var propVal = this.getPropValue(prop,this.focused[prop]);
                this.popupCanvas.strokeStyle = 'aquamarine';
                this.popupCanvas.shadowColor = '#ffffff';
                this.popupCanvas.fillStyle = "aquamarine";
                this.popupCanvas.fillText(`${prop}: `,xpos,ypos);
                txtSz = this.popupCanvas.measureText(propVal);
                this.popupCanvas.strokeStyle = '#97ea44';
                this.popupCanvas.shadowColor = '#ffffff';
                this.popupCanvas.fillStyle = "#97ea44";
                this.popupCanvas.fillText(propVal,(xpos+(boxWidth-txtSz.width)-5),ypos);
                ypos+=txtSz.actualBoundingBoxAscent+3;
            });

            this.popupCanvas.stroke();
        }
    }

    drawTrip() {
        return new Promise((resolve,reject)=>{
            var firstPoint = this.state.points[0];
            if (firstPoint){
                this.tripCanvas.fillStyle = "transparent";
                this.tripCanvas.fillRect(0,0,window.innerWidth,window.innerHeight);
    
                this.tripCanvas.strokeStyle = '#00ffff';
                this.tripCanvas.shadowColor = '#00ffff';
                this.tripCanvas.fillStyle = "#00ffff";
                this.tripCanvas.lineWidth = 2;
                this.tripCanvas.shadowBlur = 2;
        
                this.tripCanvas.beginPath();
                this.tripCanvas.moveTo(this.getClientX(firstPoint), this.getClientY(firstPoint));
                this.state.points.forEach(point => {
                    this.tripCanvas.lineTo(this.getClientX(point), this.getClientY(point));
                });
                this.tripCanvas.stroke();
                this.tripCanvas.strokeStyle = '#0000ff';
                this.tripCanvas.shadowColor = '#0000ff';
                this.tripCanvas.fillStyle = "#0000ff";
                this.state.points.forEach(point => {
                    this.tripCanvas.beginPath();
                    this.tripCanvas.arc(((point.tileX - this.state.leftTile) * 256) + (256 * point.posTileX), ((point.tileY - this.state.bottomTile) * 256) + (256 * point.posTileY), 5, 0, 2 * Math.PI);
                    this.tripCanvas.stroke();
                });
                resolve(this.state);
            } else {
                reject({error:"no points"});
            }
        });
    }

    mouseEvent(event) {
        var margin = 10;
        var focused = this.state.points.find(point => 
            (this.getClientX(point) >= (event.offsetX - margin)) &&
            (this.getClientX(point) <= (event.offsetX + margin)) &&
            (this.getClientY(point) >= (event.offsetY - margin)) &&
            (this.getClientY(point) <= (event.offsetY + margin)));
        if (focused != this.focused){
            this.focused=focused;
            window.requestAnimationFrame(this.drawPointPopup.bind(this));
        }
    }

    getPropValue(name,val) {
        if (name == "speed") {
            return `${Math.floor(1.852*val/100.0)} km/h`;
        }
        if (name == "altitude") {
            return `${Math.floor(val/100)}m`;
        }
        if ((name == "longitude") || (name == "latitude")) {
            return val;
        }
        return Math.round(val);
    }

    addTileToCache(tileX,tileY) {
        return new Promise((resolve,reject) => {
            this.getTile(tileX, tileY, 0).then(imgData => {
                var tileImage = new Image();
                tileImage.posX = tileX - this.state.leftTile;
                tileImage.posY = tileY - this.state.bottomTile;
                tileImage.src = (window.URL || window.webkitURL).createObjectURL(imgData);
                if (!this.props.cache.images[this.state.zoomlevel]) {
                    this.props.cache.images[this.state.zoomlevel] = {};
                }
                if (!this.props.cache.images[this.state.zoomlevel][tileX]) {
                    this.props.cache.images[this.state.zoomlevel][tileX] = {};
                }
                this.props.cache.images[this.state.zoomlevel][tileX][tileY] = tileImage;

                tileImage.onload = (elem) => {
                    resolve(this.mapCanvas.drawImage(tileImage, tileImage.posX * tileImage.width, tileImage.posY * tileImage.height));
                };
            }).catch(reject);
        });
    }

    getTileFromCache(tileX,tileY) {
        return new Promise((resolve,reject) => {
            var tileImage = this.props.cache.images[this.state.zoomlevel][tileX][tileY];
            try {
                resolve(this.mapCanvas.drawImage(tileImage, tileImage.posX * tileImage.width, tileImage.posY * tileImage.height));
            } catch (err) {
                this.props.cache.images[this.state.zoomlevel][tileX][tileY] = null;
                reject(err);
            }
        });
    }

    downloadTile(tileX,tileY) {
        return new Promise((resolve,reject) => {
            var newImg;
            wfetch(`https://tile.openstreetmap.de/${this.state.zoomlevel}/${tileX}/${tileY}.png`)
                .then(resp => resp.blob())
                .then(imgData => wfetch(`${httpPrefix}/sdcard/web/tiles/${this.state.zoomlevel}/${tileX}/${tileY}.png`,{
                    method: 'put',
                    body: (newImg=imgData)
                }).then(resolve(newImg)).catch(resolve(newImg)))
                  .catch(reject);
        })
    }

    drawWatermark(tileX,tileY) {
        return new Promise((resolve,reject) => {
            return this.mapWidget.toBlob(resolve);
        })
    }

    getTile(tileX,tileY, retryCount) {
        return new Promise((resolve,reject)=>{
            if (this.testing) {
                return resolve(this.drawWatermark(tileX,tileY));
            }
            wfetch(`${httpPrefix}/sdcard/web/tiles/${this.state.zoomlevel}/${tileX}/${tileY}.png`)
                .then(resp => resolve(resp.status >= 300? this.downloadTile(tileX,tileY):resp.blob()))
                .catch(err => {
                    if (retryCount > 3) {
                        reject({error:`Error in downloading ${this.state.zoomlevel}/${tileX}/${tileY}`});
                    } else {
                        resolve(this.getTile(tileX,tileY,retryCount++));
                    }
                });
        });
    }

    getClientY(firstPoint) {
        return ((firstPoint.tileY - this.state.bottomTile) * 256) + (256 * firstPoint.posTileY);
    }

    getClientX(firstPoint) {
        return ((firstPoint.tileX - this.state.leftTile) * 256) + (256 * firstPoint.posTileX);
    }

    lon2tile(lon) { 
        return Math.floor(this.lon2x(lon)); 
    }
    
    lat2tile(lat)  { 
        return Math.floor(this.lat2y(lat));
    }

    lon2x(lon) { 
        return (lon+180)/360*Math.pow(2,this.state.zoomlevel); 
    }
    
    lat2y(lat)  { 
        return (1-Math.log(Math.tan(lat*Math.PI/180) + 1/Math.cos(lat*Math.PI/180))/Math.PI)/2 *Math.pow(2,this.state.zoomlevel); 
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
            tileX: this.lon2tile(point.longitude, this.state.zoomlevel),
            tileY: this.lat2tile(point.latitude, this.state.zoomlevel),
            posTileX: this.lon2x(point.longitude,this.state.zoomlevel) - this.lon2tile(point.longitude, this.state.zoomlevel),
            posTileY: this.lat2y(point.latitude,this.state.zoomlevel) - this.lat2tile(point.latitude, this.state.zoomlevel),
            ...point
        };
    }

    closeIt(){
        this.setState({closed:true});
        if (this.props.onClose) {
            this.props.onClose();
        }
    }

    render(){
        return e("div",{key:"trip",className:`lightbox ${this.state?.closed?"closed":"opened"}`},[
            e("div",{key:"control"},[
                e("div",{key:"tripHeader",className:"tripHeader"},[
                    `Trip with ${this.props.points.length} points`,
                    e("br",{key:"daterange"}),
                    `From ${new Date(`${this.props.points[0].timestamp} UTC`).toLocaleString()} `,
                    `To ${new Date(`${this.props.points[this.props.points.length-1].timestamp} UTC`).toLocaleString()} Zoom:`,
                    e("select",{
                        key:"zoom",
                        onChange: elem=>this.setState({zoomlevel:elem.target.value}),
                        value: this.state.zoomlevel
                    },
                        Array.from(Array(18).keys()).map(zoom => e("option",{key:zoom},zoom+1))
                    )
                ]),
                e("span",{key:"close",className:"close",onClick:this.closeIt.bind(this)},"X")
            ]),
            e("div",{key:"map",className:"trip"},[
                e("canvas",{
                    key:"mapWidget",
                    className:"mapCanvas",
                    ref: (elem) => this.mapWidget = elem
                }),
                e("canvas",{
                    key:"tripWidget",
                    className:"mapCanvas",
                    ref: (elem) => this.tripWidget = elem
                }),
                e("canvas",{
                    key:"popupWidget",
                    className:"mapCanvas",
                    ref: (elem) => this.popupWidget = elem
                })
            ])
        ]);
    }
}
var app=null;

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
        System:  {active: false},
        Logs:    {active: false},
        Events:  {active: false}
        },
        autoRefresh: (httpPrefix||window.location.hostname) ? true : false
      };
    this.callbacks={stateCBFn:[],logCBFn:[],eventCBFn:[]};

    if (!httpPrefix && !window.location.hostname){
      this.lookForDevs();
    }
    if (this.state?.autoRefresh && (httpPrefix || window.location.hostname)) {
      this.openWs();
    }
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
    var RTCPeerConnection = /*window.RTCPeerConnection ||*/ window.webkitRTCPeerConnection || window.mozRTCPeerConnection;
    if (RTCPeerConnection)(() => {  
      var rtc = new RTCPeerConnection({  
          iceServers: []  
      });  
      if (1 || window.mozRTCPeerConnection) {  
          rtc.createDataChannel('', {  
              reliable: false  
          });  
      };  
      rtc.onicecandidate = (evt) => {  
          if (evt.candidate) parseLine.bind(this)("a=" + evt.candidate.candidate);  
      };  
      rtc.createOffer(function (offerDesc) {  
        offerDesc.sdp.split('\r\n').forEach(parseLine.bind(this));
        rtc.setLocalDescription(offerDesc);  
      }.bind(this),(e) => {  
          console.warn("offer failed", e);  
      });  
      var addrs = Object.create(null);  
      addrs["0.0.0.0"] = false;  

      function parseLine(line) {
        if (~line.indexOf("a=candidate")) {
          var parts = line.split(' '), addr = parts[4], type = parts[7];
          if (type === 'host')
            console.log(addr);
        } else if (~line.indexOf("c=")) {
          var parts = line.split(' '), addr = parts[2].split("."), addrtyoe = parts[1];
          if (addr[0] === '0') {
            addr[0]=192;
            addr[1]=168;
            addr[2]=0;
          }

          if ((addrtyoe === "IP4") && addr[0]) {
            this.state.lanDevices = [];
            for (var idx = 254; idx > 0; idx--) {
              addr[3] = idx;
              this.state.lanDevices.push(addr.join("."));
            }
            this.state.lanDevices = this.state.lanDevices.sort(() => .5 - Math.random());
            var foundDevices = [];
            for (var idx = 0; idx < Math.min(10, this.state.lanDevices.length); idx++) {
              if (this.state.lanDevices.length) {
                this.scanForDevices(this.state.lanDevices, foundDevices);
              }
            }
          }
        }
      }

    }).bind(this)();  
    else {  
        document.getElementById('list').innerHTML = "<code>ifconfig| grep inet | grep -v inet6 | cut -d\" \" -f2 | tail -n1</code>";  
        document.getElementById('list').nextSibling.textContent = "In Chrome and Firefox your IP should display automatically, by the power of WebRTCskull.";  
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
                    shadowColor: '#000000',
                    fillColor: '#000000',
                    textColor: '#00ffff',
                    startY: 30,
                    renderer: this.drawSprite
                })
                window.requestAnimationFrame(this.drawDidget.bind(this));
              }
              dev.ip=device;
              foundDevices.push(dev);
              if (!httpPrefix){
                httpPrefix=`http://${foundDevices[0].ip}`
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
        window.requestAnimationFrame(this.drawDidget.bind(this));
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
    this.startWs();
  }

  startWs() {
    var ws = this.ws = new WebSocket("ws://" + (httpPrefix == "" ? `${window.location.hostname}:${window.location.port}` : httpPrefix.substring(7)) + "/ws");
    var stopItWithThatShit = setTimeout(() => { console.log("Main timeout"); ws.close(); this.state.connecting = false; }, 3500);
    ws.onmessage = (event) => {
      stopItWithThatShit = this.processMessage(stopItWithThatShit, event, ws);
    };
    ws.onopen = () => {
      stopItWithThatShit = this.wsOpen(stopItWithThatShit, ws);
    };
    ws.onerror = (err) => {
      this.wsError(err, stopItWithThatShit, ws);
    };
    ws.onclose = (evt => {
      this.wsClose(stopItWithThatShit);
    });
  }

  wsClose(stopItWithThatShit) {
    clearTimeout(stopItWithThatShit);
    this.state.connected = false;
    this.state.connecting = false;
    window.requestAnimationFrame(this.drawDidget.bind(this));
    if (this.state.autoRefresh)
      this.openWs();
  }

  wsError(err, stopItWithThatShit, ws) {
    console.error(err);
    clearTimeout(stopItWithThatShit);
    this.state.error = err;
    window.requestAnimationFrame(this.drawDidget.bind(this));
    ws.close();
  }

  wsOpen(stopItWithThatShit, ws) {
    clearTimeout(stopItWithThatShit);
    this.state.connected = true;
    this.state.connecting = false;
    ws.send("Connected");
    window.requestAnimationFrame(this.drawDidget.bind(this));
    stopItWithThatShit = setTimeout(() => { this.state.timeout = "Connect"; ws.close(); console.log("Connect timeout"); }, 3500);
    return stopItWithThatShit;
  }

  processMessage(stopItWithThatShit, event, ws) {
    clearTimeout(stopItWithThatShit);
    if (!this.state.running || this.state.timeout) {
      this.state.running = true;
      this.state.error = null;
      this.state.timeout = null;
    }

    if (event && event.data) {
      if (event.data[0] == "{") {
        try {
          if (event.data.startsWith('{"eventBase"')) {
            this.ProcessEvent(fromVersionedToPlain(JSON.parse(event.data)));
          } else {
            this.UpdateState(fromVersionedToPlain(JSON.parse(event.data)));
          }
        } catch (e) {
        }
      } else if (event.data.match(/.*\) ([^:]*)/g)) {
        this.AddLogLine(event.data);
      }
    } else {
      this.ProcessEvent(undefined);
    }
    stopItWithThatShit = setTimeout(() => { this.state.timeout = "Message"; ws.close(); console.log("Message timeout"); }, 4000);
    return stopItWithThatShit;
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
            animGroups[agn].forEach(anim => {
                this.drawSprite(anim, canvas);
            });
        }

        this.anims = this.anims.filter(anim => anim.state != 2);
        window.requestAnimationFrame(this.drawDidget.bind(this));
      }
  }

  drawSprite(anim, canvas) {
    if (!anim.state) {
        anim.state = 1;
        anim.x = anim.from == "chip" ? this.chipX : this.browserX;
        anim.endX = anim.from == "chip" ? this.browserX : this.chipX;
        anim.direction=anim.from=="browser"?1:-1;
        anim.y = anim.startY;
        anim.tripLen = 1600.0;
        anim.startWindow = performance.now();
        anim.endWindow = anim.startWindow + anim.tripLen;
    } else {
        anim.x += (anim.direction*((performance.now() - anim.startWindow))/anim.tripLen);
    }
    var width=Math.min(15,4 + (anim.weight));
    if ((anim.y-(width/2)) <= 0) {
      anim.y = width;
    }
    canvas.beginPath();
    canvas.lineWidth = 2;
    canvas.shadowBlur = 2;
    canvas.strokeStyle = anim.lineColor;
    canvas.shadowColor = anim.fillColor;
    canvas.fillStyle = anim.fillColor;
    canvas.moveTo(anim.x+width, anim.y);
    canvas.arc(anim.x, anim.y, width, 0, 2 * Math.PI);
    canvas.fill();
    canvas.stroke();

    if (anim.weight > 1) {
      var today = ""+anim.weight
      canvas.font = Math.min(20,8+anim.weight)+"px Helvetica";
      var txtbx = canvas.measureText(today);
      canvas.fillStyle=anim.textColor;
      canvas.strokeStyle = anim.textColor;
      canvas.fillText(today, anim.x - txtbx.width / 2, anim.y + txtbx.actualBoundingBoxAscent / 2);
    }    
    
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
    canvas.shadowColor = '#002222';
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
    canvas.shadowColor = '#000000';
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
      var msg = ln.match(/^[^IDVEW]*(.+)/)[1];
      var lvl = msg.substr(0, 1);
      this.anims.push({
            type:"log",
            from: "chip",
            level:lvl,
            weight: 1,
            lineColor: lvl == 'D' || lvl == 'I' ? "green" : ln[0] == 'W' ? "yellow" : "red",
            shadowColor: '#000000',
            fillColor: '#000000',
            textColor: lvl == 'D' || lvl == 'I' ? "green" : ln[0] == 'W' ? "yellow" : "red",
            startY: 25,
            renderer: this.drawSprite
        })
        window.requestAnimationFrame(this.drawDidget.bind(this));
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
        weight: 1,
        lineColor: '#00ffff',
        shadowColor: '#000000',
        fillColor: '#000000',
        textColor: '#00ffff',
        startY: 5,
        renderer: this.drawSprite
      });
      window.requestAnimationFrame(this.drawDidget.bind(this));
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
          eventBase: event ? event.eventBase : undefined,
          weight: 1,
          lineColor: '#00ffff',
          shadowColor: '#000000',
          fillColor: '#000000',
          textColor: '#7fffd4',
          startY: 15,
          renderer: this.drawSprite
      });
      window.requestAnimationFrame(this.drawDidget.bind(this));
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
      return e(StatusPage,  { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Status"].active, registerEventInstanceCallback:this.registerEventInstanceCallback.bind(this), registerStateCallback:this.registerStateCallback.bind(this) });
    }
    if (name == "Config") {
      return e(ConfigPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Config"].active });
    }
    if (name == "System") {
      return e(SystemPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["System"].active });
    }
    if (name == "Logs") {
      return e(LogLines, { key: "logLines", registerLogCallback:this.registerLogCallback.bind(this), active: this.state.tabs["System"].active })
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
    return e("div",{key:genUUID(),className:"mainApp"}, [
      Object.keys(this.state.tabs).map(tab => 
        e("details",{key:genUUID(),id:tab, className:"appPage slides", open: this.state.tabs[tab].active, onClick:elem=>{
            if (elem.target.classList.contains("appTab")){
              elem.target.parentElement.setAttribute("open",true);
              Object.keys(this.state.tabs).forEach(ttab => ttab == tab ? this.state.tabs[ttab].active=true : this.state.tabs[ttab].active=false );
              [].slice.call(elem.target.parentElement.parentElement.children).filter(ttab => ttab != elem).forEach(ttab => ttab.removeAttribute("open"))
            }
          }},
          [
            e("summary",{key:genUUID(),className:"appTab"},tab),
            e("div",{key:genUUID(),className: tab == "Status" ? "pageContent system-config" : "pageContent"}, this.getPage(tab))
          ]
        )
      ),
      e("canvas",{
        key: genUUID(),
        height: 40,
        width:100,
        ref: (elem) => this.widget = elem
      }),
      e('fieldset', { key: genUUID(), className:`slides`, id: "controls"}, [
        this.state?.OnLineDevices?.length && !window.location.hostname ?
          e("select",{
              key: genUUID(),
              className: "landevices",
              value: httpPrefix.substring(7),
              onChange: elem=>{httpPrefix=`http://${elem.target.value}`;this.state?.httpPrefix!=httpPrefix?this.setState({httpPrefix:httpPrefix}):null;this.ws?.close();}
              },this.state.OnLineDevices.map(lanDev=>e("option",{
                  key:genUUID(),
                  className: "landevice"
              },lanDev.ip))
            ):null,
        e(DeviceList, {
            key: genUUID(),
            selectedDeviceId: this.state.selectedDeviceId,
            httpPrefix: httpPrefix,
            onSet: val=>this.state?.selectedDeviceId!=val?this.setState({selectedDeviceId:val}):null
        })
      ])
    ]);
  }
}

// ReactDOM.render(
//   e(MainApp, {
//     key: genUUID(),
//     className: "slider"
//   }),
//   document.querySelector(".slider")
// );

ReactDOM.createRoot(document.querySelector(".slider")).render(e(MainApp, {
  key: genUUID(),
  className: "slider"
}));
function getInSpot(anims, origin) {
  return anims
        .filter(anim => anim.from == origin)
        .find(anim => (anim.x === undefined) || (origin == "browser" ? anim.x <= (app.browserX + 4 + anim.weight) : anim.x >= (app.chipX - 4- anim.weight)));
}

