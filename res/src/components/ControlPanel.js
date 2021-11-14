
class ControlPanel extends React.Component {
    constructor(props) {
        super(props);
        this.anims=[];
        this.state = {
            autoRefresh:props.autoRefresh
        }

        if (!this.props?.OnLineDevices?.length && (!httpPrefix && !window.location.hostname)){
            this.lookForDevs();
        } else if (this.props?.autoRefresh) {
            console.log(`from mount ${JSON.stringify(this.state)}`);
            this.openWs();
        }
    }

    componentDidMount() {
        this.mountWidget();
        window.requestAnimationFrame(this.drawDidget.bind(this));
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        this.mountWidget();
    }


    lookForDevs() {
        if (this.state?.scanning) {
            return
        }
        this.setState({scanning:true});
        var devices = [];
        for (var idx = 254; idx > 0; idx--) {
            devices.push(`192.168.1.${idx}`);
        }
        console.log(`Scanning for ${devices.length} devices`);
        var foundDevices=[];
        for (var idx = 0; idx < Math.min(10, devices.length); idx++) {
            if (devices.length) {
                this.scanForDevices(devices,foundDevices);
            }
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
                    foundDevices.push(dev);
                  }
                  if (devices.length) {
                    this.scanForDevices(devices,foundDevices);
                  } else {
                    if (!this.props?.OnLineDevices?.length) {
                        httpPrefix=`http://${foundDevices[0].devName}`
                        if (this.state?.autoRefresh && !this.state.connecting && !this.state.connected)
                            console.log("from scan finished good");
                            this.props.OnToggleAutoRefresh(true);
                        this.props.OnLiveDevChange(foundDevices);
                    }
                  }
                })
              .catch(err => {
                if (devices.length) {
                    this.scanForDevices(devices,foundDevices);
                } else {
                    if (!this.props?.OnLineDevices?.length) {
                        httpPrefix=`http://${foundDevices[0].devName}`
                        if (this.state?.autoRefresh && !this.state.connecting && !this.state.connected)
                            console.log("from scan finished bad");
                            this.props.OnToggleAutoRefresh(true);
                        this.props.OnLiveDevChange(foundDevices);
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
                this.props.OnToggleAutoRefresh(!this.state?.autoRefresh);
            }
        });
    }

    closeWs() {
        this.props.OnToggleAutoRefresh(off);
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
    }

    openWs() {
        if (!window.location.hostname && !httpPrefix) {
            console.log("Not ws as not connected");
            return;
        }
        if (this.state?.connecting || this.state?.connected) {
            return;
        }
        console.log(`Connecting to ${httpPrefix || window.location.hostname } ${JSON.stringify(this.state)}...`);
        this.setState({connecting:true,running:false});
        console.log(`Connecting to ${httpPrefix || window.location.hostname } ${JSON.stringify(this.state)}`);
        var ws = this.ws = new WebSocket("ws://" + (httpPrefix == "" ? window.location.hostname : httpPrefix.substring(7)) + "/ws");
        var stopItWithThatShit = setTimeout(() => { console.log("Main timeout"); ws.close(); this.setState({connecting:false});}, 3000);
        ws.onmessage = (event) => {
            clearTimeout(stopItWithThatShit);
            if (!this.state.running || this.state.timeout) {
                console.log("Running");
                this.setState({ running: true, error: null, timeout: null });
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
            stopItWithThatShit = setTimeout(() => { this.setState({timeout:"Message"}); ws.close();console.log("Message timeout")},3000)
        };
        ws.onopen = () => {
            clearTimeout(stopItWithThatShit);
            this.setState({ connected: true, connecting:false });
            ws.send("Connected");
            console.log("Connected");
            stopItWithThatShit = setTimeout(() => { this.setState({timeout:"Connect"}); ws.close();console.log("Connect timeout")},3000)
        };
        ws.onerror = (err) => {
            console.error(err);
            clearTimeout(stopItWithThatShit);
            this.setState({error: err});
            ws.close();
        };
        ws.onclose = (evt => {
            console.log("Closed");
            clearTimeout(stopItWithThatShit);
            this.setState({connected:false,connecting:false});
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
        if (ln && this.props.logCBFn) {
            this.props.logCBFn.forEach(logCBFn=>logCBFn(ln));
        }
    }
    
    UpdateState(state) {
        var anims = this.anims.filter(anim => anim.type == "state");
        if (anims.length > 4) {
            (anims.find(anim => anim.weight < 3) || anims[0]).weight++;
        } else {
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
        }
      if (this.props.stateCBFn) {
        this.props.stateCBFn.forEach(stateCBFn=>stateCBFn(state));
      }
    }
  
    ProcessEvent(event) {
        var anims = this.anims.filter(anim => anim.type == "event");
        var curAnims = anims.filter(anim => anim.eventBase == event.eventBase);
        if ((anims.length > 4) && (curAnims.length>1)) {
            (curAnims.find(anim => anim.weight < 3) || curAnims[0]).weight++;
        } else {
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
        }

        if (this.props.eventCBFn) {
            this.props.eventCBFn.forEach(eventCBFn=>eventCBFn(event));
        }
    }

    render() {
     return e('fieldset', { key: genUUID(), className:`slides`, id: "controls", key: this.id }, [
        this.props?.OnLineDevices?.length && !window.location.hostname ?
            e("select",{
                key: genUUID(),
                className: "landevices",
                value: httpPrefix.substring(7),
                onChange: elem=>{httpPrefix=`http://${elem.target.value}`;this.props.ReloadPage()}
            },this.props.OnLineDevices.map(lanDev=>e("option",{
                key:genUUID(),
                className: "landevice"
            },lanDev.devName))):null,
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
     ]);
    }
}
