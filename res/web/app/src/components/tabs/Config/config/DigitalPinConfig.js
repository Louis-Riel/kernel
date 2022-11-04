import {createElement as e, Component} from 'react';
import {Card, CardHeader, CardContent, ListItem, TextField, List } from '@mui/material';
import PinDriverFlags from './PinDriverFlag';

export default class DigitalPinConfig extends Component {
    constructor(props) {
        super(props);
        this.state = {pin: props.item};
    }

    componentDidUpdate(prevProps, prevState) {
        if ((this.state !== prevState) && this.props.onChange) {
            this.props.onChange(this.state);
        }
    }

    onChange(name, value) {
        this.state.pin[name] = name === "pinName" ? value : parseInt(value); 
        this.setState(this.state);
    }

    render() {
        return e( Card, { key: this.state.pin.pinName, className: "pin-config" },[
            e( CardHeader, {key:"header", title: this.state.pin.pinName }),
            e( CardContent, {key:"details"},  e(List,{key: "items"},
                [
                    e( ListItem, { key: "pinName" },  
                        e( TextField, { key: "pinName", autoFocus:true, value: this.state.pin.pinName, label: "Name", type: "text", onChange: event => this.onChange("pinName", event.target.value)})),
                    e( ListItem, { key: "pinNo" },  
                        e( TextField, { key:"pinNo", value: this.state.pin.pinNo, label: "PinNo", type: "number", onChange: event => this.onChange("pinNo", event.target.value) })),
                    e( PinDriverFlags, { key: "driverFlags", autoFocus:true, value: this.state.pin.driverFlags, onChange: val => this.onChange("driverFlags", val) }),
                ]
            ))
        ]);
    }
}