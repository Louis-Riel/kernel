import {createElement as e, Component} from 'react';
import { Button,Collapse,ListItemButton,ListItemIcon,ListItemText } from '@mui/material';
import {wfetch, fromVersionedToPlain,fromPlainToVersionned, genUUID} from '../../../utils/utils'
import LocalJSONEditor from '../../controls/JSONEditor/JSONEditor'
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faSpinner } from '@fortawesome/free-solid-svg-icons/faSpinner'
import { faGear } from '@fortawesome/free-solid-svg-icons'
import { faGears } from '@fortawesome/free-solid-svg-icons'
import { faChevronDown } from '@fortawesome/free-solid-svg-icons/faChevronDown'
import { faChevronUp } from '@fortawesome/free-solid-svg-icons/faChevronUp'
import ConfigGroup from './config/ConfigGroup';
//import JSONEditor from 'jsoneditor'
import './Config.css'
import './JSONEditor.css'
import 'jsoneditor/dist/jsoneditor.css';

var httpPrefix = "";

export default class ConfigPage extends Component {
    constructor(props) {
        super(props);
        this.state={}
        this.fetchConfig();
    }

    fetchConfig() {
        //var abort = new AbortController()
        //var timer = setTimeout(() => abort.abort(), 8000);
        wfetch(`${httpPrefix}/config/${this.props.selectedDeviceId==="current"?"":this.props.selectedDeviceId+".json"}`, {
                method: 'post',
                //signal: abort.signal
            }).then(resp => resp.json())
            .then(config => {
                //clearTimeout(timer);
                this.setState({
                    config: fromVersionedToPlain(config),
                    newconfig: fromVersionedToPlain(config),
                    original: config
                });
            });
    }

    buildEditor() {
        try {
            if (!this.jsoneditor && this.state.jsoneditorid) {
                this.jsoneditor = import('jsoneditor');
                this.jsoneditor.then(({default: JSONEditor})=> {
                    this.jsoneditor = new JSONEditor(document.getElementById(this.state.jsoneditorid), {
                        onChangeJSON: json => this.setState({newconfig: json})
                    }, this.state.config);
                });
            } else {
                this.jsoneditor.set(this.state.config);
            }
        } catch (err) {
            this.nativejsoneditor = e(LocalJSONEditor, {
                key: 'ConfigEditor',
                path: '/',
                json: this.state.config,
                selectedDeviceId: this.props.selectedDeviceId,
                editable: true
            });
        }
    }

    getEditor() {
        new Promise((resolve,reject)=>resolve(this.setState({jsoneditorid:"fancy-editor"})));
        
        return [
            e("div", { key: 'fancy-editor', id: `fancy-editor`, "data-theme": "spectre" }),
            this.nativejsoneditor
        ]
    }

    getEditorGroups() {
        return e(ConfigGroup, { key: "configGroups", config: this.state?.newconfig, onChange: (_) => {this.jsoneditor?.set(this.state.newconfig); this.setState(this.state) } });
    }

    saveChanges() {
        var abort = new AbortController()
        var timer = setTimeout(() => abort.abort(), 8000);
        wfetch(`${httpPrefix}/config`, {
                method: 'put',
                signal: abort.signal,
                body: JSON.stringify(fromPlainToVersionned(this.state.newconfig, this.state.original))
            }).then(resp => resp.json())
            .then(fromVersionedToPlain)
            .then(config => {
                clearTimeout(timer);
                this.setState({ config: config });
            }).catch(console.err);
    }

    renderConfigGroup(name,contentFnc,icon) {
        return <div>
            <ListItemButton onClick={_ => {
                (this.state[name]?this.state[name]:(this.state[name]={})).opened = !this.state[name]?.opened;
                this.setState({ [name]: this.state[name] });
            } }>
                <ListItemIcon>
                    <FontAwesomeIcon icon={icon}></FontAwesomeIcon>
                </ListItemIcon>
                <ListItemText primary={name} />
                {this.state[name]?.opened ? <FontAwesomeIcon icon={faChevronUp}></FontAwesomeIcon> :
                                            <FontAwesomeIcon icon={faChevronDown}></FontAwesomeIcon>}
            </ListItemButton>
            <Collapse in={this.state[name]?.opened} timeout="auto">
                {this.getConfigGroupContent(name,contentFnc,this.state[name]?.opened)}
            </Collapse>
        </div>;
    }

    getConfigGroupContent(name,contentFnc,opened) {
        var cfg = this.state[name]?this.state[name]:(this.state[name]={});

        if (opened && !cfg.job) {
            cfg.job = new Promise((resolve,reject) => {
                resolve(contentFnc());
            });

            this.setState({[name]:cfg});
        }

        return cfg.content || <FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />;
    }

    componentDidUpdate(prevProps,prevState) {
        Object.entries(this.state).forEach(state => {
            if (state[1].job && !state[1].jobrunning) {
                state[1].jobrunning=true;
                state[1].job.then(content => {return { ...this.state[state[0]],content:content } })
                            .then(curstate => this.setState({[state[0]]:curstate}));
            }
        });
        if (!this.jsoneditor && !this.nativejsoneditor && this.state.config && this.state.jsoneditorid) {
            this.buildEditor();
        }

        if (this.jsoneditor) {
            if (this.state.newconfig && (prevState.newconfig !== this.state.newconfig)) {
                this.jsoneditor.set(this.state.newconfig)
            } else if (this.state.config && (prevState.config !== this.state.config)) {
                this.jsoneditor.set(this.state.config)
            }
        }

    }

    render() {
        return [<div className="button-bar">
                    <Button onClick={this.fetchConfig.bind(this) }>Refresh</Button>
                    <Button onClick={this.saveChanges.bind(this) }>Save</Button>
                </div>,
                this.renderConfigGroup("Defined components",this.getEditorGroups.bind(this),faGears),
                this.renderConfigGroup("Full Configuration",this.getEditor.bind(this),faGear)
                ];
    }
}