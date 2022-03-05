class ConfigPage extends React.Component {
    componentDidMount() {
        if (window.location.host || httpPrefix){
            var abort = new AbortController()
            var timer = setTimeout(() => abort.abort(), 4000);
            wfetch(`${httpPrefix}/config/${this.props.selectedDeviceId=="current"?"":this.props.selectedDeviceId+".json"}`, {
                method: 'post',
                signal: abort.signal
            }).then(resp => resp.json())
              .then(this.orderResults)
              .then(config=>{
                clearTimeout(timer);
                this.setState({config: fromVersionedToPlain(config),
                               original: config});
            });
        }
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (this.state?.config && this.jsoneditor === undefined) {
            try {
                this.jsoneditor = new JSONEditor(this.container, {
                    onChangeJSON: json => this.setState({newconfig: json})
                }, this.state.config);
            } catch (err) {
                this.jsoneditor = null;
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

    getEditor() {
        return [
            e("div", { key: 'fancy-editor', ref: (elem) => this.container = elem, id: `${this.props.id || genUUID()}`, "data-theme": "spectre" }),
            this.jsoneditor === null && this.state?.config ? e(LocalJSONEditor, {
                key: 'ConfigEditor', 
                path: '/',
                json: this.state.config, 
                selectedDeviceId: this.props.selectedDeviceId,
                editable: true
            }) : null,
            this.state?.newconfig ? e("button", {key:"save", onClick:this.saveChanges.bind(this)}, "Save") : null
        ]
    }

    saveChanges() {
        var abort = new AbortController()
        var timer = setTimeout(() => abort.abort(), 4000);
        wfetch(`${httpPrefix}/config`, {
            method: 'put',
            signal: abort.signal,
            body: JSON.stringify(fromPlainToVersionned(this.state.newconfig,this.state.original))
        }).then(resp => resp.json())
          .then(this.orderResults)
          .then(fromVersionedToPlain)
          .then(config=>{
            clearTimeout(timer);
            this.setState({config: config});
        }).catch(console.err);
    }

    render() {
        if (window.location.host || httpPrefix){
            return [
                e("button", { key: genUUID(), onClick: elem => this.componentDidMount() }, "Refresh"),
                this.getEditor() 
            ];
        } else {
            return e("div",{key:genUUID()},"Loading....");
        }
    }
}
