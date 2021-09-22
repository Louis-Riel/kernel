class ControlPanel extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            periodicRefreshed: false,
            refreshFrequency: this.props.refreshFrequency,
            autoRefresh: false
        };
        this.ws = null;
        this.id = this.props.id || genUUID();
    }

    render() {
        return e('fieldset', { key: genUUID(), id: "controls", key: this.id }, [
            e('legend', { key: genUUID(), id: `lg${this.id}` }, 'Controls'),
            e(BoolInput, { 
                key: genUUID(), 
                label: "Periodic Refresh", 
                onOn: this.props.periodicOn, 
                onOff: this.props.periodicOff,
                initialState: ()=>this.props.interval!=null && this.props.interval!=undefined
            }),
            e(IntInput, {
                key: genUUID(),
                label: "Freq(sec)", 
                value: this.state.refreshFrequency, 
                id: "refreshFreq",
                onChange: (val) => this.props.onChangeFreq ? this.props.onChangeFreq(val):null
            }),
            e(BoolInput, {
                key: genUUID(),
                label: "Auto Refresh",
                onOn: (elem)=>this.props.onAutoRefresh(true),
                onOff: (elem)=>this.props.onAutoRefresh(false),
                id: "autorefresh",
                initialState: () => this.props.autoRefresh
            }),
            e("button", { key: genUUID(), onClick: elem => this.props.onRefreshDevice() }, "Refresh"),
            e(DeviceList, {
                key: genUUID(),
                selectedDeviceId: this.props.selectedDeviceId,
                deviceId: this.props.deviceId,
                devices: this.props.devices,
                onSetDeviceList: this.props.onSetDeviceList,
                onSet: this.props.onSetDevice
            })
        ]);
    }
}
