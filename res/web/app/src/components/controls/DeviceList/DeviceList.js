import { chipRequest } from '../../../utils/utils'
import { Component} from 'react';
import { Button, MenuItem, Select, CircularProgress } from '@mui/material';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faMagnifyingGlass } from '@fortawesome/free-solid-svg-icons'
import './DeviceList.css'

export default class DeviceList extends Component {
    constructor(props) {
        super(props);
        this.state = {
            devices : []
        }
    }

    componentDidMount() {
        this.finder = new WebSocket(process.env.REACT_APP_FINDER_SERVICE_WS);
        this.finder.onmessage = this.onMessage.bind(this);
        this.finder.onerror = console.error;
        this.finder.onclose = console.log;
    }

    onMessage(evt) {
        try {
            let msg = JSON.parse(evt.data);
            if (msg.clients) {
                this.setState({devices:msg.clients});
            }
        } catch (ex) {
            console.error(ex);
        }
    }

    getDevices() {
        this.finder.send(JSON.stringify({command:"scan"}));
    }

    processDevice(device, srcDevices, destDevices, totDevices,resolve) {
        this.getDevice(device)
            .then(device => {
                const dev = destDevices.find(dev=>dev.config.deviceid === device.config.deviceid)
                if (dev) {
                    dev.ip = device.ip;
                } else {
                    destDevices.push(device);
                    this.setState({devices:destDevices});
                }
            }).catch(err=>err)
              .finally(()=>{
                this.setState({scanProgress:((srcDevices.length*1.0)/(totDevices*1.0))})
                if (srcDevices.length && this.state.scanning) {
                    this.processDevice(srcDevices.pop(),srcDevices,destDevices, totDevices);
                }
              });
    }

    getDevice(device) {
        let abort = new AbortController();
        let timer = setTimeout(()=>abort.abort(),2000);
        return new Promise((resolve,reject) => chipRequest(`${device.devName}/config/`, {
            method: 'post',
            signal: abort.signal
        }).then(data => data.json())
          .then(config => {
              clearTimeout(timer);
              device.config = config;
              device.state = "online"
              resolve(device);
          }).catch(_err=>{
              clearTimeout(timer);
              device.state = "offline"
              reject(device);
          }));
    }

    render() {
        return <div className="scanProgress">
            {this.state?.devices?.length > 0 ?
            <Select
                className="devices"
                value= {this.props.selectedDevice.config ? this.state.devices.find(device => device.config.deviceid === this.props.selectedDevice.config.deviceid) : undefined}
                onChange= {(event) => this.props.onSet(event.target.value)}>
                    <MenuItem key={"noselection"} value={undefined}>Select a device</MenuItem>
                    {this.state?.devices
                        .map(device => <MenuItem key={device.config.devName} value={device}>{`${device.config.devName}(${device.config.deviceid})`}</MenuItem>)}
            </Select>:undefined}
            {this.state.scanning ? <CircularProgress variant="determinate" onClick={_evt => this.setState({scanning:false})} value={this.state.scanProgress*100}>33</CircularProgress > : 
                                   <Button onClick={_evt=>this.getDevices()}><FontAwesomeIcon icon={faMagnifyingGlass}></FontAwesomeIcon></Button>}
        </div>;
    }
} 
