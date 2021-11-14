class MainApp extends React.Component {
  constructor(props) {
    super(props);
    this.anims=[];
    this.state = {
      pageControler: this.GetPageControler(),
      selectedDeviceId: "current",
      tabs: { 
        Storage: {active: true}, 
        Status:  {active: false}, 
        Config:  {active: false}, 
        Logs:    {active: false},
        Events:  {active: false}
        },
        autoRefresh: (httpPrefix||window.location.hostname) ? true : false
      };
    if (!httpPrefix && !window.location.hostname){
      this.lookForDevs();
    }
    if (this.state?.autoRefresh && (httpPrefix || window.location.hostname)) {
      this.openWs();
    }
  }

  componentDidMount() {
    this.mountWidget();
  }

  componentDidUpdate(prevProps, prevState, snapshot) {
    //this.mountWidget();
  }

//#region Control Pannel
  lookForDevs() {
    this.state.lanDevices = [];
    for (var idx = 254; idx > 0; idx--) {
      this.state.lanDevices.push(`192.168.1.${idx}`);
    }
    var foundDevices=[];
    for (var idx = 0; idx < Math.min(10, this.state.lanDevices.length); idx++) {
        if (this.state.lanDevices.length) {
            this.scanForDevices(this.state.lanDevices,foundDevices);
        }
    }
  }

  scanForDevices(devices,foundDevices) {
    if (devices.length) {
      var device = devices.pop();
      var abort = new AbortController()
      var timer = setTimeout(() => abort.abort(), 1000);
      var onLine=false;
      fetch(`http://${device}/config/`, {
          method: 'post',
          signal: abort.signal
      }).then(data => {clearTimeout(timer); onLine=true; return data.json()})
        .then(fromVersionedToPlain)
        .then(dev => {
            if (dev?.deviceid) {
              foundDevices.push(dev);
            }
            if (devices.length) {
              this.scanForDevices(devices,foundDevices);
            } else {
              if (!this.state?.OnLineDevices?.length) {
                this.state.OnLineDevices=foundDevices;
                httpPrefix=`http://${foundDevices[0].devName}`
                this.state.autoRefresh=true;
                if (!this.state.connecting && !this.state.connected){
                  this.openWs();
                  this.ReloadPage()
                }
              }
            }
          })
        .catch(err => {
          if (devices.length) {
              this.scanForDevices(devices,foundDevices);
          } else {
              if (!this.state?.OnLineDevices?.length) {
                  this.state.OnLineDevices=foundDevices;
                  httpPrefix=`http://${foundDevices[0].devName}`
                  this.state.autoRefresh=true;
                  if (!this.state.connecting && !this.state.connected){
                    this.openWs();
                    this.ReloadPage();
                  }
              }
          }
        });
    }
  }

  mountWidget() {
    this.widget.addEventListener("click", (event) => {
      if ((event.offsetX > 2) &&
          (event.offsetX < 32) &&
          (event.offsetY > 2) &&
          (event.offsetY < 32)) {
        this.state.autoRefresh=!this.state.autoRefresh;
        this.state.autoRefresh?this.openWs():this.closeWs();
      }
    });
    window.requestAnimationFrame(this.drawDidget.bind(this));
  }

  closeWs() {
    if (this.ws) {
        this.state.autoRefresh=false;
        this.ws.close();
        this.ws = null;
    }
  }

  openWs() {
    if (!window.location.hostname && !httpPrefix) {
        return;
    }
    if (this.state?.connecting || this.state?.connected) {
        return;
    }
    this.state.connecting=true;
    this.state.running=false;
    var ws = this.ws = new WebSocket("ws://" + (httpPrefix == "" ? window.location.hostname : httpPrefix.substring(7)) + "/ws");
    var stopItWithThatShit = setTimeout(() => { console.log("Main timeout"); ws.close(); this.state.connecting=false}, 3000);
    ws.onmessage = (event) => {
        clearTimeout(stopItWithThatShit);
        if (!this.state.running || this.state.timeout) {
            this.state.running= true;
            this.state.error= null;
            this.state.timeout= null;
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
        stopItWithThatShit = setTimeout(() => { this.state.timeout="Message"; ws.close();console.log("Message timeout")},3000)
    };
    ws.onopen = () => {
      clearTimeout(stopItWithThatShit);
        this.state.connected=true;
        this.state.connecting=false;
        ws.send("Connected");
        stopItWithThatShit = setTimeout(() => { this.state.timeout="Connect"; ws.close();console.log("Connect timeout")},3000)
    };
    ws.onerror = (err) => {
        console.error(err);
        clearTimeout(stopItWithThatShit);
        this.state.error= err;
        ws.close();
    };
    ws.onclose = (evt => {
        clearTimeout(stopItWithThatShit);
        this.state.connected=false;
        this.state.connecting=false;
        if (this.state.autoRefresh)
          this.openWs();
    });
  }

  drawDidget(){
    if (!this.widget)
        return;
        
    var canvas = this.widget.getContext("2d");
    canvas.clearRect(0,0,100,40);

    this.browser(canvas, 2, 2, 30, 30, 10);
    this.chip(canvas, 100-20-10, 2, 20, 30, 5);

    if (this.anims.length){
        var animGroups = this.anims.reduce((pv,cv)=>{
            (pv[cv.type]=pv[cv.type]||[]).push(cv);
            return pv
        },{});
        for (var agn in animGroups) {
            canvas.beginPath();
            animGroups[agn].forEach(anim => {
                this.drawSprite(anim, canvas);
            });
        }

        this.anims = this.anims.filter(anim => anim.state != 2);
    }
    window.requestAnimationFrame(this.drawDidget.bind(this));
  }

  drawSprite(anim, canvas) {
    if (!anim.state) {
        anim.state = 1;
        anim.x = anim.startX;
        anim.y = anim.startY;
    } else {
        anim.x -= 3+anim.weight;
    }
    canvas.strokeStyle = anim.lineColor;
    canvas.lineWidth = 1;
    canvas.shadowBlur = 1;
    canvas.shadowColor = anim.shadowColor;
    canvas.fillStyle = anim.color;
    canvas.moveTo(anim.x, anim.y);
    canvas.arc(anim.x, anim.y, 4 + (anim.weight), 0, 2 * Math.PI);
    canvas.fill();
    if (anim.x < 30) {
        anim.state = 2;
    }
    return anim;
  }

  browser(canvas, startX, startY, boxWidht, boxHeight, cornerSize) {
    canvas.beginPath();
    canvas.strokeStyle = '#00ffff';
    canvas.lineWidth = 2;
    canvas.shadowBlur = 2;
    canvas.shadowColor = '#00ffff';
    canvas.fillStyle = this.state?.autoRefresh ? (this.state?.error || this.state?.timeout ? "#f27c7c" : this.state?.connected?"#00ffff59":"#0396966b") : "#000000"
    this.roundedRectagle(canvas, startX, startY, boxWidht, boxHeight, cornerSize);
    canvas.fill();
    if (this.state?.lanDevices?.length) {
      this.roundedRectagle(canvas, startX, startY, boxWidht, boxHeight * (this.state.lanDevices.length/254), cornerSize);
    }

    canvas.stroke();
  }

  chip(canvas, startX, startY, boxWidht, boxHeight, cornerSize) {
    canvas.beginPath();
    const pinWidth = 4;
    const pinHeight = 2;
    const pinVCount = 3;

    canvas.strokeStyle = '#00ffff';
    canvas.lineWidth = 2;
    canvas.shadowBlur = 2;
    canvas.shadowColor = '#00ffff';
    canvas.fillStyle = "#00ffff";
    this.roundedRectagle(canvas, startX, startY, boxWidht, boxHeight, cornerSize);

    for (var idx = 0; idx < pinVCount; idx++) {
        canvas.moveTo(startX,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        canvas.lineTo(startX-pinWidth,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        canvas.lineTo(startX-pinWidth,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        canvas.lineTo(startX,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));

        canvas.moveTo(startX+boxWidht,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        canvas.lineTo(startX+boxWidht+pinWidth,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        canvas.lineTo(startX+boxWidht+pinWidth,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        canvas.lineTo(startX+boxWidht,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
    }
    canvas.stroke();
  }

  roundedRectagle(canvas, startX, startY, boxWidht, boxHeight, cornerSize) {
    canvas.moveTo(startX + cornerSize, startY);
    canvas.lineTo(startX + boxWidht - (2 * cornerSize), startY);
    canvas.arcTo(startX + boxWidht, startY, startX + boxWidht, startY + cornerSize, cornerSize);
    canvas.lineTo(startX + boxWidht, startY + boxHeight - cornerSize);
    canvas.arcTo(startX + boxWidht, startY + boxHeight, startX + boxWidht - cornerSize, startY + boxHeight, cornerSize);
    canvas.lineTo(startX + cornerSize, startY + boxHeight);
    canvas.arcTo(startX, startY + boxHeight, startX, startY + boxHeight - cornerSize, cornerSize);
    canvas.lineTo(startX, startY + cornerSize);
    canvas.arcTo(startX, startY, startX + cornerSize, startY, cornerSize);
  }

  AddLogLine(ln) {
    var anims = this.anims.filter(anim => anim.type == "log");
    var curAnims = anims.filter(anim => anim.level == ln[0]);
    if ((anims.length > 4) && (curAnims.length>1)) {
        (curAnims.find(anim => anim.weight < 3) || curAnims[0]).weight++;
    } else {
        this.anims.push({
            type:"log",
            level:ln[0],
            color:ln[0] == 'D' ? "green" : ln[0] == 'W' ? "yellow" : "red",
            weight: 1,
            lineColor: '#00ffff',
            shadowColor: '#00ffff',
            startX: 70,
            startY: 25,
            renderer: this.drawSprite
        })
    }
    this.callbacks.logCBFn.forEach(logCBFn=>logCBFn(ln));
  }

  UpdateState(state) {
    this.anims.push({
        type:"state",
        color:"#00ffff",
        weight: 1,
        lineColor: '#00ffff',
        shadowColor: '#00ffff',
        startX: 70,
        startY: 15,
        renderer: this.drawSprite
    });
    this.callbacks.stateCBFn.forEach(stateCBFn=>stateCBFn(state));
  }

  ProcessEvent(event) {
    this.anims.push({
        type:"event",
        eventBase: event.eventBase,
        color:"#7fffd4",
        weight: 1,
        lineColor: '#00ffff',
        shadowColor: '#00ffff',
        startX: 70,
        startY: 5,
        renderer: this.drawSprite
    });
    this.callbacks.eventCBFn.forEach(eventCBFn=>eventCBFn.fn(event));
  }
//#endregion

//#region Page Tabulation
  GetPageControler() {
    var ret = new AbortController();
    ret.onabort = this.OnAbort;
    return ret;
  }

  OnAbort() {
    this.state.pageControler= this.GetPageControler();
  }

  setActiveTab(tab) {
    this.state.tabs[tab].active=true;
    Object.keys(this.state.tabs).filter(ttab => ttab != tab).forEach(ttab => this.state.tabs[ttab].active=false);
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
//#endregion

//#region Event Management
  registerStateCallback(stateCBFn) {
    if (!this.callbacks.stateCBFn.find(fn => fn.name == stateCBFn.name))
      this.callbacks.stateCBFn.push(stateCBFn);
  }

  registerLogCallback(logCBFn) {
    if (!this.callbacks.logCBFn.find(fn => fn.name == logCBFn.name))
      this.callbacks.logCBFn.push(logCBFn);
  }

  registerEventInstanceCallback(eventCBFn,instance) {
    var cur = null;
    if (!(cur=this.callbacks.eventCBFn.find(fn => fn.fn.name == eventCBFn.name && fn.instance === instance)))
      this.callbacks.eventCBFn.push({fn:eventCBFn,instance:instance});
    else {
      cur.fn=eventCBFn;
    }
  }

  registerEventCallback(eventCBFn) {
    if (!this.callbacks.eventCBFn.find(fn => fn.fn.name == eventCBFn.name && fn.instance === undefined))
      this.callbacks.eventCBFn.push({fn:eventCBFn});
  }
//#endregion

  getPage(name) {
    if (name == "Storage") {
      return e(StorageViewer, { pageControler: this.state.pageControler, active: this.state.tabs["Storage"].active});
    }
    if (name == "Status") {
      return e(MainAppState,  { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Status"].active, registerEventInstanceCallback:this.registerEventInstanceCallback.bind(this), registerStateCallback:this.registerStateCallback.bind(this) });
    }
    if (name == "Config") {
      return e(ConfigPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Config"].active });
    }
    if (name == "Logs") {
      return e(SystemPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Logs"].active, registerLogCallback:this.registerLogCallback.bind(this) });
    }
    if (name == "Events") {
      return e(EventsPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Events"].active, registerEventCallback:this.registerEventCallback.bind(this) });
    }
    return null;
  }

  ReloadPage(){
    this.setState({pageControler: this.GetPageControler()});
  }

  render() {
    this.callbacks={stateCBFn:[],logCBFn:[],eventCBFn:[]};
    return e("div",{key:genUUID(),className:"mainApp"}, [
      e('fieldset', { key: genUUID(), className:`slides`, id: "controls"}, [
        this.state?.OnLineDevices?.length && !window.location.hostname ?
            e("select",{
              key: genUUID(),
              className: "landevices",
              value: httpPrefix.substring(7),
              onChange: elem=>{httpPrefix=`http://${elem.target.value}`;this.ws?.close(),this.ReloadPage()}
              },this.state.OnLineDevices.map(lanDev=>e("option",{
                  key:genUUID(),
                  className: "landevice"
              },lanDev.devName))
            ):null,
        e("canvas",{
            key: genUUID(),
            height: 40,
            width:100,
            ref: (elem) => this.widget = elem
        }),
        e(DeviceList, {
            key: genUUID(),
            selectedDeviceId: this.props.selectedDeviceId,
            devices: this.state?.devices,
            onSet: this.props.onSelectedDeviceId,
            onGotDevices: devices=>this.setState({devices:devices})
        })
      ]),
      Object.keys(this.state.tabs).map(tab => 
        e("details",{key:genUUID(),id:tab, className:"appPage slides", open: this.state.tabs[tab].active, onClick:elem=>{
            elem.target.parentElement.setAttribute("open",true);
            Object.keys(this.state.tabs).forEach(ttab => ttab == tab ? this.state.tabs[ttab].active=true : this.state.tabs[ttab].active=false );
            [].slice.call(elem.target.parentElement.parentElement.children).filter(ttab => ttab != elem).forEach(ttab => ttab.removeAttribute("open"))
          }},
          [
            e("summary",{key:genUUID(),className:"appTab"},tab),
            e("div",{key:genUUID(),className: tab == "Status" ? "pageContent system-config" : "pageContent"}, this.getPage(tab))
          ]
        )
      )
    ]);
  }
}

ReactDOM.render(
  e(MainApp, {
    key: genUUID(),
    className: "slider"
  }),
  document.querySelector(".slider")
);
