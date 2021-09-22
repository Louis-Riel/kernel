class BoolInput extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            checked: this.props.initialState ? this.props.initialState() : false
        };
        this.id = this.props.id || genUUID();
    }

    toggleChange = (elem) => {
        this.setState({
            checked: elem.target.checked
        });
        elem.target.checked ?
            this.props.onOn ? this.props.onOn(elem.target) : null :
            this.props.onOff ? this.props.onOff(elem.target) : null;
    }

    render() {
        return e("label", { key: genUUID(), className: "editable", id: `lbl${this.id}`, key: this.id },
            e("div", { key: genUUID(), className: "label", id: `dlbl${this.id}` }, this.props.label),
            e("input", { key: genUUID(), type: "checkbox", onChange: this.toggleChange, id: `in${this.id}`, checked: this.state.checked }));
    }
}
