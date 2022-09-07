import {createElement as e, Component} from 'react';
import ROProp from './ROProp';
import Table from './Table';
import {IsDatetimeValue, genUUID} from '../../../utils/utils'
import './JSONEditor.css';
import { EditableLabel } from './EditableLabel';
import { StateCommands } from './StateCommands';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faSpinner } from '@fortawesome/free-solid-svg-icons/faSpinner';

export default class LocalJSONEditor extends Component {
    constructor(props) {
        super(props);
        this.state = {
            json: props.json
        };
        if (this.props.registerEventInstanceCallback && this.props.name) {
            this.props.registerEventInstanceCallback(this.ProcessEvent.bind(this),`${this.props.class}-${this.props.name}`);
        }
    }

    componentWillUnmount() {
        this.mounted=false;
    }

    componentDidMount() {
        this.mounted=true;
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (prevProps && prevProps.json && (prevProps.json !== this.props.json)){
            this.setState({json: this.props.json});
        }
    }

    ProcessEvent(evt) {
        if (this.mounted && evt?.data){
            if (evt.data?.name === this.props.name){
                this.setState({json: evt.data});
            }
        }
    }

    removeField(fld){
        delete this.state.json[fld];
        this.setState({value:this.state.json});
    }

    changeLabel(newLabel, oldLabel) {
        if (newLabel !== oldLabel) {
            Object.defineProperty(this.state.json, newLabel,
                Object.getOwnPropertyDescriptor(this.state.json, oldLabel));
            delete this.state.json[oldLabel];
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
            if ((typeof json === "object") && (json.version === undefined)){
                return e("div",{key: `jsonobject`,className:"statusclass jsonNodes"},this.getSortedProperties(json)
                                                                                         .map(fld => this.renderField(json, fld))
                                                                                         .reduce((pv,cv)=>{
                    if (cv){
                        var fc = this.getFieldClass(json,cv.fld);
                        var item = pv.find(it=>it.fclass === fc);
                        if (item) {
                            item.elements.push(cv.element);
                        } else {
                            pv.push({fclass:fc,elements:[cv.element]});
                        }
                    }
                    return pv;
                },[]).map((item) =>e("div",{key: `fg-${item.fclass}`,className: `fieldgroup ${item.fclass}`},item.elements)))
            } else {
                return this.renderVersioned(json);
            }
        } else {
            return null;
        }
    }

    renderField(json, fld) {
        if (Array.isArray(json[fld])) {
            if (fld === "commands"){
                return this.renderCommands(fld, json);
            }
            return this.renderArray(fld, json);
        } else if (json[fld] && (typeof json[fld] === 'object') && (json[fld].version === undefined)) {
            return this.renderObject(fld, json);
        } else if ((fld !== "class") && !((fld === "name") && (json["name"] === json["class"])) ) {
            return this.renderFieldValue(fld, json);
        }
    }

    renderFieldValue(fld, json) {
        return {
            fld: fld,
            element: this.props.editable ?
                e("label", { key: `${fld}label` }, [
                    this.fieldControlPannel(fld),
                    e("input", { key: `input`, defaultValue: json[fld].value === undefined ? json[fld] : json[fld].value, onChange: this.processUpdate.bind(this) })
                ]) :
                e(ROProp, {
                    key: `rofld${fld}`,
                    value: json[fld],
                    name: fld,
                    label: fld
                })
        };
    }

    renderObject(fld, json) {
        return {
            fld: fld, element: e(LocalJSONEditor, {
                key: `JE-${this.props.path}/${fld}`,
                path: `${this.props.path}/${fld}`,
                name: fld,
                label: fld,
                editable: this.props.editable,
                sortable: this.props.sortable,
                json: json[fld],
                updateAppStatus: this.props.updateAppStatus,
                registerEventInstanceCallback: this.props.registerEventInstanceCallback
            })
        };
    }

    renderArray(fld, json) {
        return {
            fld: fld, element: e(Table, {
                key: `Table-Object-${this.props.path}/${fld}`,
                name: fld,
                path: `${this.props.path}/${fld}`,
                label: fld,
                json: json[fld],
                registerEventInstanceCallback: this.props.registerEventInstanceCallback,
                editable: this.props.editable,
                sortable: this.props.sortable
            })
        };
    }

    renderCommands(fld, json) {
        return { fld: fld, element: e(StateCommands, { key: `${fld}cmds`, name: json["name"], commands: json[fld], onSuccess: this.props.updateAppStatus }) };
    }

    renderVersioned(json) {
        return (json.version !== undefined) || (this.props.editable && (typeof json != "object")) ?
            e("input", { key: `input`, defaultValue: json.value === undefined ? json : json.value, onChange: this.processUpdate.bind(this) }) :
            e(ROProp, {
                key: 'rofield',
                value: json,
                name: ""
            });
    }

    processUpdate(elem,fld) {
        if (fld) {
            if (this.state.json[fld] === undefined) {
                this.state.json[fld]={version:-1};
            }
            this.state.json[fld].value=elem.target.value;
            this.state.json[fld].version++;
        } else {
            this.state.json.value=elem.target.value;
            this.state.json.version++;
        }
    }

    getSortedProperties(json) {
        return Object.keys(json)
            .sort((f1, f2) => { 
                var f1s = this.getFieldWeight(json, f1);
                var f2s = this.getFieldWeight(json, f2);
                if (f1s === f2s) {
                    return 1;
                }
                return f1s > f2s ? 1 : f2s > f1s ? -1 : 0;
            })
            .filter(fld => !json[fld] || !(typeof(json[fld]) === "object" && Object.keys(json[fld]).filter(fld=>fld !== "class" && fld !== "name").length==0));
    }

    getFieldWeight(json, f1) {
        if (!IsDatetimeValue(f1) && (!json || (json[f1] === undefined))) {
            return 2;
        }
        return Array.isArray(json[f1]) ? 6 : typeof json[f1] === 'object' ? json[f1] && json[f1]["commands"] ? 4 : 5 : IsDatetimeValue(f1) ? 1 : 3;
    }

    getFieldClass(json, f1) {
        if (!json || !json[f1]) {
            return "field";
        }
        return Array.isArray(json[f1]) ? "array" : typeof json[f1] === 'object' && json[f1].version === undefined ? json[f1]["commands"] ? "commandable" : "object" : "field";
    }

    addNewProperty(){
        this.state.json[`prop${Object.keys(this.state.json).filter(prop => prop.match(/^prop.*/)).length+1}`]=null;
        this.setState({value:this.state.json});
    }

    objectControlPannel(){
        return [
            e("div",{key:'ocplabel',className:"jsonlabel"},this.props.label),
            this.props.editable?e("div",{key:genUUID(),className:"popupMenu"},
                e("div",{key:`removeprop`, onClick:elem=> this.addNewProperty()},"Add Column")):null
        ];

    }

    render() {
        if (this.state.json === null || this.state.json === undefined) {
            return <FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />;
        } else if (this.props.label != null) {
            return e("fieldset", { id: `fs${this.props.label}`,className:"jsonNodes" }, [
                e("legend", { key: 'legend' }, this.objectControlPannel()),
                this.Parse(this.state.json)
            ]);
        } else {
            return this.Parse(this.state.json);
        }
    }
}

