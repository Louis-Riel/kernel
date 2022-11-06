import {createElement as e, Component} from 'react';
import { Snackbar, Alert, Paper, Typography, CardContent, List, ListItem, TextField, MenuItem, InputLabel, Select } from '@mui/material';

export default class AnalogPinConfig extends Component {
    constructor(props) {
        super(props);
        this.state = {pin: props.item,errors:[]};
        this.state.pin.channel = this.state.pin.channel ? this.state.pin.channel : this.pinNoToChannel(this.state.pin.pinNo);
        this.state.pin.channel_width = this.state.pin.channel_width ? this.state.pin.channel_width : 9;
        this.state.pin.channel_atten = this.state.pin.channel_atten ? this.state.pin.channel_atten : 0.0;
        this.state.pin.waitTime = this.state.pin.waitTime ? this.state.pin.waitTime : 10000;
        this.state.pin.minValue = this.state.pin.minValue ? this.state.pin.minValue : 0;
        this.state.pin.maxValue = this.state.pin.maxValue ? this.state.pin.maxValue : 4096;
    }

    componentDidUpdate(prevProps, prevState) {
        if ((this.state !== prevState) && this.props.onChange) {
            this.props.onChange(this.state);
        }
        
        if (prevProps?.item !== this.props?.item) {
            this.setState({pin: this.props.item});
        }
    }

    getErrors() {
        return this.state.errors.map((val,idx) => e(Snackbar,{
            key:`error${idx}`, 
            className:"popuperror", 
            anchorOrigin: {vertical: "top", horizontal: "right"}, 
            autoHideDuration: 3000,
            onClose: (event, reason) => {val.visible=false; this.setState(this.state);},
            open: val.visible},e(Alert,{key:`error${idx}`, severity: "error"}, val.error)));
    }

    onChange(name, value) {
        this.state.pin[name] = name === "name" ? value : name === "channel_atten" ? parseFloat(value) : parseInt(value); 
        if (name === "pinNo") {
            this.state.pin.channel = this.pinNoToChannel(this.state.pin.pinNo);
        }
        this.setState(this.state);
        if ((this.state.pin.channel_width < 9) || (this.state.pin.channel_width > 12)) {
            setTimeout(() => {this.state.errors.push({visible:true, error:`Invalid channel width for ${this.state.pin.name}, needs to bebetween 9 and 12`});this.setState(this.state)}, 300);
        }
    }

    pinNoToChannel(pinNo) {
        switch(pinNo) {
            case 32:
                return 4;
            case 33:
                return 5;
            case 34:
                return 6;
            case 35:
                return 7;
            case 36:
                return 0;
            case 37:
                return 1;
            case 38:
                return 2;
            case 39:
                return 3;
            default:
                this.state.errors.push({visible:true, error:`Invalid pin number, ${pinNo} cannot be an analog pin`});
                setTimeout(() => {this.state.errors.push({visible:true, error:`Invalid pin number, ${pinNo} cannot be an analog pin`});this.setState(this.state)}, 300);
                return -1;
        }
    }

    render() {
        return <Paper variant='outlined' elevation={3} className="pin-config">
            <div className='config-header'>
                <Typography variant='h5'>{this.state.pin.name}</Typography>
            </div>
            <div className='config-content'>
                <TextField 
                    value={this.state.pin.name}
                    label="Name"
                    type="text"
                    onChange={event => this.onChange("name", event.target.value)}/>
                <TextField 
                    value={this.state.pin.pinNo}
                    label="PinNo"
                    type="number"
                    onChange={event => this.onChange("pinNo", event.target.value)}/>
                <TextField 
                    value={this.state.pin.channel}
                    label="Channel"
                    inputProps={{readOnly: true}}
                    type="number"
                    onChange={event => this.onChange("channel", parseInt(event.target.value))}/>
                <TextField 
                    value={this.state.pin.channel_width}
                    label="Channel Width"
                    type="number"
                    onChange={event => this.onChange("channel_width", parseInt(event.target.value))}/>
                <div>
                    <InputLabel
                        className='ctrllabel'>Channel Attennuation</InputLabel>
                </div>
                <Select 
                    value={this.state.pin.channel_atten} 
                    label="Channel Attennuation" 
                    onChange={event => this.onChange("channel_atten", parseFloat(event.target.value))}>
                    <MenuItem value={0.0}>0 dB - 100 mV ~ 950 mV</MenuItem>
                    <MenuItem value={2.5}>2.5 dB - 100 mV ~ 1250 mV</MenuItem>
                    <MenuItem value={6.0}>6.0 dB - 100 mV ~ 1750 mV</MenuItem>
                    <MenuItem value={11.0}>11.0 dB - 100 mV ~ 2450 mV</MenuItem>
                </Select>
                <TextField 
                    value={this.state.pin.piwaitTimenNo}
                    label="Wait Time"
                    type="number"
                    onChange={event => this.onChange("waitTime", parseInt(event.target.value))}/>
                <TextField 
                    value={this.state.pin.minValue}
                    label="Maximum"
                    type="number"
                    onChange={event => this.onChange("minValue", parseInt(event.target.value))}/>
                <TextField 
                    value={this.state.pin.maxValue}
                    label="Maximum"
                    type="number"
                    onChange={event => this.onChange("maxValue", parseInt(event.target.value))}/>
                {this.getErrors()}
            </div>
        </Paper>


        // return e( Card, { key: this.state.pin.pinName, className: "pin-config" },[
        //     e( CardHeader, {key:"header", title: this.state.pin.name }),
        //     e( CardContent, {key:"details"},  e(List,{key: "items"},
        //         [
        //             e( ListItem, { key: "pinName" },  
        //                 e( TextField, { key: "pinName", autoFocus:true, value: this.state.pin.name, label: "Name", type: "text", onChange: event => this.onChange("name", event.target.value)})),
        //             e( ListItem, { key: "pinNo" },  
        //                 e( TextField, { key:"pinNo", value: this.state.pin.pinNo, label: "PinNo", type: "number", onChange: event => this.onChange("pinNo", parseInt(event.target.value) )})),
        //             e( ListItem, { key: "channel" },  
        //                 e( TextField, { key:"channel", inputProps:{ readOnly: true },value: this.state.pin.channel, label: "Channel", type: "number", onChange: event => this.onChange("channel", parseInt(event.target.value)) })),
        //             e( ListItem, { key: "channel_width" },  
        //                 e( TextField, { key:"channel_width", value: this.state.pin.channel_width, label: "Channel Width", type: "number", min:9, max:12, onChange: event => this.onChange("channel_width", parseInt(event.target.value)) })),
        //             e( ListItem, { key: "channel_atten" },
        //                 [
        //                     e(InputLabel,{key:"attenLabel", id:"channel_atten_label", className: "ctrllabel"}, "Channel Attennuation"),
        //                     e(Select,{key:"attenSelect", value: this.state.pin.channel_atten, label: "Channel Attennuation", onChange: event => this.onChange("channel_atten", parseFloat(event.target.value))},[
        //                         e(MenuItem,{key:"atten0", value: 0.0}, "0 dB - 100 mV ~ 950 mV"),
        //                         e(MenuItem,{key:"atten1", value: 2.5}, "2.5 dB - 100 mV ~ 1250 mV"),
        //                         e(MenuItem,{key:"atten2", value: 6.0}, "6.0 dB - 100 mV ~ 1750 mV"),
        //                         e(MenuItem,{key:"atten3", value: 11.0}, "11.0 dB - 100 mV ~ 2450 mV"),
        //                     ])
        //                 ]),
        //             e( ListItem, { key: "waitTime" },  
        //                 e( TextField, { key:"waitTime", value: this.state.pin.waitTime , label: "Wait Time", type: "number", onChange: event => this.onChange("waitTime", parseInt(event.target.value) )})),
        //             e( ListItem, { key: "min" },  
        //                 e( TextField, { key:"min", value: this.state.pin.minValue , label: "Minimum", type: "number", onChange: event => this.onChange("minValue", parseInt(event.target.value) )})),
        //             e( ListItem, { key: "max" },  
        //                 e( TextField, { key:"max", value: this.state.pin.maxValue , label: "Maximum", type: "number", onChange: event => this.onChange("maxValue", parseInt(event.target.value) )})),
        //         ]
        //     )),
        //     this.getErrors()
        // ]);
    }
}