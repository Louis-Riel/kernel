import { createRef, Component} from 'react';
import { getInSpot, getAnims, fromVersionedToPlain } from '../../../utils/utils';
import './WebSocket.css';

export default class WebSocketManager extends Component {
    constructor(props) {
        super(props);
        this.widget = createRef();
        this.state = {
            httpPrefix:this.props.selectedDevice?.ip ? `${process.env.REACT_APP_API_URI}/${this.props.selectedDevice.config.devName}` : ".",
            enabled:props.enabled
        };
        window.anims=getAnims();
    }
    
    componentDidUpdate(prevProps, prevState, snapshot) {
        if (prevProps?.selectedDevice !== this.props.selectedDevice) {
            if (this.props.selectedDevice?.ip) {
                this.setState({httpPrefix:`${process.env.REACT_APP_API_URI}/${this.props.selectedDevice.config.devName}`});
            } else {
                this.setState({httpPrefix:"."});
            }
        }

        if (prevState.httpPrefix !== this.state.httpPrefix) {
            if (this.state.enabled) {
                this.ws?.close();
            }
        }

        if ((prevState.enabled !== this.state.enabled ) || 
            (this.ws !== this.state.enabled)) {
                this.state.enabled ? this.startWs() : this.stopWs();
        }
    }

    drawDidget(){
        if (!this.widget)
            return;
            
        this.canvas.clearRect(0,0,100,40);
    
        this.browser(2, 2, 30, 30, 10);
        this.chip(70, 2, 20, 30, 5);
    
        if (window.anims.length){
            window.anims.forEach(anim => {
                this.drawSprite(anim, this.canvas);
            });

            window.anims = window.anims.filter(anim => anim.state !== 2);
            window.requestAnimationFrame(window.animRenderer);
        }

        if (this.state.connecting) {
            window.requestAnimationFrame(window.animRenderer);
        }
    }
    
    drawSprite(anim) {
        if (!anim.state) {
            anim.state = 1;
            anim.x = anim.from === "chip" ? this.chipX : this.browserX;
            anim.endX = anim.from === "chip" ? this.browserX : this.chipX;
            anim.direction=anim.from ==="browser"?1:-1;
            anim.y = anim.startY;
            anim.tripLen = 1600.0;
            anim.startWindow = performance.now();
            anim.endWindow = anim.startWindow + anim.tripLen;
        } else {
            anim.x += (anim.direction*(performance.now() - anim.startWindow)/anim.tripLen);
        }
        let width=Math.min(15,4 + (anim.weight));
        if ((anim.y-(width/2)) <= 0) {
            anim.y = width;
        }
        this.canvas.beginPath();
        this.canvas.lineWidth = 2;
        this.canvas.shadowBlur = 2;
        this.canvas.strokeStyle = anim.lineColor;
        this.canvas.shadowColor = anim.fillColor;
        this.canvas.fillStyle = anim.fillColor;
        this.canvas.moveTo(anim.x+width, anim.y);
        this.canvas.arc(anim.x, anim.y, width, 0, 2 * Math.PI);
        this.canvas.fill();
        this.canvas.stroke();

        if (anim.weight > 1) {
            let today = ""+anim.weight
            this.canvas.font = Math.min(20,8+anim.weight)+"px Helvetica";
            let txtbx = this.canvas.measureText(today);
            this.canvas.fillStyle=anim.textColor;
            this.canvas.strokeStyle = anim.textColor;
            this.canvas.fillText(today, anim.x - txtbx.width / 2, anim.y + txtbx.actualBoundingBoxAscent / 2);
        }    
        
        if (anim.direction===1?(anim.x > anim.endX):(anim.x < anim.endX)) {
            anim.state = 2;
        }
        return anim;
    }

    browser(startX, startY, boxWidht, boxHeight, cornerSize) {
        this.browserX=startX+boxWidht;
        this.canvas.beginPath();
        this.canvas.strokeStyle = '#00ffff';
        this.canvas.lineWidth = 2;
        this.canvas.shadowBlur = 2;
        this.canvas.shadowColor = '#002222';
        let idx = (Date.now()%3)+2;
        if ( this.state?.enabled) {
            if (this.state.error) {
                this.canvas.fillStyle="#f27c7c";
            } else if (this.state.connecting) {
                this.canvas.fillStyle=`#00${[idx,idx,idx,idx].join('')}`;
            } else if (this.state.connected) {
                this.canvas.fillStyle="#00ffff59";
            }
        } else {
            this.canvas.fillStyle="#000000";
        }
        this.roundedRectagle(startX, startY, boxWidht, boxHeight, cornerSize);
        this.canvas.fill();
        if (this.state?.lanDevices?.length) {
            this.roundedRectagle(startX, startY, boxWidht, boxHeight * (this.state.lanDevices.length/254), cornerSize);
        }

        this.canvas.stroke();
    }

    chip(startX, startY, boxWidht, boxHeight, cornerSize) {
        this.chipX=startX;
        this.canvas.beginPath();
        const pinWidth = 4;
        const pinHeight = 2;
        const pinVCount = 3;

        this.canvas.strokeStyle = '#00ffff';
        this.canvas.lineWidth = 2;
        this.canvas.shadowBlur = 2;
        this.canvas.shadowColor = '#000000';
        this.canvas.fillStyle = "#00ffff";
        this.roundedRectagle(startX, startY, boxWidht, boxHeight, cornerSize);

        for (let idx = 0; idx < pinVCount; idx++) {
            this.canvas.moveTo(startX,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
            this.canvas.lineTo(startX-pinWidth,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
            this.canvas.lineTo(startX-pinWidth,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
            this.canvas.lineTo(startX,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));

            this.canvas.moveTo(startX+boxWidht,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
            this.canvas.lineTo(startX+boxWidht+pinWidth,1.5*cornerSize+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
            this.canvas.lineTo(startX+boxWidht+pinWidth,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
            this.canvas.lineTo(startX+boxWidht,1.5*cornerSize+pinHeight+startY+(idx * ((boxHeight-2*cornerSize)/pinVCount)));
        }
        this.canvas.stroke();
    }

    roundedRectagle(startX, startY, boxWidht, boxHeight, cornerSize) {
        this.canvas.moveTo(startX + cornerSize, startY);
        this.canvas.lineTo(startX + boxWidht - (2 * cornerSize), startY);
        this.canvas.arcTo(startX + boxWidht, startY, startX + boxWidht, startY + cornerSize, cornerSize);
        this.canvas.lineTo(startX + boxWidht, startY + boxHeight - cornerSize);
        this.canvas.arcTo(startX + boxWidht, startY + boxHeight, startX + boxWidht - cornerSize, startY + boxHeight, cornerSize);
        this.canvas.lineTo(startX + cornerSize, startY + boxHeight);
        this.canvas.arcTo(startX, startY + boxHeight, startX, startY + boxHeight - cornerSize, cornerSize);
        this.canvas.lineTo(startX, startY + cornerSize);
        this.canvas.arcTo(startX, startY, startX + cornerSize, startY, cornerSize);
    }

    stopWs() {
        if (this.ws) {
            this.ws.close();
            this.ws = undefined;
        }
    }

    startWs() {
        if (this.state.connecting || this.state.connected) {
            return;
        }
        this.setState({connecting:true,running:false});
        let ws = this.ws = new WebSocket("wss://" + (this.state.httpPrefix === "." ? `${window.location.hostname}:${window.location.port}/${window.location.pathname}`.replaceAll(/\/+$/g,"").replaceAll(/\/\//g,"/") : this.state.httpPrefix.substring(8)) + "/ws");
        let stopItWithThatShit = setTimeout(() => { console.log("Main timeout"); ws.close(); this.setState({connecting: false}); }, 3600);
        
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
        this.ws = null;
        this.setState({connected:false,connecting:false});
        window.animRenderer && window.requestAnimationFrame(window.animRenderer);
    }
    
    wsError(err, stopItWithThatShit, ws) {
        console.error(err);
        clearTimeout(stopItWithThatShit);
        this.setState({error: err});
        window.animRenderer && window.requestAnimationFrame(window.animRenderer);
        ws.close();
    }
    
    wsOpen(stopItWithThatShit, ws) {
        clearTimeout(stopItWithThatShit);
        this.setState({connected:true,connecting:false});
        ws.send("Connected");
        window.animRenderer && window.requestAnimationFrame(window.animRenderer);
        stopItWithThatShit = setTimeout(() => { this.setState({timeout: "Connect"}); ws.close(); console.log("Connect timeout"); }, 3600);
        return stopItWithThatShit;
    }
    
    AddLogLine(ln) {
        let anims = window.anims.filter(anim => anim.type === "log" && anim.level === ln[0]);
        let inSpot = getInSpot(anims, "chip");
        if (inSpot) {
            inSpot.weight++;
        } else {
            let msg = ln.match(/^[^IDVEW]*(.+)/)[1];
            let lvl = msg.substr(0, 1);
            window.anims.push({
                type:"log",
                from: "chip",
                level:lvl,
                weight: 1,
                lineColor: lvl === 'D' || lvl === 'I' ? "green" : ln[0] === 'W' ? "yellow" : "red",
                shadowColor: '#000000',
                fillColor: '#000000',
                textColor: lvl === 'D' || lvl === 'I' ? "green" : ln[0] === 'W' ? "yellow" : "red",
                startY: 25
            })
            window.requestAnimationFrame(window.animRenderer);
        }
        this.props?.logCBFns?.forEach(logCBFn=>logCBFn(ln));
    }

    UpdateState(state) {
        let anims = window.anims.filter(anim => anim.type === "state");
        let inSpot = getInSpot(anims, "chip");
        if (inSpot) {
            inSpot.weight++;
        } else {
            window.anims.push({
            type:"state",
            from: "chip",
            weight: 1,
            lineColor: '#00ffff',
            shadowColor: '#000000',
            fillColor: '#000000',
            textColor: '#00ffff',
            startY: 5
            });
            window.requestAnimationFrame(window.animRenderer);
        }
        this.props?.stateCBFns?.forEach(stateCBFn=>stateCBFn(state));
    }

    ProcessEvent(event) {
        let anims = window.anims.filter(anim => anim.type === "event");
        let inSpot = getInSpot(anims, "chip");
        if (inSpot) {
            inSpot.weight++;
        } else {
            window.anims.push({
                type:"event",
                from: "chip",
                eventBase: event ? event.eventBase : undefined,
                weight: 1,
                lineColor: '#00ffff',
                shadowColor: '#000000',
                fillColor: '#000000',
                textColor: '#7fffd4',
                startY: 15
            });
            window.requestAnimationFrame(window.animRenderer);
        }
        this.props?.eventCBFns?.forEach(eventCBFn=>eventCBFn.fn(event));
    }
    
    processMessage(stopItWithThatShit, event, ws) {
        clearTimeout(stopItWithThatShit);
        if (!this.state.running || this.state.timeout) {
            this.setState({
                running: true,
                error: null,
                timeout: null
            });
        }
    
        if (event && event.data) {
          if (event.data[0] === "{") {
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
        stopItWithThatShit = setTimeout(() => { 
            this.setState({timeout: "Message"}); 
            ws.close(); 
            console.log("Message timeout");
        }, 3600);
        return stopItWithThatShit;
    }
    
    componentDidMount() {
        if (!this.mounted){
            this.canvas = this.widget.current.getContext("2d");

            window.animRenderer = this.drawDidget.bind(this);
            window.requestAnimationFrame(window.animRenderer);
        }
        this.mounted = true;
    }

    render() {
        return (
            <div className="wsctrl"  width="100" height="40" onClick={evt=>this.setState({enabled:!this.state.enabled})}><canvas className="wsctrl" width="100" height="40" ref={this.widget}></canvas></div>
        )
    }
}