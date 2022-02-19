class StateCommands extends React.Component {
    render() {
        return e("div",{key:'commands',name:"commands", className:"commands"},this.props.commands.map(cmd => e(CmdButton,{
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

class EditableLabel extends React.Component {
    constructor(props) {
        super(props);
        this.state ={
            editing: false
        }
    }

    getLabel() {
        return e("div",{onClick: elem=>this.setState({"editing":true}), className:"label"},this.props.label);
    }

    updateLabel(elem){
        this.newLabel= elem.target.value;
    }

    cancelUpdate(elem) {
        this.newLabel=undefined;
        this.setState({editing:false});
    }

    getEditable() {
        return [
            e("input",{key: "edit", defaultValue: this.props.label, onChange:this.updateLabel.bind(this)}),
            e("div",{key: "ok", onClick: elem=>this.setState({"editing":false}), className:"ok-button", dangerouslySetInnerHTML: {__html: "&check;"}}),
            e("div",{key: "cancel", onClick: this.cancelUpdate.bind(this), className:"cancel-button", dangerouslySetInnerHTML: {__html: "&Chi;"}})
        ]
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

    changeLabel(newLabel, oldLabel) {
        if (newLabel !== oldLabel) {
            Object.defineProperty(this.state.value, newLabel,
                Object.getOwnPropertyDescriptor(this.state.value, oldLabel));
            delete this.state.value[oldLabel];
            this.setState(this.state);
        }
    }

    fieldControlPannel(fld){
        if (!this.props.editable){
            return e("div", { key: `fcpnolabel${fld}`, className: "label", id: `dlbl${this.id}` }, fld);
        }

        return e("div",{key:`fcpnolabel${fld}header`,className:"fieldHeader"},[
            e(EditableLabel,{key:`flabel${fld}`,label: fld, onChange: this.changeLabel.bind(this)}),
            e("div",{key:`fcpnolabel${fld}popupmenu`,className:"popupMenu"},[
                e("div",{key:`removefield`, onClick:elem=> this.removeField(fld)},"Remove")
            ])
        ]);
    }

    Parse(json) {
        if ((json !== undefined) && (json != null)) {
            if ((typeof json == "object") && (json.version === undefined)){
                return e("div",{key: `jsonobject`,className:"statusclass jsonNodes"},this.getSortedProperties(json).map(fld => {
                    if (Array.isArray(json[fld])) {
                        if (fld == "commands"){
                            return {fld:fld,element:e(StateCommands,{key:`jofieldcmds`,name:json["name"],commands:json[fld],onSuccess:this.props.updateAppStatus})};
                        }
                        return {fld:fld,element:e(Table, { key: `Table-Object-${this.props.path}/${fld}`, 
                                                           name: fld, 
                                                           path: `${this.props.path}/${fld}`,
                                                           label: fld, 
                                                           json: json[fld], 
                                                           registerEventInstanceCallback: this.props.registerEventInstanceCallback, 
                                                           editable: this.props.editable, 
                                                           sortable: this.props.sortable })};
                    } else if (json[fld] && (typeof json[fld] == 'object') && (json[fld].version === undefined)) {
                        return {fld:fld,element:e(JSONEditor, { 
                            key: `JE-${this.props.path}/${fld}`,
                            path: `${this.props.path}/${fld}`,
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
                                    e("label",{key:`${fld}label`},[
                                        this.fieldControlPannel(fld),
                                        e("input",{key:`input`,defaultValue:json[fld].value === undefined ?  json[fld] : json[fld].value ,onChange: this.processUpdate.bind(this)})]):
                                    e(ROProp, { 
                                        key: `rofld${fld}`, 
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
                },[]).map((item,idx) =>e("div",{key: `fg-${item.fclass}`,className: `fieldgroup ${item.fclass}`},item.elements)))
            } else {
                return (json.version !== undefined) || (this.props.editable && (typeof json != "object")) ?
                            e("input",{key:`input`,defaultValue:json.value === undefined?json:json.value,onChange: this.processUpdate.bind(this)}):
                            e(ROProp, { 
                                key: 'rofield', 
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
            if (this.state.value[fld] === undefined) {
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
        if (!IsDatetimeValue(f1) && (!json || (json[f1] === undefined))) {
            return 2;
        }
        return Array.isArray(json[f1]) ? 6 : typeof json[f1] == 'object' ? json[f1] && json[f1]["commands"] ? 4 : 5 : IsDatetimeValue(f1) ? 1 : 3;
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
            e("div",{key:'ocplabel',className:"jsonlabel"},this.props.label),
            this.props.editable?e("div",{key:genUUID(),className:"popupMenu"},
                e("div",{key:`removeprop`, onClick:elem=> this.addNewProperty()},"Add Column")):null
        ];

    }

    render() {
        if (this.state === null || this.state === undefined) {
            return e("div", { id: `loading${this.id}` }, "Loading...");
        } else if (this.props.label != null) {
            return e("fieldset", { id: `fs${this.props.label}`,className:"jsonNodes" }, [
                e("legend", { key: 'legend' }, this.objectControlPannel()),
                this.Parse(this.state.value)
            ]);
        } else {
            return this.Parse(this.props.json);
        }
    }
}