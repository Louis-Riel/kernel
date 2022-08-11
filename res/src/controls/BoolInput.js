class BoolInput extends React.Component {
    constructor(props) {
        super(props);
        this.id = this.props.id || genUUID();
        if (this.props.onOn && this.props.initialState){
            this.props.onOn()
        }
        if (this.props.onOff && !this.props.initialState){
            this.props.onOff()
        }
    }

    toggleChange = (elem) => {
        elem.target.checked ?
            this.props.onOn ? this.props.onOn(elem.target) : null :
            this.props.onOff ? this.props.onOff(elem.target) : null;
        if (this.props.onChange) {
            this.props.onChange(elem.target.checked);
        }
    }

    render() {
        return e("label", { key: genUUID(), className: `editable ${this.props.blurred ? "loading":""}`, id: `lbl${this.id}`, key: this.id },
            e("div", { key: genUUID(), className: "label", id: `dlbl${this.id}` }, this.props.label),
            e("input", { key: genUUID(), type: "checkbox", onChange: this.toggleChange, id: `in${this.id}`, checked: this.props.initialState}));
    }
}
