class DigitalPinConfig extends React.Component {
    constructor(props) {
        super(props);
        this.state = {pin: props.item};
    }

    componentDidUpdate(prevProps, prevState) {
        if ((this.state != prevState) && this.props.onChange) {
            this.props.onChange(this.state);
        }
        
        if (prevProps?.item != this.props?.item) {
            this.setState({pin: this.props.item});
        }
    }

    onChange(name, value) {
        this.state.pin[name] = name == "pinName" ? value : parseInt(value); 
        this.setState(this.state);
    }

    render() {
        return e( MaterialUI.Card, { key: this.state.pin.pinName, className: "pin-config" },[
            e( MaterialUI.CardHeader, {key:"header", title: this.state.pin.pinName }),
            e( MaterialUI.CardContent, {key:"details"},  e(MaterialUI.List,{key: "items"},
                [
                    e( MaterialUI.ListItem, { key: "pinName" },  
                        e( MaterialUI.TextField, { key: "pinName", autoFocus:true, value: this.state.pin.pinName, label: "Name", type: "text", onChange: event => this.onChange("pinName", event.target.value)})),
                    e( MaterialUI.ListItem, { key: "pinNo" },  
                        e( MaterialUI.TextField, { key:"pinNo", value: this.state.pin.pinNo, label: "PinNo", type: "number", onChange: event => this.onChange("pinNo", event.target.value) })),
                    e( PinDriverFlags, { key: "driverFlags", autoFocus:true, value: this.state.pin.driverFlags, onChange: val => this.onChange("driverFlags", val) }),
                ]
            ))
        ]);
    }
}