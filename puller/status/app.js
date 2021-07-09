'use strict';

const e = React.createElement;

var pageControler = new AbortController();
var ws = null;
var fileStats = {};
var fileStatsToFetch = []
var lastFolder = "";
var activeTab = null;
var activeInterval = null;
var editor = null;
var stateManager = null;
var stateViewer = null;
var fileManager = null;
var logs = document.getElementById("slide-4").getElementsByClassName("loglines")[0];

const httpPrefix = ""; //"http://192.168.1.107";

//#region REACTJS

class BoolInput extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            checked: this.props.initialState ? this.props.initialState() : false
        };

        this.id = this.props.id || genUUID();
    }

    toggleChange = (elem) => {
        this.setState({
            checked: elem.target.checked
        });
        elem.target.checked ?
            this.props.onOn ? this.props.onOn(elem.target) : null :
            this.props.onOff ? this.props.onOff(elem.target) : null;
    }

    render() {
        return e("label", { key: genUUID(), className: "editable", id: `lbl${this.id}`, key: this.id },
            e("div", { key: genUUID(), className: "label", id: `dlbl${this.id}` }, this.props.label),
            e("input", { key: genUUID(), type: "checkbox", onChange: this.toggleChange, id: `in${this.id}`, checked: this.state.checked }));
    }
}
class IntInput extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            value: this.props.value
        };

        this.id = this.props.id || genUUID();
    }

    toggleChange = (elem) => {
        this.setState({
            value: elem.target.value
        });
    }

    render() {
        return e("label", { key: genUUID(), className: "editable", id: `lbl${this.id}`, key: this.id },
            e("div", { key: genUUID(), className: "label", id: `div${this.id}` }, this.props.label),
            e("input", { key: genUUID(), type: "number", value: this.state.value, onChange: this.toggleChange, id: `${this.id}` }));
    }
}

class ControlPannel extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            periodicRefreshed: false,
            refreshFrequency: 10,
            autoRefresh: false
        };

        this.id = this.props.id || genUUID();
    }

    render() {
        return e('fieldset', { key: genUUID(), id: "controls", key: this.id }, [
            e('legend', { key: genUUID(), id: `lg${this.id}` }, 'Controls'),
            e(BoolInput, { key: genUUID(), label: "Periodic Refresh", onOn: PeriodicRefreshClicked, onOff: PeriodicRefreshClicked }),
            e(IntInput, { key: genUUID(), label: "Freq(sec)", value: 10, id: "refreshFreq" }),
            e(BoolInput, { key: genUUID(), label: "Auto Refresh", onOn: setupWs, onOff: setupWs, id: "autorefresh", initialState: () => { return ws != null } })
        ]);
    }
}

class ROProp extends React.Component {
    constructor(props) {
        super(props);
        this.state = this.props.json;

        this.id = this.props.id || genUUID();
    }

    getValue() {
        var val = this.props.json && this.props.json[this.props.name] && this.props.json[this.props.name]["version"] ? this.props.json[this.props.name]["value"] : this.props.json[this.props.name];
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
        var now = fld.endsWith("_us") ? new Date(val / 1000) : new Date(val);
        if (fld.startsWith("run"))
            now.setTime(now.getTime() + now.getTimezoneOffset() * 60 * 1000);
        var today = now.toDateString();
        var time = now.toLocaleTimeString();
        var hrs = now.getHours();
        var min = now.getMinutes();
        var sec = now.getSeconds();
        var mil = now.getMilliseconds();
        var smoothsec = sec + (mil / 1000);
        var smoothmin = min + (smoothsec / 60);
        var canvas = input.querySelector(`canvas`) || input.appendChild(document.createElement("canvas"));
        canvas.height = 100;
        canvas.width = 100;
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
        //ctx.fillStyle = 'rgba(00 ,00 , 00, 1)';
        ctx.clearRect(0, 0, rect.width, rect.height);
        //Hours
        ctx.beginPath();
        ctx.arc(rect.width / 2, rect.height / 2, rect.height * 0.44, degToRad(270), degToRad((hrs * 30) - 90));
        ctx.stroke();
        //Minutes
        ctx.beginPath();
        ctx.arc(rect.width / 2, rect.height / 2, rect.height * 0.38, degToRad(270), degToRad((smoothmin * 6) - 90));
        ctx.stroke();
        //Date
        ctx.font = "8px Helvetica";
        ctx.fillStyle = 'rgba(00, 255, 255, 1)'
        var txtbx = ctx.measureText(today);
        ctx.fillText(today, (rect.width / 2) - txtbx.width / 2, rect.height * .55);
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
        this.state = {
            json: this.props.json,
            error: null,
            stateReqDt: this.props.stateReqDt,
            cols: []
        };
        this.id = this.props.id || genUUID();
    }

    BuildHead(json) {
        if (json) {
            return [e("thead", { key: genUUID(), }, e("tr", { key: genUUID() },
                json.flatMap(row => Object.keys(row))
                .concat(this.props.cols)
                .filter((val, idx, arr) => (val !== undefined) && (arr.indexOf(val) === idx))
                .map(fld => {
                    if (!this.state.cols.some(col => fld == col)) {
                        this.state.cols.push(fld);
                    }
                    return e("td", { key: genUUID() }, fld);
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
                json.map(line => e("tr", { key: genUUID() },
                    this.state.cols.map(fld => e("td", { key: genUUID(), className: "readonly" }, line[fld] !== undefined ? e("div", { key: genUUID(), className: "value" }, this.getValue(fld, line[fld])) : null)))));
        } else {
            return null;
        }
    }

    render() {
        if (this.props.json === null || this.props.json === undefined) {
            return e("div", { key: genUUID(), id: `loading${this.id}` }, "Loading...");
        }

        return e("label", { key: genUUID(), id: this.id, className: "table" }, e("table", { key: genUUID(), className: "greyGridTable" }, [this.BuildHead(this.props.json), this.BuildBody(this.props.json)]));
    }
}

class AppState extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            json: this.props.json,
            error: null,
            stateReqDt: this.props.stateReqDt
        };
        this.id = this.props.id || genUUID();
    }

    Parse(json) {
        if (json) {
            return Object.keys(json)
                .map(fld => {
                    if (Array.isArray(json[fld])) {
                        return e(StateTable, { key: genUUID(), name: fld, label: fld, json: json[fld] });
                    } else if (typeof json[fld] == 'object') {
                        return e(AppState, { key: genUUID(), name: fld, label: fld, json: json[fld] });
                    } else {
                        return e(ROProp, { key: genUUID(), json: json, name: fld, label: fld });
                    }
                });
        } else if (this.state && this.state.error) {
            return this.state.error;
        } else {
            return e("div", { id: `loading${this.id}` }, "Loading...");
        }
    }

    render() {
        if (this.props.json === null || this.props.json === undefined) {
            return e("div", { id: `loading${this.id}` }, "Loading...");
        }
        if (this.props.label != null) {
            return e("fieldset", { name: `/${this.state.path}`, id: `fs${this.id}` }, [e("legend", { key: genUUID() }, this.props.label), this.Parse(this.props.json)]);
        } else {
            return e("fieldset", { name: `/${this.state.path}`, id: `fs${this.id}` }, this.Parse(this.props.json));
        }
    }
}

class MainAppState extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            jsons: [],
            loading: false,
            error: null,
            stateReqDt: this.props.stateReqDt,
            lastFetched: 0
        };

        stateViewer = this;
        this.componentDidUpdate(null, null, null);
    }


    componentDidUpdate(prevProps, prevState, snapshot) {
        if ((this.state.stateReqDt > this.state.lastFetched) && !this.state.loading) {
            this.state.loading = true;
            this.updateStatuses([{ url: "/status/" }, { url: "/status/app" }, { url: "/status/tasks", path: "tasks" }], {});
        }
    }

    updateStatuses(requests, newState) {
        var abort = new AbortController()
        var timer = setTimeout(() => abort.abort(), 1000);
        Promise.all(requests.map(request => {
                return new Promise((resolve, reject) => {
                    fetch(`${httpPrefix}${request.url}`, {
                            method: 'post',
                            signal: abort.signal
                        })
                        .then(data => data.json())
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
                            request.waitFor = 100;
                            request.error = err;
                            reject(err);
                        });
                });
            })).then(results => {
                clearTimeout(timer);
                this.props.domContainer.style.opacity = 1;
                this.setState({
                    error: null,
                    loading: false,
                    jsons: this.orderResults(newState),
                    lastFetched: new Date()
                });
            })
            .catch(err => {
                clearTimeout(timer);
                var errors = requests.filter(req => req.error);
                this.props.domContainer.style.opacity = 0.5
                if (errors[0].waitFor) {
                    setTimeout(() => {
                        if (err.message != "Failed to fetch")
                            console.error(err);
                        this.updateStatuses(requests, newState);
                    }, errors[0].waitFor);
                } else {
                    this.updateStatuses(requests, newState);
                }
            });
    }

    orderResults(res) {
        var ret = {};
        Object.keys(res).filter(fld => (typeof res[fld] != 'object') && !Array.isArray(res[fld])).forEach(fld => ret[fld] = res[fld]);
        Object.keys(res).filter(fld => (typeof res[fld] == 'object') && !Array.isArray(res[fld])).forEach(fld => ret[fld] = res[fld]);
        Object.keys(res).filter(fld => Array.isArray(res[fld])).forEach(fld => ret[fld] = res[fld]);
        return ret;
    }

    render() {
        return [
            e(ControlPannel, { key: genUUID() }),
            e(AppState, { key: genUUID(), json: this.state.jsons, stateReqDt: this.props.stateReqDt })
        ];
    }
}

//#endregion

//#region utility functions
function genUUID() {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
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
    return fld.match(".*ime_.s$");
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

class StorageViewer extends React.Component {
    constructor(props) {
        super(props);
        this.state = { loaded: false, path: this.props.path, files: null };

        this.id = this.props.id || genUUID();
        this.fetchFiles();
        this.mainDiv = document.createElement("div");
    }

    getSystemFolders() {
        return [e("tr", { key: genUUID(), className: "folder" }, [e("td", { key: genUUID() }, this.getFileLink({
            name: "..",
            folder: dirname(this.state.path),
            ftype: "folder",
            size: 0,
        })), e("td", { key: genUUID() })])]
    }

    getFileLink(file) {
        if (file.ftype == "folder") {
            return e("a", {
                key: genUUID(),
                href: "#",
                onClick: () => {
                    this.setState({ total: 0, loaded: false, path: `${file.folder||"/"}${file.name == ".." ? "" :"/"+file.name}`.replaceAll("//", "/") });
                }
            }, file.name);
        } else {
            return e("a", { href: `${httpPrefix}${this.state.path}/${file.name}` }, file.name);
        }
    }

    GetFileStat(fileToFetch, fileStatsToFetch) {
        var timer1 = setTimeout(() => pageControler.abort(), 3000);
        fetch(`${httpPrefix}/stat${fileToFetch.folder}/${fileToFetch.name}`, {
            method: 'post',
            signal: pageControler.signal
        }).then(data => {
            data.json().then(jdata => {
                fileToFetch.size = jdata.size;
                this.setState({ files: this.state.files, total: this.state.total + jdata.size });
                clearTimeout(timer1);
            });
            this.setState({ files: this.state.files });
            if (fileStatsToFetch.length) {
                this.GetFileStat(fileStatsToFetch.pop(), fileStatsToFetch);
            }
        }).catch(ex => {
            clearTimeout(timer1);
            fileStatsToFetch.push(fileToFetch);
        });
    }

    fetchFiles() {
        var timer1 = setTimeout(() => pageControler.abort(), 3000);
        fetch(`${httpPrefix}/files` + this.state.path, {
            method: 'post',
            signal: pageControler.signal
        }).then(data => {
            clearInterval(timer1);
            data.json()
                .then(files => files.sort((f1, f2) => {
                    if (f1.ftype != f2.ftype) {
                        return f1.ftype == "folder" ? 1 : -1;
                    }
                    return f1.name == f2.name ? 0 : f1.name > f2.name ? 1 : -1;
                }))
                .then(files => {
                    this.setState({ loaded: true, files: files });
                    var fileStatsToFetch = files.filter(file => file.ftype == "file");
                    for (var idx = 0; idx < Math.min(3, fileStatsToFetch.length); idx++) {
                        if (fileStatsToFetch.length) {
                            this.GetFileStat(fileStatsToFetch.pop(), fileStatsToFetch);
                        }
                    }
                });
        }).catch(console.error);
    }

    componentDidUpdate(prevProps, prevState) {
        if (!this.state.loaded) {
            this.state.loaded = true;
            this.fetchFiles();
        }
    }

    componentDidMount() {
        document.getElementById(this.id).querySelectorAll("thd").forEach(th => th.addEventListener('click', (() => {
            const table = document.getElementsByClassName("file-table")[0].getElementsByTagName("tbody")[0];
            Array.from(table.querySelectorAll('tr:nth-child(n+2)'))
                .sort(comparer(Array.from(th.parentNode.children).indexOf(th), this.asc = !this.asc))
                .forEach(tr => table.appendChild(tr));
        })));
    }

    getFiles() {
        if (!this.state.files) {
            return e("tr", { key: genUUID() }, [e("td", { key: genUUID() }, "Loading..."), e("td", { key: genUUID() })])
        } else {
            return this.getSystemFolders()
                .concat(this.state.files.map(file => e("tr", { key: genUUID(), className: file.ftype }, [
                    e("td", { key: genUUID() }, this.getFileLink(file)),
                    e("td", { key: genUUID() }, file.ftype != "file" ? "" : file.size)
                ])));
        }
    }

    getTableHeader() {
        return e("thead", { key: genUUID() }, e("tr", { key: genUUID() }, this.props.cols.map(col => e("th", { key: genUUID() }, col))));
    }

    render() {
        return [
            this.mainDiv = e("div", { key: genUUID(), id: this.id, className: "file-table" }, e("table", { key: genUUID(), className: "greyGridTable" }, [
                e("caption", { key: genUUID() }, this.state.path),
                this.getTableHeader(),
                e("tbody", { key: genUUID() }, this.getFiles()),
                e("tfoot", { key: genUUID() }, e("tr", { key: genUUID() }, [e("td", { key: genUUID() }, "Total"), e("td", { key: genUUID() }, this.state.total)]))
            ]))

        ];
    }
}

function renderFolder(clicked) {
    if (clicked) {
        activeTab = document.querySelector("#slide-4");
        if (pageControler)
            pageControler.abort();
        pageControler = new AbortController();
    }
    const domContainer = activeTab = document.querySelector('#slide-1');
    if (fileManager == null) {
        fileManager = ReactDOM.render(e(StorageViewer, { key: genUUID(), path: "/", cols: ["Name", "Size"] }), domContainer);
    } else {
        fileManager.setState({ loaded: false, total: 0 });
    }
}

//#endregion

//#region form populate

function degToRad(degree) {
    var factor = Math.PI / 180;
    return degree * factor;
}

//#endregion

//#region Logs
function AddLogLine(logLn, logs) {
    if (logs == null) {
        return;
    }
    var data = logLn.replaceAll(/^[^IDVWE]*/g, "").replaceAll(/(.*)([\x1B].*\n?)/g, "$1");

    if (data.match(/^[IDVWE] \([0-9.:]{12}\) [^:]+:.*/)) {
        var log = document.createElement("div");
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

        var lmsg = document.createElement("div");
        lmsg.innerText = data.substr(data.indexOf(lfunc.innerText) + lfunc.innerText.length + 2).replaceAll(/^[\r\n]*/g, "");
        lmsg.classList.add(`LOGLINE`);
        log.appendChild(lmsg);
        logs.appendChild(log);
    } else {
        if (lmsg)
            lmsg.innerText += `\n${data}`;
    }
    if (activeTab == logs)
        if (lmsg)
            lmsg.scrollIntoView();
}

function setupWs(ctrl) {
    if (ctrl.checked) {
        if (ws == null) {
            ctrl = document.getElementById(ctrl.id);
            ctrl.style.opacity = 0.5;
            console.log("Creating ws://" + (httpPrefix == "" ? window.location.hostname : httpPrefix.substring(7)) + "/ws");
            ws = new WebSocket("ws://" + (httpPrefix == "" ? window.location.hostname : httpPrefix.substring(7)) + "/ws");
            var stopItWithThatShit = setTimeout(() => {
                ws.close();
                ws = null;
            }, 3000);
            ws.onmessage = (event) => {
                if (event && event.data) {
                    if (event.data[0] == "{") {
                        stateViewer.setState({ lastFetched: new Date(), jsons: Object.assign(Object.assign({}, stateViewer.state.jsons), fromVersionedToPlain(JSON.parse(event.data))) });
                    } else {
                        AddLogLine(event.data, logs);
                    }
                } else {
                    clearTimeout(stopItWithThatShit);
                    stopItWithThatShit = setTimeout(() => {
                        if (ws)
                            ws.close();
                        ws = null;
                    }, 3000);
                }
            };
            ws.onopen = () => {
                clearTimeout(stopItWithThatShit);
                ctrl = document.getElementById(ctrl.id);
                ctrl.style.opacity = 1;
                ctrl.connected = true;
                console.log("Requesting log ws");
                ws.send("Logs")
                console.log("Requesting state ws");
                ws.send("State")
            };
            ws.onerror = (err) => {
                clearTimeout(stopItWithThatShit);
                ws.close();
                setTimeout(() => {
                    ctrl.checked = true;
                    setupWs(ctrl);
                }, 1500);
            };
            ws.onclose = (evt => {
                ctrl = document.getElementById(ctrl.id);
                ctrl.checked = false;
                console.log("log closed");
                ws = null;
            })
        }
    } else {
        if (ws != null) {
            ws.close();
            ws = null;
            ctrl = document.getElementById(ctrl.id);
            ctrl.checked = false;
            ctrl.style.opacity = 1;
        }
    }
}

function refreshSystem(clicked) {
    activeTab = logs;
    if (clicked) {
        activeTab = document.querySelector("#slide-4");
        if (pageControler)
            pageControler.abort();
        pageControler = new AbortController();
    }
}
//#endregion

function SendCommand(body) {
    return fetch(`${httpPrefix}/status/cmd`, {
        method: 'put',
        body: JSON.stringify(body)
    });
}

function SaveForm(form) {
    getJsonConfig().then(vcfg => fromPlainToVersionned(editor.get(), vcfg))
        .then(cfg => fetch(form.action, {
            method: 'post',
            body: JSON.stringify(cfg)
        }).then(data => {
            //refreshConfig(true);
        }));
}

function PeriodicRefreshClicked(target) {
    if (target.checked) {
        activeInterval = setInterval(() => {
            refreshStatus(false);
        }, parseInt(document.getElementById("refreshFreq").value) * 1000);
    } else if (activeInterval !== null) {
        clearInterval(activeInterval);
        activeInterval = null;
    }
}

function refreshStatus(clicked) {
    const domContainer = activeTab = document.querySelector('#slide-2');
    if (clicked) {
        activeTab = domContainer;
        if (pageControler)
            pageControler.abort();
        pageControler = new AbortController();
    }
    if (stateManager == null) {
        stateManager = ReactDOM.render(e(MainAppState, { key: genUUID(), stateReqDt: new Date(), domContainer: domContainer }), domContainer);
    } else {
        stateManager.setState({ stateReqDt: new Date() });
    }
}

function fromVersionedToPlain(obj, level = "") {
    var ret = {};
    var arr = Object.keys(obj);
    var fldidx;
    for (fldidx in arr) {
        var fld = arr[fldidx];
        if ((typeof obj[fld] == 'object') &&
            (Object.keys(obj[fld]).filter(cfld => cfld == "version" || cfld == "value").length == 2)) {
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

function getJsonConfig() {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => pageControler.abort(), 3000);
        fetch(`${httpPrefix}/config`, {
                method: 'post',
                signal: pageControler.signal
            }).then(data => {
                clearTimeout(timer);
                resolve(data.json());
            })
            .catch((err) => {
                clearTimeout(timer);
                reject(err);
            });
    });
}

function refreshConfig(clicked) {
    if (clicked) {
        activeTab = document.querySelectorAll(".system-config form")[1];;
        if (pageControler)
            pageControler.abort();
        pageControler = new AbortController();
    }
    if (editor == null) {
        editor = new JSONEditor(document.getElementById('editor_holder'));
    }
    var retryCnt = 0;
    getJsonConfig().then(fromVersionedToPlain).then(cfg => {
        editor.set(cfg);
    }).catch(err => {
        setTimeout(() => { refreshConfig(clicked) }, 200);
    });
}

const getCellValue = (tr, idx) => tr.children[idx].innerText || tr.children[idx].textContent;

const comparer = (idx, asc) => (a, b) => ((v1, v2) =>
    v1 !== '' && v2 !== '' && !isNaN(v1) && !isNaN(v2) ? v1 - v2 : v1.toString().localeCompare(v2)
)(getCellValue(asc ? a : b, idx), getCellValue(asc ? b : a, idx));


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
};
//#endregion

renderFolder(false);
refreshStatus(false);
refreshConfig(false);
refreshSystem(false);