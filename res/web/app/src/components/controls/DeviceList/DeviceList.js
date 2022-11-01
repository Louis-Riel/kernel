import {fromVersionedToPlain, wfetch } from '../../../utils/utils'
import {createElement as e, Component} from 'react';
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
            .then(fromVersionedToPlain)
            .then(config => {
                var dev;
                this.setState({config:config, devices: [(dev={config:config,state:"online" })]});
                this.props.onSet && this.props.onSet(dev);
            }).catch(console.error);
    }

    getDevices() {
        this.setState({scanning:true,scanProgress:1});

        return new Promise((resolve,reject) => {
            var devices = [];
            var srcDevices = Array.from(Array(253).keys()).map(num=>{return {
                                                                                ip: `192.168.1.${num+1}`,
                                                                                state: "unscanned"
                                                                            }}).sort((a,b)=>(1-(Math.random()*2)));
            Array.from(Array(5).keys()).map(idx => this.processDevice(srcDevices.pop(),srcDevices,devices,srcDevices.length,resolve));
        });
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
                if (srcDevices.length) {
                    resolve(this.processDevice(srcDevices.pop(),srcDevices,destDevices, totDevices));
                } else {
                    resolve(destDevices);
                }
              });
    }

    getLocalConfig() {
        var abort = new AbortController();
        var timer = setTimeout(()=>abort.abort(),4000);
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
        var abort = new AbortController();
        var timer = setTimeout(()=>abort.abort(),2000);
        return new Promise((resolve,reject) => wfetch(`http://${device.ip}/config/`, {
            method: 'post',
            signal: abort.signal
        }).then(data => data.json())
          .then(fromVersionedToPlain)
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
            {this.state?.devices?.filter(device => device.state === "online")?.length > 1 ?
            <Select
                key= "devices"
                value= {this.props.selectedDevice?.config?.deviceid ? this.state.devices.find(device => device.config.deviceid === this.props.selectedDevice.config.deviceid) : 
                                                                      this.state.devices.find(device => device.config.deviceid === this.state.config.deviceid)}
                onChange= {(event) => this.props.onSet(event.target.value)}>
             {(this.state?.devices||this.props.devices)
                    .filter(device => device.state === "online")
                    .map(device => <MenuItem value={device}>{`${device.config.devName}(${device.config.deviceid})${device.ip ? ` - ${device.ip}`:''}`}</MenuItem>)}
            </Select>:null}
            {this.state.scanning ? <CircularProgress variant="determinate" value={this.state.scanProgress*100}>33</CircularProgress > : 
                                   <Button onClick={_evt=>this.getDevices()}><FontAwesomeIcon icon={faMagnifyingGlass}></FontAwesomeIcon></Button>}
        </div>;
    }
}
