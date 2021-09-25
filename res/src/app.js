class MainApp extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      tabs: { Storage: {active: true}, 
              Status:  {active: false}, 
              Config:  {active: false}, 
              Logs:    {active: false},
              Events:  {active: false}
            },
      callbacks: {
        stateCBFn: [],
        logCBFn: [],
        eventCBFn: []
      },
      selectedDeviceId: "current"
    };
    this.id = this.props.id || genUUID();
    this.logCBFn = null;
  }

  componentDidMount(){
    Object.keys(this.state.tabs).filter(tab => this.state.tabs[tab].active).forEach(this.setActiveTab.bind(this));
  }

  setActiveTab(tab) {
    this.state.tabs[tab].active = true;
    Object.keys(this.state.tabs)
      .filter(ttab => ttab != tab)
      .forEach(ttab => (this.state.tabs[ttab].active = false));
    Object.keys(this.state.tabs).forEach(ttab => {
      var section = document.getElementById(`${ttab}`);
      var link = document.getElementById(`a${ttab}`);
      if (this.state.tabs[ttab].active){
        link.classList.add("active")
        if (section)
          section.classList.add("active")
        if ((tab == "Config") || (tab == "Status") || (tab == "Events")){
          document.getElementById("controls").classList.remove("hidden");
          document.querySelector("div.slides").classList.remove("expanded");
        } else {
          document.getElementById("controls").classList.add("hidden");
          document.querySelector("div.slides").classList.add("expanded");
        }
      } else{
        link.classList.remove("active")
        if (section)
          section.classList.remove("active")
      }
    });
  }

  registerStateCallback(stateCBFn) {
    this.state.callbacks.stateCBFn.push(stateCBFn);
  }

  registerLogCallback(logCBFn) {
    this.state.callbacks.logCBFn.push(logCBFn);
    }

  registerEventCallback(eventCBFn) {
    this.state.callbacks.eventCBFn.push(eventCBFn);
  }

  onSelectedDeviceId(deviceId) {
    this.setState({selectedDeviceId:deviceId});
  }

  render() {
    return [
      Object.keys(this.state.tabs).map(tab => e("a",{key: genUUID(),
                                                     id: `a${tab}`,
                                                     className: this.state.tabs[tab].active ? "active" : "",
                                                     onClick: () => this.setActiveTab(tab),href: `#${tab}`},tab)),
      e("div", { key: genUUID()},[
      e(ControlPanel, { key: genUUID(), selectedDeviceId: this.state.selectedDeviceId, onSelectedDeviceId: this.onSelectedDeviceId.bind(this), callbacks: this.state.callbacks }),
      e("div", { key: genUUID(), className: `slides${this.state.tabs.Config.active || this.state.tabs.Status.active ? "" : " expanded"}` }, this.state.selectedDeviceId ? [
        e("div",{ className: "file_section",  id: "Storage", key: genUUID() },e(StorageViewer, { active: this.state.tabs.Storage.active, pageControler: this.props.pageControler,path: "/",cols: ["Name", "Size"]})),
        e("div",{ className: "system-config", id: "Status",  key: genUUID() },e(MainAppState,  { active: this.state.tabs.Status.active, pageControler: this.props.pageControler, selectedDeviceId: this.state.selectedDeviceId, registerStateCallback:this.registerStateCallback.bind(this) })),
        e("div",{ className: "system-config", id: "Config",  key: genUUID() },e(ConfigPage,    { active: this.state.tabs.Config.active, pageControler: this.props.pageControler, selectedDeviceId: this.state.selectedDeviceId })),
        e("div",{ className: "logs",          id: "Logs",    key: genUUID() },e(SystemPage,    { active: this.state.tabs.Logs.active, pageControler: this.props.pageControler,   selectedDeviceId: this.state.selectedDeviceId, registerLogCallback:this.registerLogCallback.bind(this) })),
        e("div",{ className: "events",        id: "Events",  key: genUUID() },e(EventsPage,    { active: this.state.tabs.Events.active, pageControler: this.props.pageControler, selectedDeviceId: this.state.selectedDeviceId, registerEventCallback:this.registerEventCallback.bind(this) }))
      ]:[])])
    ];
  }
}

ReactDOM.render(
  e(MainApp, {
    key: genUUID(),
    className: "slider",
    refreshFrequency: 10,
    pageControler: new AbortController()
  }),
  document.querySelector(".slider")
);
