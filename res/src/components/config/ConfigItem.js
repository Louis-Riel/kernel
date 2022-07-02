class ConfigItem extends React.Component {
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

    getFieldType(name,value) {
        if (this.state.pin[name] !== undefined) {
            if (isNaN(this.state.pin[name])) {
                return "text";
            }
            return "number";
        }
        return "text";
    }

    getFieldValue(name,value) {
        if (this.state.pin[name] !== undefined) {
            if (isNaN(this.state.pin[name])) {
                return value;
            }
            if (typeof value == "string") {
                if (value.indexOf(".") != -1) {
                    return parseFloat(value);
                }
                return parseInt(value);
            }
        }
        return value;
    }

    onChange(name, value) {
        this.state.pin[name] = this.getFieldValue(name,value); 
        this.setState(this.state);
    }

    getFieldWeight(field) {
        if (field == this.props.nameField) {
            return 100;
        }

        switch(this.getFieldType(field, this.state.pin[field])) {
            case "text":
                return 75;
            case "number":
                return 50;
            default:
                return 25;
        }
    }

    render() {
        return e( MaterialUI.Card, { key: this.state.pin[this.props.nameField], className: "config-item" },[
            e( MaterialUI.CardHeader, {key:"header", title: this.state.pin[this.props.nameField] }),
            e( MaterialUI.CardContent, {key:"details"},  e(MaterialUI.List,{key: "items"},
                Object.keys(this.state.pin)
                      .sort((a,b) => {
                        var wa = this.getFieldWeight(a);
                        var wb = this.getFieldWeight(b);
                        if (wa == wb) {
                            return a.localeCompare(b);
                        }
                        return wb - wa;
                    }).map(this.getEditor.bind(this))
            ))
        ]);
    }

    getEditor(key) {
        if (this.props.editors && this.props.editors[key]) {
            return e(this.props.editors[key].editor, {
                key: key,
                autoFocus: true,
                value: this.getFieldValue(key, this.state.pin[key]),
                label: key,
                type: this.getFieldType(key, this.state.pin[key]),
                onChange: value => this.onChange(key, value)
            })
        }
        return e(MaterialUI.ListItem, { key: key },
            e(MaterialUI.TextField, {
                key: key,
                autoFocus: this.getFieldType(key, this.state.pin[key]) == "text",
                value: this.getFieldValue(key, this.state.pin[key]),
                label: key,
                type: this.getFieldType(key, this.state.pin[key]),
                onChange: event => this.onChange(key, event.target.value)
            })
        );
    }
}