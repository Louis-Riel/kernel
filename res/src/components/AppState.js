class AppState extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            json: this.props.json,
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
            statuses: {},
            devices: [],
            loading: this.props.loading,
            loaded: this.props.loaded,
            error: null,
            selectedDeviceId: 0,
            deviceId: 0,
            refreshFrequency: 10,
            autoRefresh: false
        };

        this.componentDidUpdate(null, null, null);
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

    setupWs(running) {
        if (running) {
            if (this.ws == null) {
                console.log("Creating ws://" + (httpPrefix == "" ? window.location.hostname : httpPrefix.substring(7)) + "/ws");
                this.ws = new WebSocket("ws://" + (httpPrefix == "" ? window.location.hostname : httpPrefix.substring(7)) + "/ws");
                var stopItWithThatShit = setTimeout(() => {
                    console.warn("WS Timeout");
                    this.ws.close();
                }, 3000);
                this.ws.onmessage = (event) => {
                    clearTimeout(stopItWithThatShit);
                    if (event && event.data) {
                        if (event.data[0] == "{") {
                            this.state.statuses[this.state.deviceId] = Object.assign(Object.assign({}, 
                                                                                      this.state.statuses[this.state.deviceId]), 
                                                                                      fromVersionedToPlain(JSON.parse(event.data)))
                            this.setState({ statuses: this.state.statuses });
                        } else if (event.data.match(/.*\) ([^:]*)/g)) {
                            if (this.props.AddLogLine)
                                this.props.AddLogLine(event.data);
                        }
                    }
                    stopItWithThatShit = setTimeout(() => {
                        this.ws.close();
                    }, 3000);
                };
                this.ws.onopen = () => {
                    clearTimeout(stopItWithThatShit);
                    console.log("Connected");
                    this.ws.send("Connect");
                    stopItWithThatShit = setTimeout(() => {
                        console.warn("WS Timeout on open");
                        this.ws.close();
                    }, 3000);
                };
                this.ws.onerror = (err) => {
                    clearTimeout(stopItWithThatShit);
                    this.ws.close();
                };
                this.ws.onclose = (evt => {
                    clearTimeout(stopItWithThatShit);
                    console.log("log closed");
                    this.ws = null;
                    this.setState({autoRefresh:false});
                })
            }
        } else {
            if (this.ws != null) {
                console.log("Closing ws://" + (httpPrefix == "" ? window.location.hostname : httpPrefix.substring(7)) + "/ws");
                this.ws.close();
                this.ws = null;
            } else {
                this.setState({autoRefresh:false});
            }
        }
    }

    updateStatuses(requests, newState) {
        var abort = new AbortController()
        var timer = setTimeout(() => abort.abort(), 4000);
        if (this.state.selectedDeviceId == (this.state.deviceId || 0)) {
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
                            if (!this.state.deviceId && jstats.deviceid) {
                                this.setState({ deviceId: jstats.deviceid, selectedDeviceId: jstats.deviceid });
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
                document.getElementById(this.props.id).style.opacity = 1;
                this.state.statuses[this.state.deviceId] = this.orderResults(newState)
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
                    document.getElementById(this.props.id).style.opacity = 0.5
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
        } else if (this.state.selectedDeviceId) {
            fetch(`${httpPrefix}/lfs/status/${this.state.selectedDeviceId}.json`, {
                method: 'get',
                signal: abort.signal
            }).then(data => data.json()).then(fromVersionedToPlain)
                .then(cfg => {
                    this.state.statuses[this.state.selectedDeviceId] = this.orderResults(cfg);
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
            e(ControlPanel, {
                key: genUUID(),
                deviceId: this.state.deviceId,
                devices: this.state.devices,
                selectedDeviceId: this.state.selectedDeviceId,
                onSetDevice: (val) => this.setState({ selectedDeviceId: val, loaded: false }),
                onRefreshDevice: (elem) => this.setState({loaded: false}),
                onSetDeviceList: (devs) => this.state.devices = devs,
                onAutoRefresh: (toggle) => this.setState({autoRefresh: toggle}),
                refreshFrequency: this.state.refreshFrequency,
                onChangeFreq: (value) => this.setState({refreshFrequency:value}),
                periodicOn: (elem) => {
                    if (!this.state?.interval)
                        this.setState({ interval: setInterval(() => this.setState({ loaded: false, loading: false }),this.state.refreshFrequency*1000) });
                },
                periodicOff: (elem) => {
                    if (this.state?.interval)
                        this.setState({ interval: clearTimeout(this.state?.interval) })
                },
                interval: this.state.interval,
                autoRefresh: this.state.autoRefresh
            }),
            e(AppState, { 
                key: genUUID(), 
                json: this.state.statuses[this.state.selectedDeviceId], 
                deviceId: this.state.selectedDeviceId, 
                devices: this.state.devices
            })
        ];
    }
}
