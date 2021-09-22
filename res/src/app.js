class MainApp extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      tabs: { Storage: {active: true}, 
              Status:  {active: false}, 
              Config:  {active: false}, 
              Logs:    {active: false} }
    };
    this.id = this.props.id || genUUID();
    this.logCBFn = null;
  }
  setActiveTab(tab) {
    this.state.tabs[tab].active = true;
    Object.keys(this.state.tabs)
      .filter(ttab => ttab != tab)
      .forEach(ttab => (this.state.tabs[ttab].active = false));
    Object.keys(this.state.tabs).forEach(ttab =>
      this.state.tabs[ttab].active
        ? document.getElementById(`a${ttab}`).classList.add("active")
        : document.getElementById(`a${ttab}`).classList.remove("active"));
  }

  AddLogLine(ln) {
    if (ln && this.logCBFn) {
      this.logCBFn(ln);
    }
  }

  registerLogCallback(logCBFn) {
    this.logCBFn = logCBFn;
  }

  render() {
    return [
      Object.keys(this.state.tabs).map(tab => e("a",{key: genUUID(),
                                                     id: `a${tab}`,
                                                     className: this.state.tabs[tab].active ? "active" : "",
                                                     onClick: () => this.setActiveTab(tab),href: `#${tab}`},tab)),
      e("div", { key: genUUID(), className: "slides" }, [
        e("div",{ className: "file_section", id: "Storage", key: genUUID() },e(StorageViewer, {pageControler: this.props.pageControler,path: "/",cols: ["Name", "Size"]})),
        e("div",{ className: "system-config", id: "Status", key: genUUID() },e(MainAppState, { pageControler: this.props.pageControler, AddLogLine: this.AddLogLine.bind(this) })),
        e("div",{ className: "system-config", id: "Config", key: genUUID() },e(ConfigPage, { pageControler: this.props.pageControler })),
        e("div",{ className: "logs", id: "Logs", key: genUUID() },e(SystemPage, { pageControler: this.props.pageControler, registerLogCallback:this.registerLogCallback.bind(this) }))
      ])
    ];
  }
}

ReactDOM.render(
  e(MainApp, {
    key: genUUID(),
    className: "slider",
    pageControler: new AbortController()
  }),
  document.querySelector(".slider")
);
