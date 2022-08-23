import {createElement as e, Component} from 'react';
import AnalogPinConfig from './AnalogPinConfig';
import DigitalPinConfig from './DigitalPinConfig';
import ConfigItem from './ConfigItem';
import { Tabs, Tab, Button } from '@mui/material';
import { wfetch } from '../../../../utils/utils';

var httpPrefix = "";

export default class ConfigGroup extends Component {
    constructor(props) {
        super(props);
        this.supportedTypes = {
            analogPins:{
                caption:"Analog Pins",
                class: "AnalogPin",
                component: AnalogPinConfig
            },pins:{
                caption:"Digital Pins",
                class: "Pin",
                component: DigitalPinConfig
            }
        };
        this.state = {
            currentTab: undefined
        };
        wfetch(`${httpPrefix}/templates/config`,{
            method: 'post'
        }).then(data => data.json())
          .then(this.updateConfigTemplates.bind(this))
          .catch(console.error);
    }

    updateConfigTemplates(configTemplates) {
        this.setState({ 
            configTemplates: configTemplates,
            currentTab: configTemplates[0].collectionName
        });
    }

    render() {
        if (this.props.config && this.state.configTemplates) {
            return this.renderArrayTypes();
        } else {
            return e("div", {key: "loading"}, "Loading...");
        }
    }

    renderArrayTypes() {
        var tabs = this.state.configTemplates
            .filter(configTemplate => configTemplate.isArray);
        return [
            e(Tabs, {
                value: this.state.currentTab,
                onChange: (_, v) => { this.setState({ currentTab: v }); },
                key: "ConfigTypes"
            }, [...tabs.map(this.renderTypeTab.bind(this)),
            e(Tab, { key: "full-config", label: "Configuration", value: "Configuration" })]),
            tabs.map(this.renderConfigType.bind(this)),
            e("div",{key:`config-control-panel`,className:`edior-pannel ${this.state.currentTab === "Configuration" ? "":"hidden"}`},
                    this.props.fullEditor())
            
        ];
    }

    renderTypeTab(key) {
        return key.isArray ? 
                e( Tab, { key: key.collectionName, label: this.supportedTypes[key.collectionName]?.caption || key.collectionName, value: key.collectionName }):
                e( Tab, { key: key.class, label: this.supportedTypes[key.class]?.caption || key.class, value: key.class });
    }

    renderConfigType(key) {
        return e("div",{key:`${key.class}-control-panel`,className:`edior-pannel ${this.state.currentTab === key.collectionName ? "":"hidden"}`},[
            e(Button, { key: "add", onClick: evt=> {this.props.config[key.collectionName] ? this.props.config[key.collectionName].push({}) : this.props.config[key.collectionName] = [{}]; this.props.onChange()} }, e("i",{key:"add", className:"fa fa-plus-square"})),
            e("div",{key:"items", className:`config-cards`}, this.props.config[key.collectionName] ? Object.keys(this.props.config[key.collectionName]).map(idx =>
                this.renderEditor(key,this.props.config[key.collectionName],idx)) : null)
        ]);
    }

    renderConfigItemTab(key, item, idx) {
        return e( Tab, { key: item, label: idx+1, value: key });
    }

    renderEditor(key, item, idx) {
        return  e("div",{key:`${key.collectionName}-${idx}-control-editor`,className:`control-editor`},[
                    e( this.supportedTypes[key.collectionName]?.component || ConfigItem, { key: key.collectionName + idx, value: key, role: "tabpanel", item: item[idx], onChange: this.props.onChange}),
                    e(Button, { key: "dup", onClick: evt=> {this.props.config[key.collectionName].push(JSON.parse(JSON.stringify(this.props.config[key.collectionName][idx]))); this.props.onChange()} }, e("i",{key:"copy", className:"fa fa-clone"})),
                    e(Button, { key: "delete", onClick: evt=> {this.props.config[key.collectionName].splice(idx,1); this.props.onChange()} }, e("i",{key:"copy", className:"fa fa-trash"})),
                ]);
    }

    isSupported(key) {
        return Object.keys(this.supportedTypes).indexOf(key)>-1;
    }
}