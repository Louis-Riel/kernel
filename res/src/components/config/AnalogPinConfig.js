class AnalogPinConfig extends React.Component {
    constructor(props) {
        super(props);
        this.state = {};
    }
    render() {
        return e( MaterialUI.Card, { key: this.props.item.name }, 
            e( MaterialUI.CardContent, {key:"details"},  e(MaterialUI.List,{key: "items"},[
                e( MaterialUI.ListItem, { key: "pin" }, e( MaterialUI.ListItem, { key: "pincfg" }, e( MaterialUI.TextField, { value: this.props.pinNo, label: "Pin", type: "number" }))),
            ])));
    }
}