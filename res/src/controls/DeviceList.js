class DeviceList extends React.Component {
    getDevices() {
        if (window.location.host || httpPrefix){
            fetch(`${httpPrefix}/files/lfs/config`, {method: 'post'})
            .then(data => data.json())
            .then(json => json.filter(fentry => fentry.ftype == "file"))
            .then(devFiles => {
                var devices = devFiles.map(devFile=> devFile.name.substr(0,devFile.name.indexOf(".")));
                this.setState({ devices: devices });
                if (this.props?.onGotDevices)
                    this.props.onGotDevices(devices);
            })
            .catch(err => {console.error(err);this.setState({error: err})});
        }
    }

    render() {
        if (!this.state?.devices && !this.props.devices && !this.state?.error) {
            this.getDevices();
        }
        return this.state?.devices?.length || this.props?.devices?.length <= 1 || this.state?.error ? null :
            this.state?.devices || this.props?.devices ?
                e("select", {
                    key: genUUID(),
                    value: this.props.selectedDeviceId,
                    onChange: (elem) => this.props?.onSet(elem.target.value)
                }, (this.state?.devices||this.props.devices).map(device => e("option", { key: genUUID(), value: device }, device))):
                null
    }
}
