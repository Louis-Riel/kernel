import { createElement as e, Component } from 'react';

export class EditableLabel extends Component {
    constructor(props) {
        super(props);
        this.state = {
            editing: false
        };
    }

    getLabel() {
        return e("div", { onClick: elem => this.setState({ editing: true }), className: "label" }, this.props.label);
    }

    updateLabel(elem) {
        this.newLabel = elem.target.value;
    }

    cancelUpdate(elem) {
        this.newLabel = undefined;
        this.setState({ editing: false });
    }

    getEditable() {
        return [
            e("input", { key: "edit", defaultValue: this.props.label, onChange: this.updateLabel.bind(this) }),
            e("div", { key: "ok", onClick: elem => this.setState({ editing: false }), className: "ok-button", dangerouslySetInnerHTML: { __html: "&check;" } }),
            e("div", { key: "cancel", onClick: this.cancelUpdate.bind(this), className: "cancel-button", dangerouslySetInnerHTML: { __html: "&Chi;" } })
        ];
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (this.props.onChange && this.newLabel && (prevState.editing !== this.state.editing)) {
            this.props.onChange(this.newLabel, this.props.label);
        }
    }

    render() {
        return this.state.editing ? this.getEditable() : this.getLabel();
    }
}
