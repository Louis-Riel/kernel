import {createElement as e, Component, lazy, Suspense} from 'react';
import { Tabs, Tab, Button } from '@mui/material';
import { chipRequest } from '../../../../utils/utils';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faSpinner } from '@fortawesome/free-solid-svg-icons/faSpinner'
import { faPlusSquare, faClone, faTrashCan } from '@fortawesome/free-solid-svg-icons'


const AnalogPinConfig = lazy(() => import('./AnalogPinConfig'));
const DigitalPinConfig = lazy(() => import('./DigitalPinConfig'));
const ConfigItem = lazy(() => import('./ConfigItem'));

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
        chipRequest(`/templates/config`,{
            method: 'post'
        }).then(data => data.json())
          .then(this.updateConfigTemplates.bind(this))
          .catch(console.error);
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (prevProps?.selectedDevice !== this.props.selectedDevice) {
            chipRequest(`/templates/config`,{
                method: 'post'
            }).then(data => data.json())
              .then(this.updateConfigTemplates.bind(this))
              .catch(console.error);
        }
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
            return <FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />;
        }
    }

    renderArrayTypes() {
        let tabs = this.state.configTemplates
            .filter(configTemplate => configTemplate.isArray);
        return [
            e(Tabs, {
                value: this.state.currentTab,
                onChange: (_, v) => { this.setState({ currentTab: v }); },
                key: "ConfigTypes"
            }, tabs.map(this.renderTypeTab.bind(this))),
            tabs.map(this.renderConfigType.bind(this))
        ];
    }

    renderTypeTab(key) {
        return key.isArray ? 
                e( Tab, { key: key.collectionName, label: this.supportedTypes[key.collectionName]?.caption || key.collectionName, value: key.collectionName }):
                e( Tab, { key: key.class, label: this.supportedTypes[key.class]?.caption || key.class, value: key.class });
    }

    renderConfigType(key) {
        return e("div",{key:`${key.class}-control-panel`,className:`edior-pannel ${this.state.currentTab === key.collectionName ? "":"hidden"}`},[
            e(Button, { key: "add", onClick: evt=> {this.props.config[key.collectionName] ? this.props.config[key.collectionName].push({}) : this.props.config[key.collectionName] = [{}]; this.props.onChange(this.props.config)} }, <FontAwesomeIcon icon={faPlusSquare} />),
            e("div",{key:"items", className:`config-cards`}, this.props.config[key.collectionName] ? Object.keys(this.props.config[key.collectionName]).map(idx =>
                this.renderEditor(key,this.props.config[key.collectionName],idx)) : null)
        ]);
    }

    renderEditor(key, items, idx) {
        return  e("div",{key:`${key.collectionName}-${idx}-control-editor`,className:`control-editor`},[
                    <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
                        {e( this.supportedTypes[key.collectionName]?.component || ConfigItem, { 
                            key: key.collectionName + idx, 
                            value: key, 
                            role: "tabpanel", 
                            item: items[idx], 
                            nameField: "name", 
                            onChange: (updatedItem)=>this.props.onChange({
                                ...this.props.config,
                                [key.collectionName]:items.map((item,idx2)=>parseInt(idx) === idx2 ? updatedItem : item)})
                            }
                        )}
                    </Suspense>,
                    e(Button, { key: "dup", onClick: evt=> 
                        this.props.onChange({
                            ...this.props.config,
                            [key.collectionName]:[...items,JSON.parse(JSON.stringify(this.props.config[key.collectionName][idx]))]})
                    }, <FontAwesomeIcon icon={faClone} />),
                    e(Button, { key: "delete", onClick: evt=>
                        this.props.onChange({
                            ...this.props.config,
                            [key.collectionName]:[...items.filter((it,idx2)=>parseInt(idx) !== idx2)]})
                    }, <FontAwesomeIcon icon={faTrashCan} />),
                ]);
    }
}