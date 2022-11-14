import {Component} from 'react';
import {wfetch } from '../../../utils/utils';
import CryptoJS from 'crypto-js';
import { Button } from '@mui/material';
import './FirmwareUpdater.css';

export default class FirmwareUpdater extends Component {
    constructor(props) {
        super(props);
        this.state = {
            httpPrefix:this.props.selectedDevice?.ip ? `http://${this.props.selectedDevice.ip}` : ".",
        };
    }

    UploadFirmware() {
        this.setState({ loaded: `Sending ${this.state.len} firmware bytes` })
        if (this.state.fwdata && this.state.md5) {
            return wfetch(`${this.state.httpPrefix}/ota/flash?md5=${this.state.md5}&len=${this.state.len}`, {
                method: 'post',
                body: this.state.fwdata
            }).then(res => res.text())
              .then(res => this.setState({ loaded: res }));
        }
    }

    waitForDevFlashing() {
        let abort = new AbortController();
        let stopAbort = setTimeout(() => { abort.abort() }, 1000);
        wfetch(`${this.state.httpPrefix}/status/app`, {
            method: 'post',
            signal: abort.signal
        }).then(res => {
            clearTimeout(stopAbort);
            return res.json()
        })
        .then(state => this.setState({ waiter: null, loaded: `Loaded version built on ${state.build.date}` }))
        .catch(err => {
            clearTimeout(stopAbort);
            this.setState({ waiter: setTimeout(() => this.waitForDevFlashing(), 500) });
        });
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (prevProps?.selectedDevice !== this.props.selectedDevice) {
            if (this.props.selectedDevice?.ip) {
                this.setState({httpPrefix:`http://${this.props.selectedDevice.ip}`});
            } else {
                this.setState({httpPrefix:"."});
            }
        }

        if ((this.state.loaded === "Flashing") && (this.state.waiter === null)) {
            this.waitForDevFlashing();
        }

        if (this.state.firmware && !this.state.md5) {
            this.setState({md5 : "loading"});
            let reader = new FileReader();
            reader.onload = () => {
                let res = reader.resultString || reader.result;
                let md5 = CryptoJS.algo.SHA256.create();
                md5.update(CryptoJS.enc.Latin1.parse(reader.result));
                let fwdata = new Uint8Array(this.state.firmware.size);
                for (let i = 0; i < res.length; i++) {
                    fwdata[i] = res.charCodeAt(i);
                }

                this.setState({
                    md5: md5.finalize().toString(CryptoJS.enc.Hex),
                    fwdata: fwdata,
                    len: this.state.firmware.size
                });
                console.log(JSON.stringify(this.state.md5));
            };
            reader.onprogress = (evt) => this.setState({
                loaded: `${((this.state.firmware.size * 1.0) / (evt.loaded * 1.0)) * 100.0}% loaded`
            });
            reader.onerror = (evt) => this.setState({
                md5: null,
                firmware: null,
                error: evt.textContent
            });
            reader.onabort = (evt) => this.setState({
                md5: null,
                firmware: null,
                error: "Aborted"
            });
            reader.readAsBinaryString(this.state.firmware);
        }
    }

    render() {
        return (
            <div className="firmware_updater">
                <label for="firmware-upload" class="custom-file-upload">
                 <Button onClick={_=>this.HandleClick()}>
                    {this.state.fwdata?`Upload ${this.state.loaded}`:"Update Firmware"}
                 </Button>
                </label>
                <input id="firmware-upload" type= "file" name="firmware" multiple onChange={event => this.setState({ firmware: event.target.files[0] }) }></input>
            </div>
        )       
    }

    HandleClick() {
        if (this.state.fwdata) { 
            if ((this.state.loaded.indexOf("Loaded version")===0)||
                (this.state.loaded.indexOf("Not new")===0) ||
                (this.state.loaded.indexOf("Bad Checksum")===0)) {
                this.setState({
                    loaded:"",
                    firmware: undefined,
                    md5: undefined,
                    waiter: null,
                    fwdata: undefined
                });
            } else if (this.state.loaded.indexOf("100% loaded")===0) {
                this.UploadFirmware();
            }
        } else {
            document.getElementById("firmware-upload").click()
        }
    }
}
