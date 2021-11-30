class JSONNode extends React.Component {
    getNodeType() {
        if (this.isArray()) {
            return "array";
        }

        return typeof this.getValue();
    }

    isArray() {
        return Array.isArray(this.props.json);
    }

    getValue() {
        if (this.state?.updatedValue) {
            return this.state.updatedValue;
        }
        if (this.isEditable()) {
            return this.props.json.value !== undefined ? 
                this.props.json.value:
                this.props.json;
        }
        return this.props.json;
    }

    isEditable() {
        return this.props.editable;
    }

    updateValue(elem){
        if (this.isEditable()) {
            if (this.getValue() != elem.target.value){
                this.state = {
                    updated: {
                        value:elem.target.value,
                        version: this.props.json.version+1
                    }
                }
                elem.target.classList.add("updated");
            } else {
                this.state={};
                elem.target.classList.remove("updated");
            }
        }
    }

    renderField(){
        if (this.isEditable()){
            return this.props.name !== undefined ?
                     e("details",{key:genUUID(),open:true},[
                        e("summary",{key:genUUID()},this.props.name),
                        e("div",{key:genUUID()},
                            e("input",{key:genUUID(),defaultValue:this.getValue(),onChange:this.updateValue.bind(this)})
                        )
                    ]):
                    e("input",{key:genUUID(),defaultValue:this.getValue(),onChange:this.updateValue.bind(this)})
                } else {
            return this.props.name !== undefined ?
                e("details",{key:genUUID(),open:true},[
                    e("summary",{key:genUUID()},this.props.name),
                    e("div",{key:genUUID()},this.getValue())
                ]):
                e("div",{key:genUUID()},this.getValue())
        }
    }

    getTypeNumber(val) {
        if (Array.isArray(val)) {
            return 30;
        }
        if ((typeof val == "object") && (val.version !== undefined)) {
            return 20;
        }
        return 10;
    }

    fieldComparer(f1,f2) {
        if (this.getTypeNumber(f1) == this.getTypeNumber(f2)) {
            return f1.localeCompare(f2);
        }
        this.getTypeNumber(f1) > this.getTypeNumber(f2) ? 1 : -1;
    }

    render() {
        switch (this.getNodeType()) {
            case "object":
                if (Object.keys(this.props.json).length > 0){
                    return e("fieldset",{key:genUUID()},[
                        this.props.name !== undefined ? e("legend",{key:genUUID()},this.props.name):null,
                        Object.keys(this.getValue())
                            .sort(this.fieldComparer.bind(this))
                            .map(prop=>e(JSONNode,{key:genUUID(),editable:this.props.editable, name:prop,json:this.getValue()[prop]}))
                    ])
                } else {
                    return null;
                }
            case "array":
                return this.getValue().length ? e(Table, { key: genUUID(), sortable: !this.props.editable, name: this.props.name, label: this.props.name, json: this.getValue() }):null;
            default:
                return this.renderField();
        }
    }
}

class JSONEditor extends React.Component {
    render() {
        if (this.props.json && this.props.name) {
            return e("div",{key:genUUID(),className: "jsoneditor"},[
                e("div",{key:genUUID(),className: "controlPanel"},
                    e("div",{key:genUUID(),className: "pendingUpdates"})
                ),
                e("div",{key:genUUID(),className:"jsonNodes"},e(JSONNode,{key:genUUID(),editable:this.props.editable, json:this.props.json,name:"Config"}))
            ]);
        }
    }
}
