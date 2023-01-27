import { createRef, Component} from 'react';
import { getInSpot, getAnims, fromVersionedToPlain, isStandalone } from '../../../utils/utils';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import './WebSocket.css';
import { faCamera } from '@fortawesome/free-solid-svg-icons';

export default class WebSocketManager extends Component {
    constructor(props) {
        super(props);
        this.widget = createRef();
        this.camera = createRef();
        this.state = {
            httpPrefix:this.props.selectedDevice?.ip ? `${process.env.REACT_APP_API_URI}/${this.props.selectedDevice.config.devName}` : "",
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

        if (this.state?.streaming && (prevState.connected !== this.state.connected)) {
            if (!this.state.connected) {
                this.setState({streaming:this.state.connected});
                this.setState({stream_zoomed:this.state.connected});
            }
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
        let ws = this.ws = new WebSocket(`ws${isStandalone()? '' : 's'}://` + (this.state.httpPrefix === "" ? `${window.location.hostname}:${window.location.port}`.replaceAll(/\/+$/g,"").replaceAll(/\/\//g,"/") : this.state.httpPrefix.substring(8)) + "/ws");
        ws.binaryType = 'arraybuffer';
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

    // Converts an ArrayBuffer directly to base64, without any intermediate 'convert to string then
// use window.btoa' step. According to my tests, this appears to be a faster approach:
// http://jsperf.com/encoding-xhr-image-data/5

/*
MIT LICENSE
Copyright 2011 Jon Leighton
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

 base64ArrayBuffer(arrayBuffer) {
    var base64    = ''
    var encodings = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
  
    var bytes         = new Uint8Array(arrayBuffer)
    var byteLength    = bytes.byteLength
    var byteRemainder = byteLength % 3
    var mainLength    = byteLength - byteRemainder
  
    var a, b, c, d
    var chunk
  
    // Main loop deals with bytes in chunks of 3
    for (var i = 0; i < mainLength; i = i + 3) {
      // Combine the three bytes into a single integer
      chunk = (bytes[i] << 16) | (bytes[i + 1] << 8) | bytes[i + 2]
  
      // Use bitmasks to extract 6-bit segments from the triplet
      a = (chunk & 16515072) >> 18 // 16515072 = (2^6 - 1) << 18
      b = (chunk & 258048)   >> 12 // 258048   = (2^6 - 1) << 12
      c = (chunk & 4032)     >>  6 // 4032     = (2^6 - 1) << 6
      d = chunk & 63               // 63       = 2^6 - 1
  
      // Convert the raw binary segments to the appropriate ASCII encoding
      base64 += encodings[a] + encodings[b] + encodings[c] + encodings[d]
    }
  
    // Deal with the remaining bytes and padding
    if (byteRemainder === 1) {
      chunk = bytes[mainLength]
  
      a = (chunk & 252) >> 2 // 252 = (2^6 - 1) << 2
  
      // Set the 4 least significant bits to zero
      b = (chunk & 3)   << 4 // 3   = 2^2 - 1
  
      base64 += encodings[a] + encodings[b] + '=='
    } else if (byteRemainder === 2) {
      chunk = (bytes[mainLength] << 8) | bytes[mainLength + 1]
  
      a = (chunk & 64512) >> 10 // 64512 = (2^6 - 1) << 10
      b = (chunk & 1008)  >>  4 // 1008  = (2^6 - 1) << 4
  
      // Set the 2 least significant bits to zero
      c = (chunk & 15)    <<  2 // 15    = 2^4 - 1
  
      base64 += encodings[a] + encodings[b] + encodings[c] + '='
    }
    
    return base64
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
          if (event.data instanceof ArrayBuffer) {
            this.camera.current.src = 'data:image/jpg;base64,'+ this.base64ArrayBuffer(event.data)
          } else if (event.data[0] === "{") {
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
        if (this.hasCamera()) {
            return <div className='websocketbar'>{this.getCameraControl()}{this.getWsControl()}</div>
        } else {
            return this.getWsControl()
        }
    }

    hasCamera() {
        return this.props.selectedDevice?.config?.Cameras?.length > 0;
    }

    streamCamera() {
        this.ws.send("STREAM:" + this.props.selectedDevice.config.Cameras[0].type);
        this.setState({streaming:true});
    }

    getCameraControl() {
        return <div className={this.state?.streaming ? "streaming" : "off"}>
                    <img className={this.state.stream_zoomed ? "viewer zoomed" : "viewer"} ref={this.camera} alt="Camera" onClick={_evt => this.setState({stream_zoomed:!this.state.stream_zoomed})}></img>
                    <FontAwesomeIcon className='trigger' icon={faCamera} onClick={_evt => this.streamCamera()}></FontAwesomeIcon>
                </div>
    }

    getWsControl() {
        return <div className="wsctrl" width="100" height="40" onClick={evt => this.setState({ enabled: !this.state.enabled })}><canvas className="wsctrl" width="100" height="40" ref={this.widget}></canvas></div>;
    }
}