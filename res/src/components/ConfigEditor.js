class ConfigPage extends React.Component {
    componentDidMount() {
        if (this.isConnected()) {
            var abort = new AbortController()
            var timer = setTimeout(() => abort.abort(), 8000);
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
        var timer = setTimeout(() => abort.abort(), 8000);
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
}