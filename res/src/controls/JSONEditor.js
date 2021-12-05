class StateCommands extends React.Component {
    render() {
        return e("div",{key:genUUID(),name:"commands", className:"commands"},this.props.commands.map(cmd => e(CmdButton,{
            key:genUUID(),
            caption:cmd.caption || cmd.command,
            command:cmd.command,
            name:this.props.name,
            onSuccess:this.props.onSuccess,
            onError:this.props.onError,
            param1:cmd.param1,
            param2:cmd.param2,
            HTTP_METHOD:cmd.HTTP_METHOD
        })));
    }
}

class JSONEditor extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            value:props.json,
            class: props.json?.class,
            name: props.json?.name
        };
        if (this.props.registerEventInstanceCallback && this.state.class && this.state.name) {
            this.props.registerEventInstanceCallback(this.ProcessEvent.bind(this),`${this.state.class}-${this.state.name}`);
        }
    }

    componentWillUnmount() {
        this.mounted=false;
    }

    componentDidMount() {
        this.mounted=true;
    }

    ProcessEvent(evt) {
        if (this.mounted && evt?.data){
            if ((evt.data?.class == this.state.class) && (evt.data?.name == this.state.name)){
                this.setState({value: evt.data});
            }
        }
    }

    removeField(fld){
        delete this.state.value[fld];
        this.setState({value:this.state.value});
    }

    BuildTablePannel(){
        if (this.props.editable) {
            return e("div",{key:genUUID(),className:"popupMenu"},[
                e("div",{key:genUUID()},"*"),
                e("div",{key:genUUID(),className:"menuItems"},[
                    e("div",{key:genUUID(),className:"menuItem", onClick:elem=> this.addRow(elem)},"Add Empty Row")
                ])
            ])
        }
        return null;
    }


    fieldControlPannel(fld){
        if (!this.props.editable){
            return e("div", { key: genUUID(), className: "label", id: `dlbl${this.id}` }, fld);
        }

        return e("div",{key:genUUID(),className:"fieldHeader"},[
            e("div",{key:genUUID(),className:"label"},fld),
            e("div",{key:genUUID(),className:"popupMenu"},[
                e("div",{key:genUUID()},"*"),
                e("div",{key:genUUID(),className:"menuItems"},[
                    e("div",{key:genUUID(),className:"menuItem", onClick:elem=> this.removeField(fld)},"Remove Field")
                ])
            ])
        ]);
    }

    Parse(json) {
        if ((json !== undefined) && (json != null)) {
            if ((typeof json == "object") && (json.version === undefined)){
                return e("div",{key: genUUID(),className:"statusclass jsonNodes"},this.getSortedProperties(json).map(fld => {
                    if (Array.isArray(json[fld])) {
                        if (fld == "commands"){
                            return {fld:fld,element:e(StateCommands,{key:genUUID(),name:json["name"],commands:json[fld],onSuccess:this.props.updateAppStatus})};
                        }
                        return {fld:fld,element:e(Table, { key: genUUID(), name: fld, label: fld, json: json[fld], registerEventInstanceCallback: this.props.registerEventInstanceCallback, editable:this.props.editable, sortable: this.props.sortable })};
                    } else if (json[fld] && (typeof json[fld] == 'object') && (json[fld].version === undefined)) {
                        return {fld:fld,element:e(JSONEditor, { 
                            key: genUUID(), 
                            name: fld, 
                            label: fld, 
                            editable: this.props.editable,
                            json: json[fld],
                            updateAppStatus:this.props.updateAppStatus,
                            registerEventInstanceCallback: this.props.registerEventInstanceCallback })};
                    } else if ((fld != "class") && !((fld == "name") && (json["name"] == json["class"])) ) {
                        return {
                            fld:fld,
                            element: this.props.editable ?
                                    e("label",{key:genUUID()},[
                                        this.fieldControlPannel(fld),
                                        e("input",{key:genUUID(),defaultValue:json[fld]?.value || json[fld], onChange: elem => this.processUpdate(elem,fld)})]):
                                    e(ROProp, { 
                                        key: genUUID(), 
                                        value: json[fld], 
                                        name: fld, 
                                        label: fld 
                                    })
                        };
                    }
                }).reduce((pv,cv)=>{
                    if (cv){
                        var fc = this.getFieldClass(json,cv.fld);
                        var item = pv.find(it=>it.fclass == fc);
                        if (item) {
                            item.elements.push(cv.element);
                        } else {
                            pv.push({fclass:fc,elements:[cv.element]});
                        }
                    }
                    return pv;
                },[]).map(item =>e("div",{key: genUUID(),className: `fieldgroup ${item.fclass}`},item.elements)))
            } else {
                return (json.version !== undefined) || (this.props.editable && (typeof json != "object")) ?
                            e("input",{key:genUUID(),defaultValue:json.value !== undefined ? json.value : json,onChange: this.processUpdate.bind(this)}):
                            e(ROProp, { 
                                key: genUUID(), 
                                value: json, 
                                name: "" 
                            });
            }
        } else {
            return null;
        }
    }

    processUpdate(elem,fld) {
        if (fld) {
            if (!this.state.value[fld]) {
                this.state.value[fld]={version:-1};
            }
            this.state.value[fld].value=elem.target.value;
            this.state.value[fld].version++;
        } else {
            this.state.value.value=elem.target.value;
            this.state.value.version++;
        }
    }

    getSortedProperties(json) {
        return Object.keys(json)
            .sort((f1, f2) => { 
                var f1s = this.getFieldWeight(json, f1);
                var f2s = this.getFieldWeight(json, f2);
                if (f1s == f2s) {
                    return f1.localeCompare(f2);
                }
                return f1s > f2s ? 1 : f2s > f1s ? -1 : 0;
            }).filter(fld => !json[fld] || !(typeof(json[fld]) == "object" && Object.keys(json[fld]).filter(fld=>fld != "class" && fld != "name").length==0));
    }

    getFieldWeight(json, f1) {
        if (!IsDatetimeValue(f1) && (!json || !json[f1])) {
            return 2;
        }
        return Array.isArray(json[f1]) ? 6 : typeof json[f1] == 'object' ? json[f1]["commands"] ? 4 : 5 : IsDatetimeValue(f1) ? 1 : 3;
    }

    getFieldClass(json, f1) {
        if (!json || !json[f1]) {
            return "field";
        }
        return Array.isArray(json[f1]) ? "array" : typeof json[f1] == 'object' && json[f1].version === undefined ? json[f1]["commands"] ? "commandable" : "object" : "field";
    }

    addNewProperty(){
        this.state.value[`prop${Object.keys(this.state.value).filter(prop => prop.match(/^prop.*/)).length+1}`]=null;
        this.setState({value:this.state.value});
    }

    objectControlPannel(){
        return [
            e("div",{key:genUUID(),className:"jsonlabel"},this.props.label),
            this.props.editable?e("div",{key:genUUID(),className:"popupMenu"},[
                e("div",{key:genUUID(),className:"menuItems"},[
                    e("div",{key:genUUID(),className:"menuItem", onClick:elem=> this.addNewProperty()},"Add Property"),
                    this.props.onRemove?e("div",{key:genUUID(),className:"menuItem", onClick:elem=> this.props.onRemove},"Remove"):null
                ]),
                e("div",{key:genUUID()},"*")
            ]):null
        ];

    }

    render() {
        if (this.state === null || this.state === undefined) {
            return e("div", { id: `loading${this.id}` }, "Loading...");
        } else if (this.props.label != null) {
            return e("fieldset", { id: `fs${this.props.label}`,className:"jsonNodes" }, [
                e("legend", { key: genUUID() }, this.objectControlPannel()),
                this.Parse(this.state.value)
            ]);
        } else {
            return this.Parse(this.props.json);
        }
    }
}