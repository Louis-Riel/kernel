var app=null;

class MainApp extends React.Component {
  constructor(props) {
    super(props);
    this.anims=[];
    app=this;
    this.state = {
      pageControler: this.GetPageControler(),
      selectedDeviceId: "current",
      httpPrefix: httpPrefix,
      tabs: { 
        Storage: {active: true}, 
        Status:  {active: false}, 
        Config:  {active: false}, 
        System:  {active: false},
        Logs:    {active: false},
        Events:  {active: false}
        },
        autoRefresh: (httpPrefix||window.location.hostname) ? true : false
      };
    this.callbacks={stateCBFn:[],logCBFn:[],eventCBFn:[]};

    if (!httpPrefix && !window.location.hostname){
      this.lookForDevs();
    }
    if (this.state?.autoRefresh && (httpPrefix || window.location.hostname)) {
      this.openWs();
    }
  }
  
  componentDidMount() {
    app=this;
    this.mountWidget();
    this.mounted=true;
  }

  componentWillUnmount(){
    this.mounted=false;
  }

//#region Control Pannel
  lookForDevs() {
    var RTCPeerConnection = /*window.RTCPeerConnection ||*/ window.webkitRTCPeerConnection || window.mozRTCPeerConnection;
    if (RTCPeerConnection)(() => {  
      var rtc = new RTCPeerConnection({  
          iceServers: []  
      });  
      if (1 || window.mozRTCPeerConnection) {  
          rtc.createDataChannel('', {  
              reliable: false  
          });  
      };  
      rtc.onicecandidate = (evt) => {  
          if (evt.candidate) parseLine.bind(this)("a=" + evt.candidate.candidate);  
      };  
      rtc.createOffer(function (offerDesc) {  
        offerDesc.sdp.split('\r\n').forEach(parseLine.bind(this));
        rtc.setLocalDescription(offerDesc);  
      }.bind(this),(e) => {  
          console.warn("offer failed", e);  
      });  
      var addrs = Object.create(null);  
      addrs["0.0.0.0"] = false;  

      function parseLine(line) {
        if (~line.indexOf("a=candidate")) {
          var parts = line.split(' '), addr = parts[4], type = parts[7];
          if (type === 'host')
            console.log(addr);
        } else if (~line.indexOf("c=")) {
          var parts = line.split(' '), addr = parts[2].split("."), addrtyoe = parts[1];
          if (addr[0] === '0') {
            addr[0]=192;
            addr[1]=168;
            addr[2]=0;
          }

          if ((addrtyoe === "IP4") && addr[0]) {
            this.state.lanDevices = [];
            for (var idx = 254; idx > 0; idx--) {
              addr[3] = idx;
              this.state.lanDevices.push(addr.join("."));
            }
            this.state.lanDevices = this.state.lanDevices.sort(() => .5 - Math.random());
            var foundDevices = [];
            for (var idx = 0; idx < Math.min(10, this.state.lanDevices.length); idx++) {
              if (this.state.lanDevices.length) {
                this.scanForDevices(this.state.lanDevices, foundDevices);
              }
            }
          }
        }
      }

    }).bind(this)();  
    else {  
        document.getElementById('list').innerHTML = "<code>ifconfig| grep inet | grep -v inet6 | cut -d\" \" -f2 | tail -n1</code>";  
        document.getElementById('list').nextSibling.textContent = "In Chrome and Firefox your IP should display automatically, by the power of WebRTCskull.";  
    }
  }

  scanForDevices(devices,foundDevices) {
    if (devices.length) {
      var device = devices.pop();
      var abort = new AbortController()
      var timer = setTimeout(() => abort.abort(), 1000);
      var onLine=false;
      wfetch(`http://${device}/config/`, {
          method: 'post',
          signal: abort.signal
      }).then(data => {clearTimeout(timer); onLine=true; return data.json()})
        .then(fromVersionedToPlain)
        .then(dev => {
            if (dev?.deviceid) {
              var anims = this.anims.filter(anim => anim.type == "ping" && anim.from=="chip");
              var inSpot = getInSpot(anims, "chip");
              if (inSpot) {
                inSpot.weight++;
              } else {
                this.anims.push({
                    type:"ping",
                    from: "chip",
                    weight: 1,
                    lineColor: '#00ffff',
                    shadowColor: '#000000',
                    fillColor: '#000000',
                    textColor: '#00ffff',
                    startY: 30,
                    renderer: this.drawSprite
                })
                window.requestAnimationFrame(this.drawDidget.bind(this));
              }
              dev.ip=device;
              foundDevices.push(dev);
              if (!httpPrefix){
                httpPrefix=`http://${foundDevices[0].ip}`
                this.state.autoRefresh=true;
                if (!this.state.connecting && !this.state.connected){
                  this.openWs();
                }
              }
              this.setState({OnLineDevices: foundDevices});
            }

            if (devices.length) {
              this.scanForDevices(devices,foundDevices);
            }
          })
        .catch(err => {
          if (devices.length) {
              this.scanForDevices(devices,foundDevices);
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
    if (!this.mounted)
      window.requestAnimationFrame(this.drawDidget.bind(this));
  }

  closeWs() {
    if (this.ws) {
        this.state.autoRefresh=false;
        this.ws.close();
        this.ws = null;
        window.requestAnimationFrame(this.drawDidget.bind(this));
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
    this.startWs();
  }

  startWs() {
    var ws = this.ws = new WebSocket("ws://" + (httpPrefix == "" ? `${window.location.hostname}:${window.location.port}` : httpPrefix.substring(7)) + "/ws");
    var stopItWithThatShit = setTimeout(() => { console.log("Main timeout"); ws.close(); this.state.connecting = false; }, 3500);
    ws.onmessage = (event) => {
      stopItWithThatShit = this.processMessage(stopItWithThatShit, event, ws);
    };
    ws.onopen = () => {
      stopItWithThatShit = this.wsOpen(stopItWithThatShit, ws);
    };
    ws.onerror = (err) => {
      this.wsError(err, stopItWithThatShit, ws);
    };
    ws.onclose = (evt => {
      this.wsClose(stopItWithThatShit);
    });
  }

  wsClose(stopItWithThatShit) {
    clearTimeout(stopItWithThatShit);
    this.state.connected = false;
    this.state.connecting = false;
    window.requestAnimationFrame(this.drawDidget.bind(this));
    if (this.state.autoRefresh)
      this.openWs();
  }

  wsError(err, stopItWithThatShit, ws) {
    console.error(err);
    clearTimeout(stopItWithThatShit);
    this.state.error = err;
    window.requestAnimationFrame(this.drawDidget.bind(this));
    ws.close();
  }

  wsOpen(stopItWithThatShit, ws) {
    clearTimeout(stopItWithThatShit);
    this.state.connected = true;
    this.state.connecting = false;
    ws.send("Connected");
    window.requestAnimationFrame(this.drawDidget.bind(this));
    stopItWithThatShit = setTimeout(() => { this.state.timeout = "Connect"; ws.close(); console.log("Connect timeout"); }, 3500);
    return stopItWithThatShit;
  }

  processMessage(stopItWithThatShit, event, ws) {
    clearTimeout(stopItWithThatShit);
    if (!this.state.running || this.state.timeout) {
      this.state.running = true;
      this.state.error = null;
      this.state.timeout = null;
    }

    if (event && event.data) {
      if (event.data[0] == "{") {
        try {
          if (event.data.startsWith('{"eventBase"')) {
            this.ProcessEvent(fromVersionedToPlain(JSON.parse(event.data)));
          } else {
            this.UpdateState(fromVersionedToPlain(JSON.parse(event.data)));
          }
        } catch (e) {
        }
      } else if (event.data.match(/.*\) ([^:]*)/g)) {
        this.AddLogLine(event.data);
      }
    } else {
      this.ProcessEvent(undefined);
    }
    stopItWithThatShit = setTimeout(() => { this.state.timeout = "Message"; ws.close(); console.log("Message timeout"); }, 4000);
    return stopItWithThatShit;
  }

  drawDidget(){
    if (!this.widget)
        return;
        
    var canvas = this.widget.getContext("2d");
    canvas.clearRect(0,0,100,40);

    this.browser(canvas, 2, 2, 30, 30, 10);
    this.chip(canvas, 70, 2, 20, 30, 5);

    if (this.anims.length){
        var animGroups = this.anims.reduce((pv,cv)=>{
            (pv[cv.type]=pv[cv.type]||[]).push(cv);
            return pv
        },{});
        for (var agn in animGroups) {
            animGroups[agn].forEach(anim => {
                this.drawSprite(anim, canvas);
            });
        }

        this.anims = this.anims.filter(anim => anim.state != 2);
        window.requestAnimationFrame(this.drawDidget.bind(this));
      }
  }

  drawSprite(anim, canvas) {
    if (!anim.state) {
        anim.state = 1;
        anim.x = anim.from == "chip" ? this.chipX : this.browserX;
        anim.endX = anim.from == "chip" ? this.browserX : this.chipX;
        anim.direction=anim.from=="browser"?1:-1;
        anim.y = anim.startY;
        anim.tripLen = 1600.0;
        anim.startWindow = performance.now();
        anim.endWindow = anim.startWindow + anim.tripLen;
    } else {
        anim.x += (anim.direction*((performance.now() - anim.startWindow))/anim.tripLen);
    }
    var width=Math.min(15,4 + (anim.weight));
    if ((anim.y-(width/2)) <= 0) {
      anim.y = width;
    }
    canvas.beginPath();
    canvas.lineWidth = 2;
    canvas.shadowBlur = 2;
    canvas.strokeStyle = anim.lineColor;
    canvas.shadowColor = anim.fillColor;
    canvas.fillStyle = anim.fillColor;
    canvas.moveTo(anim.x+width, anim.y);
    canvas.arc(anim.x, anim.y, width, 0, 2 * Math.PI);
    canvas.fill();
    canvas.stroke();

    if (anim.weight > 1) {
      var today = ""+anim.weight
      canvas.font = Math.min(20,8+anim.weight)+"px Helvetica";
      var txtbx = canvas.measureText(today);
      canvas.fillStyle=anim.textColor;
      canvas.strokeStyle = anim.textColor;
      canvas.fillText(today, anim.x - txtbx.width / 2, anim.y + txtbx.actualBoundingBoxAscent / 2);
    }    
    
    if (anim.direction==1?(anim.x > anim.endX):(anim.x < anim.endX)) {
        anim.state = 2;
    }
    return anim;
  }

  browser(canvas, startX, startY, boxWidht, boxHeight, cornerSize) {
    this.browserX=startX+boxWidht;
    canvas.beginPath();
    canvas.strokeStyle = '#00ffff';
    canvas.lineWidth = 2;
    canvas.shadowBlur = 2;
    canvas.shadowColor = '#002222';
    canvas.fillStyle = this.state?.autoRefresh ? (this.state?.error || this.state?.timeout ? "#f27c7c" : this.state?.connected?"#00ffff59":"#0396966b") : "#000000"
    this.roundedRectagle(canvas, startX, startY, boxWidht, boxHeight, cornerSize);
    canvas.fill();
    if (this.state?.lanDevices?.length) {
      this.roundedRectagle(canvas, startX, startY, boxWidht, boxHeight * (this.state.lanDevices.length/254), cornerSize);
    }

    canvas.stroke();
  }

  chip(canvas, startX, startY, boxWidht, boxHeight, cornerSize) {
    this.chipX=startX;
    canvas.beginPath();
    const pinWidth = 4;
    const pinHeight = 2;
    const pinVCount = 3;

    canvas.strokeStyle = '#00ffff';
    canvas.lineWidth = 2;
    canvas.shadowBlur = 2;
    canvas.shadowColor = '#000000';
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
    var anims = this.anims.filter(anim => anim.type == "log" && anim.level == ln[0]);
    var inSpot = getInSpot(anims, "chip");
    if (inSpot) {
      inSpot.weight++;
    } else {
      var msg = ln.match(/^[^IDVEW]*(.+)/)[1];
      var lvl = msg.substr(0, 1);
      this.anims.push({
            type:"log",
            from: "chip",
            level:lvl,
            weight: 1,
            lineColor: lvl == 'D' || lvl == 'I' ? "green" : ln[0] == 'W' ? "yellow" : "red",
            shadowColor: '#000000',
            fillColor: '#000000',
            textColor: lvl == 'D' || lvl == 'I' ? "green" : ln[0] == 'W' ? "yellow" : "red",
            startY: 25,
            renderer: this.drawSprite
        })
        window.requestAnimationFrame(this.drawDidget.bind(this));
    }
    this.callbacks.logCBFn.forEach(logCBFn=>logCBFn(ln));
  }

  UpdateState(state) {
    var anims = this.anims.filter(anim => anim.type == "state");
    var inSpot = getInSpot(anims, "chip");
    if (inSpot) {
      inSpot.weight++;
    } else {
      this.anims.push({
        type:"state",
        from: "chip",
        weight: 1,
        lineColor: '#00ffff',
        shadowColor: '#000000',
        fillColor: '#000000',
        textColor: '#00ffff',
        startY: 5,
        renderer: this.drawSprite
      });
      window.requestAnimationFrame(this.drawDidget.bind(this));
    }
    this.callbacks.stateCBFn.forEach(stateCBFn=>stateCBFn(state));
  }

  ProcessEvent(event) {
    var anims = this.anims.filter(anim => anim.type == "event");
    var inSpot = getInSpot(anims, "chip");
    if (inSpot) {
      inSpot.weight++;
    } else {
      this.anims.push({
          type:"event",
          from: "chip",
          eventBase: event ? event.eventBase : undefined,
          weight: 1,
          lineColor: '#00ffff',
          shadowColor: '#000000',
          fillColor: '#000000',
          textColor: '#7fffd4',
          startY: 15,
          renderer: this.drawSprite
      });
      window.requestAnimationFrame(this.drawDidget.bind(this));
    }
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
      return e(StatusPage,  { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Status"].active, registerEventInstanceCallback:this.registerEventInstanceCallback.bind(this), registerStateCallback:this.registerStateCallback.bind(this) });
    }
    if (name == "Config") {
      return e(ConfigPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["Config"].active });
    }
    if (name == "System") {
      return e(SystemPage,    { pageControler: this.state.pageControler, selectedDeviceId: this.state.selectedDeviceId, active: this.state.tabs["System"].active });
    }
    if (name == "Logs") {
      return e(LogLines, { key: "logLines", registerLogCallback:this.registerLogCallback.bind(this), active: this.state.tabs["System"].active })
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
    return e("div",{key:genUUID(),className:"mainApp"}, [
      Object.keys(this.state.tabs).map(tab => 
        e("details",{key:genUUID(),id:tab, className:"appPage slides", open: this.state.tabs[tab].active, onClick:elem=>{
            if (elem.target.classList.contains("appTab")){
              elem.target.parentElement.setAttribute("open",true);
              Object.keys(this.state.tabs).forEach(ttab => ttab == tab ? this.state.tabs[ttab].active=true : this.state.tabs[ttab].active=false );
              [].slice.call(elem.target.parentElement.parentElement.children).filter(ttab => ttab != elem).forEach(ttab => ttab.removeAttribute("open"))
            }
          }},
          [
            e("summary",{key:genUUID(),className:"appTab"},tab),
            e("div",{key:genUUID(),className: tab == "Status" ? "pageContent system-config" : "pageContent"}, this.getPage(tab))
          ]
        )
      ),
      e("canvas",{
        key: genUUID(),
        height: 40,
        width:100,
        ref: (elem) => this.widget = elem
      }),
      e('fieldset', { key: genUUID(), className:`slides`, id: "controls"}, [
        this.state?.OnLineDevices?.length && !window.location.hostname ?
          e("select",{
              key: genUUID(),
              className: "landevices",
              value: httpPrefix.substring(7),
              onChange: elem=>{httpPrefix=`http://${elem.target.value}`;this.state?.httpPrefix!=httpPrefix?this.setState({httpPrefix:httpPrefix}):null;this.ws?.close();}
              },this.state.OnLineDevices.map(lanDev=>e("option",{
                  key:genUUID(),
                  className: "landevice"
              },lanDev.ip))
            ):null,
        e(DeviceList, {
            key: genUUID(),
            selectedDeviceId: this.state.selectedDeviceId,
            httpPrefix: httpPrefix,
            onSet: val=>this.state?.selectedDeviceId!=val?this.setState({selectedDeviceId:val}):null
        })
      ])
    ]);
  }
}

// ReactDOM.render(
//   e(MainApp, {
//     key: genUUID(),
//     className: "slider"
//   }),
//   document.querySelector(".slider")
// );

ReactDOM.createRoot(document.querySelector(".slider")).render(e(MainApp, {
  key: genUUID(),
  className: "slider"
}));
function getInSpot(anims, origin) {
  return anims
        .filter(anim => anim.from == origin)
        .find(anim => (anim.x === undefined) || (origin == "browser" ? anim.x <= (app.browserX + 4 + anim.weight) : anim.x >= (app.chipX - 4- anim.weight)));
}

