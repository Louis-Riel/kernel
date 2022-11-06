import {createElement as e, Component} from 'react';
import {Snackbar,Alert,Card,CardHeader,CardContent,List, Chip, Divider, Paper, Typography} from '@mui/material';

export default class PinDriverFlags extends Component {
    constructor(props) {
        super(props);
        this.state = {
            digital_in: { value: props.value &  0b00000001 ? true : false, errors:[] },
            digital_out: { value: props.value & 0b00000010 ? true : false, errors:[] },
            pullup: { value: props.value &      0b00000100 ? true : false, errors:[] },
            pulldown: { value: props.value &    0b00001000 ? true : false, errors:[] },
            touch: { value: props.value &       0b00010000 ? true : false, errors:[] },
            wakeonhigh: { value: props.value &  0b00100000 ? true : false, errors:[] },
            wakeonlow: { value: props.value &   0b01000000 ? true : false, errors:[] }
        };
    }

    getValue(state) {
        return state.digital_in.value | 
               (state.digital_out.value << 1) | 
               (state.pullup.value << 2) | 
               (state.pulldown.value << 3) | 
               (state.touch.value << 4) | 
               (state.wakeonhigh.value << 5) | 
               (state.wakeonlow.value << 6);
    }

    getFlagNames(value) {
        return [
            value &  0b00000001 ? "digital_in" : "",
            value & 0b00000010 ? "digital_out" : "",
            value &      0b00000100 ? "pullup" : "",
            value &    0b00001000 ? "pulldown" : "",
            value &       0b00010000 ? "touch" : "",
            value &  0b00100000 ? "wakeonhigh" : "",
            value &   0b01000000 ? "wakeonlow" : ""
        ].filter(x => x.length > 0).join(", ");
    }

    componentDidUpdate(prevProps, prevState) {
        if (this.props.value !== this.getValue(this.state)) {
            this.setState({
                digital_in: { value: this.props.value &  0b00000001 ? true : false, errors:[] },
                digital_out: { value: this.props.value & 0b00000010 ? true : false, errors:[] },
                pullup: { value: this.props.value &      0b00000100 ? true : false, errors:[] },
                pulldown: { value: this.props.value &    0b00001000 ? true : false, errors:[] },
                touch: { value: this.props.value &       0b00010000 ? true : false, errors:[] },
                wakeonhigh: { value: this.props.value &  0b00100000 ? true : false, errors:[] },
                wakeonlow: { value: this.props.value &   0b01000000 ? true : false, errors:[] }
            });
        }
    }

    isValidChange(name, value) {
        let newState =  JSON.parse(JSON.stringify(this.state));
        newState[name].value = value;
        if ((name === "digital_in" || name === "digital_out") && value) {
            if ((name === "digital_in") && newState.digital_out.value) {
                this.state.digital_out.value = false;
            }
            if ((name === "digital_out") && newState.digital_in.value) {
                this.state.digital_in.value = false;
                this.state.pullup.value = false;
                this.state.pulldown.value = false;
                this.state.touch.value = false;
                this.state.wakeonhigh.value = false;
                this.state.wakeonlow.value = false;
            }
        }
        if (newState.digital_out.value && this.getValue(newState) & 0b01111100) {
            this.state[name].errors.push({visible: true, error: "Can't set " + this.getFlagNames(this.getValue(newState) & 0b01111100) + " option for output pins"});
            Promise.resolve(this.setState(this.state));
            return false;
        }

        if (newState.touch.value && (this.getValue(newState) & 0b00001100)) {
            this.state[name].errors.push({visible: true, error: "Can't set " + this.getFlagNames(this.getValue(newState) & 0b00001100) + " option for touch pins"});
            Promise.resolve(this.setState(this.state));
            return false;
        }
        return true;
    }

    onChange(name, value) {
        if (this.isValidChange(name, value)) {
            this.state[name].value = value;
            this.props.onChange(this.getValue(this.state));
            return true;
        }
        return false;
    }

    getErrors(value) {
        return value.errors.map((val,idx) => e(Snackbar,{
            key:`error${idx}`, 
            className:"popuperror", 
            anchorOrigin: {vertical: "top", horizontal: "right"}, 
            autoHideDuration: 3000,
            onClose: (event, reason) => {val.visible=false; this.setState(this.state);},
            open: val.visible},e(Alert,{key:`error${idx}`, severity: "error"}, val.error)));
    }

    renderOption(name, value) {    
        return [
            <Chip label={name} className={this.state[name]?.value ? "selected" : "available"} onClick={_ => this.onChange(name, !this.state[name].value)}/>,
            ...this.getErrors(value)
        ];
    }

    render() {
        return <Paper className="driver-flags" elevation={3}>
            {Object.keys(this.state).map(name => this.renderOption(name, this.state[name]))}
            </Paper>
    }
}