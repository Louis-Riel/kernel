class StatusPage extends React.Component {
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
                Promise.all(requests.map(request => {
                    return new Promise((resolve, reject) => {
                        wfetch(`${httpPrefix}${request.url}`, {
                            method: 'post',
                            signal: abort.signal
                        }).then(data => data.json())
                          .then(this.filterProperties.bind(this))
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
                e(JSONEditor, {
                    key: genUUID(), 
                    json: this.state.status, 
                    editable: false,
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
