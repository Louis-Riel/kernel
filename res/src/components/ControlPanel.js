class ControlPanel extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            loaded: this.props.loaded,
            error: null,
            refreshFrequency: this.props.refreshFrequency,
            devices: [],
            selectedDeviceId: this.props.selectedDeviceId
        }
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (prevState && prevState.selectedDeviceId && (prevState.selectedDeviceId != this.state.selectedDeviceId)) {
            if (this.props.onSelectedDeviceId) {
                this.props.onSelectedDeviceId(this.state.selectedDeviceId);
            }
        }
    }
    
    setupWs(running) {
      this.state.autoRefresh=running;
      if (running) {
          if (this.ws == null) {
              this.ws = new WebSocket("ws://" + (httpPrefix == "" ? window.location.hostname : httpPrefix.substring(7)) + "/ws");
              var stopItWithThatShit = setTimeout(() => {
                  console.warn("WS Timeout");
                  this.ws.close();
              }, 3000);
              this.ws.onmessage = (event) => {
                  clearTimeout(stopItWithThatShit);
                  if (event && event.data) {
                      if (event.data[0] == "{") {
                        if (event.data.startsWith('{"eventBase"'))
                          this.ProcessEvent(fromVersionedToPlain(JSON.parse(event.data)));
                        else
                          this.UpdateState(fromVersionedToPlain(JSON.parse(event.data)));
                      } else if (event.data.match(/.*\) ([^:]*)/g)) {
                        this.AddLogLine(event.data);
                      }
                  }
                  stopItWithThatShit = setTimeout(() => {
                      this.ws.close();
                  }, 3000);
              };
              this.ws.onopen = () => {
                  clearTimeout(stopItWithThatShit);
                  this.ws.send("Connect");
                  stopItWithThatShit = setTimeout(() => {
                      console.warn("WS Timeout on open");
                      this.ws.close();
                  }, 3000);
              };
              this.ws.onerror = (err) => {
                  clearTimeout(stopItWithThatShit);
                  this.ws.close();
              };
              this.ws.onclose = (evt => {
                  clearTimeout(stopItWithThatShit);
                  this.ws = null;
                  this.setState({autoRefresh:false});
              })
          }
      } else {
          if (this.ws != null) {
              this.ws.close();
              this.ws = null;
          } else {
            this.state.autoRefresh=false;
          }
      }
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
            e('legend', { key: genUUID(), id: `lg${this.id}` }, 'Controls'),
                e(BoolInput, { 
                    key: genUUID(), 
                    label: "Periodic Refresh", 
                    onOn: (elem => this.state?.interval ? null: this.state.interval= setInterval(() => this.UpdateState(null),this.state.refreshFrequency*1000) ),
                    onOff: (elem => this.state?.interval ? this.state.interval= clearTimeout(this.state?.interval)  : null),
                    initialState: ()=>this.state.interval!=null && this.state.interval!=undefined
                }),
                e(IntInput, {
                    key: genUUID(),
                    label: "Freq(sec)", 
                    value: this.state.refreshFrequency, 
                    id: "refreshFreq",
                    onChange: (val) => this.state.refreshFrequency=val
                }),
                e(BoolInput, {
                    key: genUUID(),
                    label: "Auto Refresh",
                    onOn: (elem)=>this.setupWs(true),
                    onOff: (elem)=>this.setupWs(false),
                    id: "autorefresh",
                    initialState: () => this.state.autoRefresh
                }),
                e("button", { key: genUUID(), onClick: elem => this.UpdateState(null) }, "Refresh"),
                e(DeviceList, {
                    key: genUUID(),
                    selectedDeviceId: this.state.selectedDeviceId,
                    devices: this.state.devices,
                    onSet: val=>this.setState({selectedDeviceId:val}),
                    onGotDevices: devices => { this.state.selectedDeviceId?this.setState({devices:devices}):this.setState({selectedDeviceId:"current", devices:devices})}
                })]);
    }
}
