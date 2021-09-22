class DeviceList extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            devices: this.props.devices,
            loaded: false,
            loading: false
        };
    }

    componentDidMount() {
        if (!this.state.loading && !this.state.loaded && !this.state.devices.length) {
            this.state.loading = true;
            fetch(`${httpPrefix}/files/lfs/status`, {
                method: 'post'
            }).then(data => data.json().then(statuses => {
                this.state.error = null;
                this.state.loaded = true;
                this.state.loading = false;
                this.state.devices = statuses.filter(fentry => fentry.ftype == "file" && fentry.name != "current.json")
                    .map(fentry => parseInt(fentry.name.substr(0, fentry.name.indexOf("."))));
                this.props.onSetDeviceList(this.state.devices);
            })
                .catch(err => this.setState({ loaded: false, error: err })));
        }
    }

    render() {
        return e("select", {
            key: genUUID(),
            value: this.props.selectedDeviceId,
            onChange: (elem) => this.props.onSet(elem.target.value)
        }, this.state.devices.concat(this.props.deviceId).map(device => e("option", { key: genUUID(), value: device }, device)));
    }
}
