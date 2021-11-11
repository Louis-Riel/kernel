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
