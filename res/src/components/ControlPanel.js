class ControlPanel extends React.Component {
    componentDidUpdate(prevProps, prevState, snapshot) {
        if ((prevState?.periodicRefresh != this.state?.periodicRefresh) || (prevState?.refreshFrequency != this.state?.refreshFrequency)) {
            this.periodicRefresh(this.state?.periodicRefresh,this.state.refreshFrequency || 10)
        }
    }

    periodicRefresh(enabled, interval) {
        if (enabled && interval) {
            if (this.refreshTimeer) {
                clearTimeout(this.refreshTimeer);
            }
            this.refreshTimeer=setInterval(() => this.UpdateState(null),interval*1000);
        }
        if (!enabled && this.refreshTimeer) {
            clearTimeout(this.refreshTimeer);
            this.refreshTimeer = null;
        }
    }
    
    closeWs() {
        if (this.state?.autoRefresh)
            this.setState({autoRefresh:false});
        if (this.ws) {
            this.ws.close();
        }
    }

    openWs() {
        if (this.ws) {
            return;
        }
        if (!this.state?.autoRefresh)
            this.setState({autoRefresh:true});
        this.ws = new WebSocket("ws://" + (httpPrefix == "" ? window.location.hostname : httpPrefix.substring(7)) + "/ws");
        var stopItWithThatShit = setTimeout(() => { console.log("Main timeout"); if (this.ws.OPEN ) this.ws.close();}, 3000);
        this.ws.onmessage = (event) => {
            clearTimeout(stopItWithThatShit);

            if (!this.state.running || this.state.disconnected) {
                console.log("Connected");
                this.setState({ running: true, disconnected: false, error: null });
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
            stopItWithThatShit = setTimeout(() => { this.state.timeout="Message"; this.ws.close(); },3000)
        };
        this.ws.onopen = () => {
            clearTimeout(stopItWithThatShit);
            this.setState({ connected: true, disconnected: false });
            this.ws.send("Connect");
            stopItWithThatShit = setTimeout(() => { this.state.timeout="Connect"; this.ws.close(); },3000)
        };
        this.ws.onerror = (err) => {
            console.error(err);
            clearTimeout(stopItWithThatShit);
            this.state.error= err ;
            this.ws.close();
        };
        this.ws.onclose = (evt => {
            console.log("Closed");
            clearTimeout(stopItWithThatShit);
            this.setState({ connected: false, running: false });
            this.ws = null;
            if (this.state.autoRefresh) {
                setTimeout(() => {this.openWs();}, 1000);
            }
        });
    }

    AddLogLine(ln) {
      if (ln && this.props.logCBFn) {
        this.props.logCBFn.forEach(logCBFn=>logCBFn(ln));
      }
    }
    
    UpdateState(state) {
      if (this.props.stateCBFn) {
        this.props.stateCBFn.forEach(stateCBFn=>stateCBFn(state));
      }
    }
  
    ProcessEvent(event) {
        if (this.props.eventCBFn) {
            this.props.eventCBFn.forEach(eventCBFn=>eventCBFn(event));
        }
    }

    render() {
     return e('fieldset', { key: genUUID(), className:`slides`, id: "controls", key: this.id }, [
        e(BoolInput, { 
            key: genUUID(), 
            label: "Periodic Refresh", 
            initialState: this.state?.periodicRefresh,
            onChange: val=>this.setState({periodicRefresh:val})
        }),
        e(IntInput, {
            key: genUUID(),
            label: "Freq(sec)", 
            value: this.state?.refreshFrequency || 10, 
            onChange: val=>this.setState({refreshFrequency:val})
        }),
        e(BoolInput, {
            key: genUUID(),
            label: "Auto Refresh",
            onOn: this.openWs.bind(this),
            onOff: this.closeWs.bind(this),
            blurred: this.state?.autoRefresh && this.state.disconnected,
            initialState: this.state ? this.state.autoRefresh : true
        }),
        e(DeviceList, {
            key: genUUID(),
            selectedDeviceId: this.props.selectedDeviceId,
            devices: this.state?.devices,
            onSet: this.props.onSelectedDeviceId,
            onGotDevices: devices=>this.setState({devices:devices})
        })
     ]);
    }
}
