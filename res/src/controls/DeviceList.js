class DeviceList extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            devices: this.props.devices,
            loaded: false
        };
    }

    componentDidMount() {
        if (!this.state.loaded && !this.state.devices.length) {
            this.getDevices();
        }
    }

    getDevices() {
        fetch(`${httpPrefix}/files/lfs/config`, {method: 'post'})
            .then(data => data.json())
            .then(json => json.filter(fentry => fentry.ftype == "file"))
            .then(devFiles => {
                this.setState({ loaded:false, devices: devFiles.map(devFile=> devFile.name.substr(0,devFile.name.indexOf("."))) });
                this.props.onGotDevices(this.state.devices);
            })
            .catch(err => {console.error(err);this.setState({error: err})});
    }

    render() {
        return e("select", {
            key: genUUID(),
            value: this.props.selectedDeviceId,
            onChange: (elem) => this.props.onSet(elem.target.value)
        }, this.state.devices ? this.state.devices.map(device => e("option", { key: genUUID(), value: device, value: this.state.selectedDeviceId }, device)):"Loading...");
    }
}
