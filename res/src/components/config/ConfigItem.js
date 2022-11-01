class ConfigItem extends React.Component {
    constructor(props) {
        super(props);
        if (JSON.stringify(props.item) === "{}") {
            Object.keys(props.value)
                  .filter(fld => !['collectionName','class','isArray'].find(val=>val===fld))
                  .forEach(fld => props.item[fld] = props.value[fld]);
        }
        this.state = { instance: props.item };
    }

    componentDidUpdate(prevProps, prevState) {
        if ((this.state != prevState) && this.props.onChange) {
            this.props.onChange(this.state.instance);
        }
    }

    getFieldType(name) {
        var obj = this.props.item[name];
        if (obj !== undefined) {
            if (typeof(obj) === 'boolean') {
                return "boolean";
            }
            if (isNaN(obj)) {
                return "text";
            }
            return "number";
        }
        return "text";
    }

    parseFieldValue(value) {
        if (value !== undefined) {
            if (isNaN(value)) {
                return value;
            }
            if (typeof value === "string") {
                if (value.indexOf(".") != -1) {
                    return parseFloat(value);
                }
                return parseInt(value);
            }
        }
        return value;
    }

    onChange(name, value) {
        this.props.item[name] = this.parseFieldValue(value); 
        this.setState(this.state);
    }

    getFieldWeight(field) {
        if (field == this.props.nameField) {
            return 100;
        }

        switch(this.getFieldType(field, this.props.item[field])) {
            case "text":
                return 75;
            case "number":
                return 50;
            default:
                return 25;
        }
    }

    render() {
        return e( MaterialUI.Card, { key: this.props.item[this.props.nameField], className: "config-item" },[
            e( MaterialUI.CardHeader, {key:"header", title: this.props.item[this.props.nameField] }),
            e( MaterialUI.CardContent, {key:"details"},  e(MaterialUI.List,{key: "items"},
                Object.keys(JSON.stringify(this.props.item) === "{}" ? this.props.value : this.props.item)
                      .filter(fld => !['collectionName','class','isArray'].find(val=>val===fld))
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
        var tp = this.getFieldType(key);
        return e(MaterialUI.ListItem, { key: key },
            tp === "boolean" ? 
            e(MaterialUI.FormControlLabel,{
                key:"label",
                label: key,
                control:e(MaterialUI.Checkbox, {
                    key: "ctrl",
                    checked: this.parseFieldValue(this.props.item[key]),
                    onChange: event => this.onChange(key, event.target.checked)
                })
            }):
            e(MaterialUI.TextField, {
                key: key,
                autoFocus: tp == "text",
                value: this.parseFieldValue(this.props.item[key]),
                label: key,
                type: tp,
                onChange: event => this.onChange(key, event.target.value)
            })
        );
    }
}