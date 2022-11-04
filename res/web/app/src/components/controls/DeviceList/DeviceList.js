import { wfetch } from '../../../utils/utils'
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

        this.getLocalConfig()
            .then(config => {
                let dev ={config:config,state:"online" };
                this.setState({config:config, devices: [dev]});
                this.props.onSet && this.props.onSet(dev);
            }).catch(console.error);
    }

    getDevices() {
        this.setState({scanning:true,scanProgress:1});

        new Promise((resolve,reject) => {
            let devices = [];
            let srcDevices = Array.from(Array(253).keys()).sort((a,b)=>{
                    let ah = Math.floor(a/100);
                    let bh = Math.floor(b/100);
                    if (ah !== bh) {
                        if ((ah === 1) && (bh !== 1)) {
                            return -1;
                        } else if ((bh === 1) && (ah !== 1)) {
                            return 1;
                        }
                    }
                    return a > b ? 1 : (a < b ? -1 : 0);
                }).map(num=>{return {
                    ip: `192.168.0.${num+1}`,
                    state: "unscanned"
                }}).reverse();
            Array.from(Array(5).keys()).map(idx => this.processDevice(srcDevices.pop(),srcDevices,devices,srcDevices.length,resolve));
        }).finally(() => this.setState({scanning:false}));
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

    getLocalConfig() {
        let abort = new AbortController();
        let timer = setTimeout(()=>abort.abort(),4000);
        return new Promise((resolve,reject) => wfetch(`/config/`, {
            method: 'post',
            signal: abort.signal
        }).then(data => data.json())
          .then(config => {
              clearTimeout(timer);
              resolve(config);
          }).catch(err=>{
              clearTimeout(timer);
              reject(err);
          }));
    }

    getDevice(device) {
        let abort = new AbortController();
        let timer = setTimeout(()=>abort.abort(),2000);
        return new Promise((resolve,reject) => wfetch(`http://${device.ip}/config/`, {
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
            {this.state?.devices?.filter(device => device.state === "online")?.length > 0 ?
            <Select
                key= "devices"
                value= {this.props.selectedDeviceId ? this.state.devices.find(device => device.config.deviceid.value === this.props.selectedDeviceId) : undefined}
                onChange= {(event) => this.props.onSet(event.target.value)}>
                {this.state?.devices
                    .filter(device => device.state === "online")
                    .map(device => <MenuItem value={device}>{`${device.config.devName.value}(${device.config.deviceid.value})`}</MenuItem>)}
            </Select>:undefined}
            {this.state.scanning ? <CircularProgress variant="determinate" onClick={_evt => this.setState({scanning:false})} value={this.state.scanProgress*100}>33</CircularProgress > : 
                                   <Button onClick={_evt=>this.getDevices()}><FontAwesomeIcon icon={faMagnifyingGlass}></FontAwesomeIcon></Button>}
        </div>;
    }
} 
