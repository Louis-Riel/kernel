class ControlPanel extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            refreshFrequency: this.props.refreshFrequency,
            devices: [],
            autoRefresh: true,
            selectedDeviceId: this.props.selectedDeviceId
        }
    }

    componentDidMount(){
        this.openWs();
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (prevState && prevState.selectedDeviceId && (prevState.selectedDeviceId != this.state.selectedDeviceId)) {
            if (this.props.onSelectedDeviceId) {
                this.props.onSelectedDeviceId(this.state.selectedDeviceId);
            }
        }

        if (prevState.autoRefresh != this.state.autoRefresh) {
            this.state.autoRefresh ? this.openWs() : this.closeWs()
        }

        if (prevState.periodicRefresh != this.state.periodicRefresh) {
            if (this.state.periodicRefresh) {
                this.state.interval ? null: this.state.interval= setInterval(() => this.UpdateState(null),this.state.refreshFrequency*1000);
            } else {
                this.state.interval ? this.state.interval= clearTimeout(this.state?.interval)  : null;
            }
        }
    }
    
    closeWs() {
        if (this.ws != null) {
            this.ws.close();
            this.ws = null;
        }
    }

    openWs() {
        if (this.ws) {
            return;
        }
        this.ws = new WebSocket("ws://" + (httpPrefix == "" ? window.location.hostname : httpPrefix.substring(7)) + "/ws");
        var stopItWithThatShit = setTimeout(() => { this.ws.close(); }, 3000);
        this.ws.onmessage = (event) => {
            clearTimeout(stopItWithThatShit);

            if (!this.state.running || this.state.disconnected) {
                this.setState({ running: true, disconnected: false });
            }

            if (event && event.data) {
                if (event.data[0] == "{") {
                    if (event.data.startsWith('{"eventBase"')) {
                        this.ProcessEvent(fromVersionedToPlain(JSON.parse(event.data)));
                    } else {
                        this.UpdateState(fromVersionedToPlain(JSON.parse(event.data)));
                    }
                } else if (event.data.match(/.*\) ([^:]*)/g)) {
                    this.AddLogLine(event.data);
                }
            }
            stopItWithThatShit = setTimeout(() => { this.setState({ disconnected: true }); this.ws.close(); }, 3000);
        };
        this.ws.onopen = () => {
            clearTimeout(stopItWithThatShit);
            this.setState({ connected: true, disconnected: false });
            this.ws.send("Connect");
            stopItWithThatShit = setTimeout(() => { this.setState({ disconnected: true }); this.ws.close(); }, 3000);
        };
        this.ws.onerror = (err) => {
            clearTimeout(stopItWithThatShit);
            this.setState({ error: err });
            this.ws.close();
        };
        this.ws.onclose = (evt => {
            clearTimeout(stopItWithThatShit);
            this.ws = null;
            this.setState({ connected: false, disconnected: true, running: false });
            if (this.state.autoRefresh) {
                setTimeout(() => {this.openWs();}, 1000);
            }
        });
    }

    AddLogLine(ln) {
      if (ln && this.props.callbacks.logCBFn) {
        this.props.callbacks.logCBFn.forEach(logCBFn=>logCBFn(ln));
      }
    }
    
    UpdateState(state) {
      if (this.props.callbacks.stateCBFn) {
        this.props.callbacks.stateCBFn.forEach(stateCBFn=>stateCBFn(state));
      }
    }
  
    ProcessEvent(event) {
        if (this.props.callbacks.eventCBFn) {
            this.props.callbacks.eventCBFn.forEach(eventCBFn=>eventCBFn(event));
        }
    }

    render() {
     return e('fieldset', { key: genUUID(), className:`slides`, id: "controls", key: this.id }, [
        e(BoolInput, { 
            key: genUUID(), 
            label: "Periodic Refresh", 
            onChange: state=>this.setState({periodicRefresh:state})
        }),
        e(IntInput, {
            key: genUUID(),
            label: "Freq(sec)", 
            value: this.state.refreshFrequency, 
            onChange: val=>this.setState({refreshFrequency:val})
        }),
        e(BoolInput, {
            key: genUUID(),
            label: "Auto Refresh",
            onChange: state => this.setState({autoRefresh:state}),
            blurred: this.state.autoRefresh && this.state.disconnected,
            initialState: this.state.autoRefresh
        }),
        e("button", { key: genUUID(), onClick: elem => this.UpdateState(null) }, "Refresh"),
        this.state.devices.length < 1 ? null : e(DeviceList, {
            key: genUUID(),
            selectedDeviceId: this.state.selectedDeviceId,
            devices: this.state.devices,
            onSet: val=>this.setState({selectedDeviceId:val}),
            onGotDevices: devices => { this.state.selectedDeviceId?this.setState({devices:devices}):this.setState({selectedDeviceId:"current", devices:devices})}
        })]);
    }
}
