class DeviceList extends React.Component {

    getDevices() {
        if (window.location.host || httpPrefix){
            wfetch(`${httpPrefix}/files/lfs/config`, {method: 'post'})
            .then(data => data.json())
            .then(json => json.filter(fentry => fentry.ftype == "file"))
            .then(devFiles => {
                let devices = devFiles.map(devFile=> devFile.name.substr(0,devFile.name.indexOf(".")));
                this.setState({ devices: devices, httpPrefix:httpPrefix });
                if (this.props?.onGotDevices)
                    this.props.onGotDevices(devices);
            })
            .catch(err => {console.error(err);this.setState({error: err})});
        }
    }

    componentDidMount() {
        this.getDevices();
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (prevProps?.httpPrefix != this.props.httpPrefix) {
            this.getDevices(this.props.httpPrefix);
        }
    }

    render() {
        return this.state?.devices?.length > 1 ?
            e("select", {
                key: genUUID(),
                value: this.props.selectedDeviceId,
                onChange: (elem) => this.props?.onSet(elem.target.value)
            }, (this.state?.devices||this.props.devices).map(device => e("option", { key: genUUID(), value: device }, device))):
            null
    }
}
