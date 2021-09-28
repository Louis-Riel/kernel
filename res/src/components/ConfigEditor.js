class ConfigEditor extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            loaded: false,
        };

        this.id = this.props.id || genUUID();
    }
    componentDidMount() {
        if (this.props.deviceConfig) {
            this.jsonEditor = new JSONEditor(this.container, {
                onChangeJSON: json => Object.assign(this.props.deviceConfig, json)
            });
            this.jsonEditor.set(this.props.deviceConfig || {});
        } else {
            this.container.innerText = "Loading...";
        }
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (this.jsonEditor) {
            this.jsonEditor.set(this.props.deviceConfig);
        }
    }

    render() {
        return e("div", { key: genUUID(), ref: (elem) => this.container = elem, id: `${this.id}`, className: "column col-md-12", "data-theme": "spectre" })
    }
}

class ConfigPage extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            loaded: false,
            deviceConfigs: {}
        };

        this.id = this.props.id || genUUID();
        this.componentDidUpdate(null, null, null);
    }

    componentDidMount() {
        if (this.props.active) {
            document.getElementById("Config").scrollIntoView()
        }
    }
    
    getJsonConfig(devid) {
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => this.props.pageControler.abort(), 3000);
            fetch(`${httpPrefix}/config${devid?`/${devid}`:""}`, {
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

    getDevices() {
        return new Promise((resolve,reject) =>
            this.getJsonConfig(null)
                .then(fromVersionedToPlain)
                .then(cfg => fetch(`${httpPrefix}/files/lfs/config`, {method: 'post'})
                                .then(data => data.json())
                                .then(cfgs => Promise.all(cfgs.filter(fentry => fentry.ftype == "file" && fentry.name != "current.json")
                                                     .map(fentry => fetch(`${httpPrefix}/lfs/config/${fentry.name}`, {method: 'get'})
                                                                .then(data => data.json())
                                                                .then(fromVersionedToPlain)
                                                                .then(json => {
                                                                    this.state.deviceConfigs[fentry.name.substr(0, fentry.name.indexOf("."))] = json;
                                                                    this.state.deviceConfigs[cfg.deviceid] = cfg;
                                                                    this.state.deviceId = cfg.deviceid;
                                                                    return json;
                                                                })))
                                                  .then(resolve)
                                    )
                ).catch(reject)
        )
    }
    
    componentDidUpdate(prevProps, prevState, snapshot) {
        if (!this.state.loaded && !this.state.loading && !this.state.error) {
            this.state.loading = true;
            if (this.props.selectedDeviceId) {
                fetch(`${httpPrefix}/config${this.props.selectedDeviceId == "current"?"":"/"+this.props.selectedDeviceId}`, {method: 'post'})
                    .then(data => data.json()).then(fromVersionedToPlain)
                    .then(json => {
                        this.state.deviceConfigs[this.props.selectedDeviceId] = json;
                        this.setState({
                            deviceConfigs: this.state.deviceConfigs,
                            loading: false,
                            loaded: true
                        });
                    }).catch((err) => {
                        this.setState({ error: err, loaded: false, loading: false });
                        console.error(err);
                    });
            }
        }
    }

    SaveForm(form) {
        this.getJsonConfig(this.props.selectedDeviceId).then(vcfg => fromPlainToVersionned(this.state.deviceConfigs[this.props.selectedDeviceId], vcfg))
            .then(cfg => fetch(form.target.action.replace("file://", httpPrefix) + "/" + this.props.selectedDeviceId, {
                method: 'put',
                body: JSON.stringify(cfg)
            }).then(res => alert(JSON.stringify(res)))
              .catch(res => alert(JSON.stringify(res))));
        form.preventDefault();
    }

    render() {
        return e("form", { onSubmit: form => this.SaveForm(form), key: `f${this.id}`, action: "/config", method: "post" }, [
            e(ConfigEditor, { key: genUUID(), deviceId: this.props.selectedDeviceId, deviceConfig: this.state.deviceConfigs[this.props.selectedDeviceId] }),
            e("button", { key: genUUID() }, "Save"),
            e("button", { key: genUUID(), onClick:(elem) => this.setState({loaded:false, loading:false, error:null}) }, "Refresh")
        ]);
    }
}
