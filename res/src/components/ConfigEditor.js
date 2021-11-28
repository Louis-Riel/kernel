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
                e(ConfigEditor, { key: genUUID(), deviceId: this.props.selectedDeviceId, deviceConfig: this.state.config }),
                e("button", { key: genUUID() }, "Save"),
                e("button", { key: genUUID(), type: "button", onClick:(elem) => this.getJsonConfig(this.props.selectedDeviceId).then(config => this.setState({config:config}))} , "Refresh")
            ]);
        } else {
            return e("div",{key:genUUID()},"Loading....");
        }
    }
}
