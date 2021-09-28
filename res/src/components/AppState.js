class AppState extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            error: null
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
                        return e(ROProp, { key: genUUID(), value: json[fld], name: fld, label: fld });
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
            statuses: {},
            loading: this.props.loading,
            loaded: this.props.loaded,
            error: null,
            selectedDeviceId: 0,
            refreshFrequency: 10,
            autoRefresh: false
        };
        if (this.props.registerStateCallback) {
            this.props.registerStateCallback(this.updateMainStatus.bind(this));
        }
    }

    componentDidMount() {
        if (this.props.active) {
            document.getElementById("Status").scrollIntoView()
        }
        this.updateStatuses([{ url: "/status/" }, { url: "/status/app" }, { url: "/status/tasks", path: "tasks" }], {});
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (!this.state.loading && !this.state.loaded) {
            this.state.loading = true;
            this.updateStatuses([{ url: "/status/" }, { url: "/status/app" }, { url: "/status/tasks", path: "tasks" }], {});
        }
        if (prevState && (this.state.autoRefresh != prevState.autoRefresh)) {
            this.setupWs(this.state.autoRefresh);
        }
    }

    updateMainStatus(stat) {
        if (stat){
            const flds = Object.keys(stat);
            for (const fld in flds) {
                this.state.statuses["current"][flds[fld]] = stat[flds[fld]];    
            }
            this.setState({statuses:this.state.statuses});
        } else {
            this.updateStatuses([{ url: "/status/" }, { url: "/status/app" }, { url: "/status/tasks", path: "tasks" }], {});
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
                            request.waitFor = 4000;
                            request.error = err;
                            reject(err);
                        });
                });
            })).then(results => {
                clearTimeout(timer);
                document.getElementById("Status").style.opacity = 1;
                this.state.statuses[this.props.selectedDeviceId] = this.orderResults(newState)
                this.setState({
                    error: null,
                    loading: false,
                    loaded: true,
                    statuses: this.state.statuses
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
                .then(cfg => {
                    this.state.statuses[this.props.selectedDeviceId] = this.orderResults(cfg);
                    this.setState({ statuses: this.state.statuses, loading: false, loaded: true });
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
        return [
            e(AppState, {
                key: genUUID(), 
                json: this.state.statuses[this.props.selectedDeviceId], 
                selectedDeviceId: this.props.selectedDeviceId
            })
        ];
    }
}
