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
                        original: config
                    });
                    try {
                        this.jsoneditor = new JSONEditor(this.container, {
                            onChangeJSON: json => this.setState({ newconfig: json })
                        }, this.state.config);
                    } catch (err) {
                        this.nativejsoneditor = e(LocalJSONEditor, {
                            key: 'ConfigEditor',
                            path: '/',
                            json: this.state.config,
                            selectedDeviceId: this.props.selectedDeviceId,
                            editable: true
                        });
                    }
                });
        }
    }

    getEditor() {
        return [
            e("div", { key: 'fancy-editor', ref: (elem) => this.container = elem, id: `${this.props.id || genUUID()}`, "data-theme": "spectre" }),
            this.nativejsoneditor,
            this.state?.newconfig ? e("button", { key: "save", onClick: this.saveChanges.bind(this) }, "Save") : null
        ]
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
        if (this.isConnected()) {
            return [e("button", { key: genUUID(), onClick: elem => this.componentDidMount() }, "Refresh"), 
                    this.getEditor()
                   ];
        } else {
            return e("div", { key: genUUID() }, "Loading....");
        }
    }

    isConnected() {
        return window.location.host || httpPrefix;
    }
}