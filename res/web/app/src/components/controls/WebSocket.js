import {createElement as e, createRef, Component} from 'react';
import { getInSpot, getAnims } from '../../utils/utils';
import { fromVersionedToPlain } from '../../utils/utils';
import './WebService.css';

var httpPrefix = "";

export default class WebSocketManager extends Component {
    constructor(props) {
        super(props);
        this.widget = createRef();
        this.state = {autoRefresh:true};
        window.anims=getAnims();
        if (this.state?.autoRefresh && (httpPrefix || window.location.hostname)) {
            this.openWs();
        }
    }
    
    drawDidget(){
        if (!this.widget)
            return;
            
        var canvas = this.widget.current.getContext("2d");
        canvas.clearRect(0,0,100,40);
    
        this.browser(canvas, 2, 2, 30, 30, 10);
        this.chip(canvas, 70, 2, 20, 30, 5);
    
        if (window.anims.length){
            window.anims.forEach(anim => {
                this.drawSprite(anim, canvas);
            });

            window.anims = window.anims.filter(anim => anim.state != 2);
            window.requestAnimationFrame(window.animRenderer);
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
        canvas.fillStyle = this.state?.autoRefresh ? (this.state?.error || this.state?.timeout ? "#f27c7c" : this.state?.connected?"#00ffff59":"#000000") : "#000000"
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
        var anims = window.anims.filter(anim => anim.type == "log" && anim.level == ln[0]);
        var inSpot = getInSpot(anims, "chip");
        if (inSpot) {
            inSpot.weight++;
        } else {
            var msg = ln.match(/^[^IDVEW]*(.+)/)[1];
            var lvl = msg.substr(0, 1);
            window.anims.push({
                type:"log",
                from: "chip",
                level:lvl,
                weight: 1,
                lineColor: lvl == 'D' || lvl == 'I' ? "green" : ln[0] == 'W' ? "yellow" : "red",
                shadowColor: '#000000',
                fillColor: '#000000',
                textColor: lvl == 'D' || lvl == 'I' ? "green" : ln[0] == 'W' ? "yellow" : "red",
                startY: 25
            })
            window.requestAnimationFrame(window.animRenderer);
        }
        this.props?.logCBFns?.forEach(logCBFn=>logCBFn(ln));
    }

    UpdateState(state) {
        var anims = window.anims.filter(anim => anim.type == "state");
        var inSpot = getInSpot(anims, "chip");
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
        var anims = window.anims.filter(anim => anim.type == "event");
        var inSpot = getInSpot(anims, "chip");
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
    
    componentDidMount() {
        this.widget.current.addEventListener("click", this.handleClick.bind(this));
        if (!this.mounted){
            window.animRenderer = this.drawDidget.bind(this);
            window.requestAnimationFrame(window.animRenderer);
        }

        this.mounted = true;
    }

    handleClick(event) {
        if ((event.offsetX > 2) &&
            (event.offsetX < 32) &&
            (event.offsetY > 2) &&
            (event.offsetY < 32)) {
            this.state.autoRefresh = !this.state.autoRefresh;
            this.state.autoRefresh ? this.openWs() : this.closeWs();
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
        window.requestAnimationFrame(window.animRenderer);
        if (this.state.autoRefresh)
          this.openWs();
      }
    
      wsError(err, stopItWithThatShit, ws) {
        console.error(err);
        clearTimeout(stopItWithThatShit);
        this.state.error = err;
        window.requestAnimationFrame(window.animRenderer);
        ws.close();
      }
    
      wsOpen(stopItWithThatShit, ws) {
        clearTimeout(stopItWithThatShit);
        this.state.connected = true;
        this.state.connecting = false;
        ws.send("Connected");
        window.requestAnimationFrame(window.animRenderer);
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
    
    render() {
        return (
            <canvas className="wsctrl" width="100" height="40" onClick={this.handleClick.bind(this)} ref={this.widget}></canvas>
        )
    }
}