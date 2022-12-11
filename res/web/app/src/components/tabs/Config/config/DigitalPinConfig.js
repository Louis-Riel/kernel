import {Component} from 'react';
import {TextField, Paper, Typography } from '@mui/material';
import PinDriverFlags from './PinDriverFlag';

export default class DigitalPinConfig extends Component {
    constructor(props) {
        super(props);
        this.state = {pin: props.item};
    }

    componentDidUpdate(prevProps, prevState) {
        if ((this.state !== prevState) && this.props.onChange) {
            this.props.onChange(this.state.pin);
        }

        if (JSON.stringify(this.props.item) !== JSON.stringify(prevProps.item)) {
            this.setState({pin: this.props.item});
        }
    }

    onChange(name, value) {
        this.setState({pin: {...this.state.pin, [name]:name === "pinName" ? value : parseInt(value)}});
    }

    render() {
        return <Paper variant='outlined' elevation={3} className="pin-config">
            <div className='config-header'>
                <Typography variant='h5'>{this.state.pin.pinName}</Typography>
            </div>
            <div className='config-content'>
                <TextField 
                    value={this.state.pin.pinName}
                    label="Name"
                    type="text"
                    onChange={event => this.onChange("pinName", event.target.value)}/>
                <TextField 
                    value={this.state.pin.pinNo}
                    label="PinNo"
                    type="number"
                    onChange={event => this.onChange("pinNo", event.target.value)}/>
                <PinDriverFlags 
                    autoFocus={true}
                    value={this.state.pin.driverFlags}
                    onChange={val => this.onChange("driverFlags", val)}/>
            </div>
        </Paper>
    }
}