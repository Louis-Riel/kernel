import {createElement as e, Component} from 'react';

export default class IntInput extends Component {
    toggleChange = (elem) => {
        if (this.props.onChange) {
            this.props.onChange(elem.target.value);
        }
    }

    render() {
        return e("label", { key: genUUID(), className: "editable", id: `lbl${this.id}`, key: this.id },
            e("div", { key: genUUID(), className: "label", id: `div${this.id}` }, this.props.label),
            e("input", { key: genUUID(), type: "number", value: this.props.defaultValue, onChange: this.toggleChange.bind(this), id: `${this.id}` }));
    }
}
