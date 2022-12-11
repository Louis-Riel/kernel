import {Component} from 'react';
import {Snackbar,Alert,Paper,Chip} from '@mui/material';

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
        if (this.props.value !== prevProps.value) {
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

        if (prevState !== this.state) {
            this.props.onChange(this.getValue(this.state));
        }
    }

    isValidChange(name, value) {
        let newState =  JSON.parse(JSON.stringify(this.state));
        newState[name].value = value;
        if ((name === "digital_in") && newState.digital_out.value) {
            this.setState({digital_out:{value:false,errors:[]},
                            digital_in:{value:true,errors:[]}});
        } else if ((name === "digital_out") && newState.digital_in.value) {
            this.setState({
                digital_out:{value: true,errors:[]},
                digital_in:{value: false,errors:[]},
                pullup:{value: false,errors:[]},
                pulldown:{value: false,errors:[]},
                touch:{value: false,errors:[]},
                wakeonhigh:{value: false,errors:[]},
                wakeonlow:{value: false,errors:[]}
            });
        } else {
            if (newState.digital_out.value && this.getValue(newState) & 0b01111100) {
                newState[name].value = false;
                newState[name].errors.push({visible: true, error: "Can't set " + this.getFlagNames(this.props.value) + " option for output pins"});
                this.setState(newState);
                return false;
            }
            
            if (newState.touch.value && (this.getValue(newState) & 0b00001100)) {
                newState[name].value = false;
                newState[name].errors.push({visible: true, error: "Can't set " + this.getFlagNames(this.props.value) + " option for touch pins"});
                this.setState(newState);
                return false;
            } 
            this.setState({[name]:{value:value,errors:[]}});
        }
        return true;
    }

    onChange(name, value) {
        return this.isValidChange(name, value);
    }

    getErrors(value) {
        return value.errors.map((val) => <Snackbar 
            className={"popuperror"}
            anchorOrigin={{vertical: "top", horizontal: "right"}}
            autoHideDuration={3000}
            onClose={(event, reason) => {val.visible=false; this.setState(this.state);}}
            open={val.visible}>
            <Alert severity={"error"}>{val.error}</Alert>
        </Snackbar>);
    }

    renderOption(name, value) {    
        return [
            <Chip 
                label={name} 
                className={this.state[name]?.value ? "selected" : "available"} 
                onClick={_ => this.onChange(name, !this.state[name].value)}/>,
            ...this.getErrors(value)
        ];
    }

    render() {
        return <Paper className="driver-flags" elevation={3} variant={'outlined'}>
            {Object.entries(this.state).map(state => this.renderOption(state[0], state[1]))}
            </Paper>
    }
}