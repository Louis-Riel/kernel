class MainApp extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      pageControler: this.GetPageControler(),
      selectedDeviceId: "current",
      tabs: { 
        Storage: {active: true}, 
        Status:  {active: false}, 
        Config:  {active: false}, 
        Logs:    {active: false},
        Events:  {active: false}
        }
      };
    this.callbacks={stateCBFn:[],logCBFn:[],eventCBFn:[]};
  }

  GetPageControler() {
    var ret = new AbortController();
    ret.onabort = this.OnAbort;
    return ret;
  }

  OnAbort() {
    console.log("Abort!!!!");
    this.state.pageControler= this.GetPageControler();
  }

  setActiveTab(tab) {
    this.state.tabs[tab].active=true;
    Object.keys(this.state.tabs).filter(ttab => ttab != tab).forEach(ttab => this.state.tabs[ttab].active=false);
    this.refreshActiveTab();
  }

  componentDidMount() {
    this.refreshActiveTab();
  }
  
  refreshActiveTab() {
    Object.keys(this.state.tabs).forEach(ttab => {
      var section = document.getElementById(`${ttab}`);
      var link = document.getElementById(`a${ttab}`);
      if (this.state.tabs[ttab].active){
        link.classList.add("active")
        if (section)
          section.classList.add("active")
        if ((ttab == "Config") || (ttab == "Status") || (ttab == "Events")){
          document.getElementById("controls").classList.remove("hidden");
          document.querySelector("div.slides").classList.remove("expanded");
        } else {
          document.getElementById("controls").classList.add("hidden");
          document.querySelector("div.slides").classList.add("expanded");
        }
      } else{
        link.classList.remove("active")
        section.classList.remove("active")
      }
    });
  }

  registerStateCallback(stateCBFn) {
    if (!this.callbacks.stateCBFn.find(fn => fn.name == stateCBFn.name))
      this.callbacks.stateCBFn.push(stateCBFn);
  }

  registerLogCallback(logCBFn) {
    if (!this.callbacks.logCBFn.find(fn => fn.name == logCBFn.name))
      this.callbacks.logCBFn.push(logCBFn);
  }

  registerEventCallback(eventCBFn) {
    if (!this.callbacks.eventCBFn.find(fn => fn.name == eventCBFn.name))
      this.callbacks.eventCBFn.push(eventCBFn);
  }

  render() {
    return [
      e("div",{ key: genUUID(), className:"tabs"}, Object.keys(this.state.tabs).map(tab => e("a",{key: genUUID(),
                                                     id: `a${tab}`,
                                                     className: this.state.tabs[tab].active ? "active" : "",
                                                     onClick: () => this.setActiveTab(tab),href: `#${tab}`},tab))),
      e("div", { key: genUUID(), className:"slide"},[
      e(ControlPanel, { key: genUUID(), 
                        selectedDeviceId: this.state?.selectedDeviceId, 
                        onSelectedDeviceId: deviceId=>this.setState({selectedDeviceId:deviceId}), 
                        stateCBFn: this.callbacks.stateCBFn,
                        logCBFn: this.callbacks.logCBFn,
                        eventCBFn: this.callbacks.eventCBFn
                       }),
      e("div", { key: genUUID(), className: `slides` }, [
        e("div",{ className: `${this.state.tabs["Storage"].active ? "active":""} file_section`, id: "Storage", key: genUUID() },e(StorageViewer, { pageControler: this.state.pageControler, active: this.state.tabs["Storage"].active})),
        e("div",{ className: `${this.state.tabs["Status"].active ? "active":""} system-config`, id: "Status",  key: genUUID() },e(MainAppState,  { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Status"].active, registerStateCallback:this.registerStateCallback.bind(this) })),
        e("div",{ className: `${this.state.tabs["Config"].active ? "active":""} system-config`, id: "Config",  key: genUUID() },e(ConfigPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Config"].active })),
        e("div",{ className: `${this.state.tabs["Logs"].active ? "active":""} logs`,            id: "Logs",    key: genUUID() },e(SystemPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Logs"].active, registerLogCallback:this.registerLogCallback.bind(this) })),
        e("div",{ className: `${this.state.tabs["Events"].active ? "active":""} events`,        id: "Events",  key: genUUID() },e(EventsPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Events"].active, registerEventCallback:this.registerEventCallback.bind(this) }))
      ])])
    ];
  }
}

ReactDOM.render(
  e(MainApp, {
    key: genUUID(),
    className: "slider"
  }),
  document.querySelector(".slider")
);
