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
                this.setState({config: config});
            });
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
        if (this.state?.config){
            return [
                e("button", { key: genUUID(), onClick: elem => this.componentDidMount() }, "Refresh"),
                e(JSONEditor, {
                    key: 'ConfigEditor', 
                    path: '/',
                    json: this.state.config, 
                    selectedDeviceId: this.props.selectedDeviceId,
                    editable: true
                })
            ];
        } else {
            return e("div",{key:genUUID()},"Loading....");
        }
    }
}
