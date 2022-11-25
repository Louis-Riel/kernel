import { createElement as e, Component, Suspense } from 'react';
import { Button,Collapse,ListItemButton,ListItemIcon,ListItemText } from '@mui/material';
import {wfetch, fromVersionedToPlain,fromPlainToVersionned} from '../../../utils/utils'
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faSpinner } from '@fortawesome/free-solid-svg-icons/faSpinner'
import { faGears } from '@fortawesome/free-solid-svg-icons'
import { faChevronDown } from '@fortawesome/free-solid-svg-icons/faChevronDown'
import { faChevronUp } from '@fortawesome/free-solid-svg-icons/faChevronUp'
import ConfigGroup from './config/ConfigGroup';
import './Config.css'
import './JSONEditor.css'
import 'jsoneditor/dist/jsoneditor.css';

export default class ConfigPage extends Component {
    constructor(props) {
        super(props);
        this.state={
            httpPrefix:this.props.selectedDevice?.ip ? `http://${this.props.selectedDevice.config.devName}` : ".",
        };
        this.fetchConfig();
    }

    fetchConfig() {
        wfetch(`${this.state.httpPrefix}/config/${!this.props.selectedDevice?.config?.deviceid?"":this.props.selectedDevice.config.deviceid+".json"}`, {
                method: 'post',
                //signal: abort.signal
            }).then(resp => resp.json())
              .then(config => {
                //clearTimeout(timer);
                this.props.selectedDevice.config = fromVersionedToPlain(config);
                this.setState({
                    config: fromVersionedToPlain(config),
                    newconfig: fromVersionedToPlain(config),
                    original: config
                });
            });
    }

    getEditorGroups() {
        return e(ConfigGroup, { key: "configGroups", selectedDevice:this.props.selectedDevice, config: this.state?.newconfig, onChange: (_) => {this.jsoneditor?.update(this.state.newconfig); this.setState({newconfig: this.state.newconfig}) } });
    }

    saveChanges() {
        let abort = new AbortController()
        let timer = setTimeout(() => abort.abort(), 8000);
        wfetch(`${this.state.httpPrefix}/config`, {
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
                <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
                    {contentFnc()}
                </Suspense>
            </Collapse>
        </div>;
    }

    componentDidUpdate(prevProps,prevState) {
        if (prevProps?.selectedDevice !== this.props.selectedDevice) {
            if (this.props.selectedDevice?.ip) {
                this.setState({httpPrefix:`https://${this.props.selectedDevice.config.devName}`});
            } else {
                this.setState({httpPrefix:"."});
            }
        }

        if (this.jsoneditor) {
            if (this.state.newconfig && (this.jsoneditor.get() !== this.state.newconfig)) {
                this.jsoneditor.update(this.state.newconfig)
            } else if (this.state.config && (this.jsoneditor.get() !== this.state.config)) {
                this.jsoneditor.update(this.state.config)
            }
        }

        if (this.state["Full Configuration"]?.opened && !this.jsoneditor && this.container) {
            import('jsoneditor').then(({default: JSONEditor})=> {
                this.jsoneditor = new JSONEditor(this.container, {
                    onChangeJSON: json => this.setState({newconfig: json})
                }, this.state.config);
            });    
        }

        if (prevState.httpPrefix !== this.state.httpPrefix) {
            this.fetchConfig();
        }
    }

    render() {
        return [<div className="button-bar">
                    <Button onClick={this.fetchConfig.bind(this) }>Refresh</Button>
                    <Button onClick={this.saveChanges.bind(this) }>Save</Button>
                </div>,
                this.renderConfigGroup("Defined Components",this.getEditorGroups.bind(this),faGears),
                this.renderConfigGroup("Full Configuration",()=><Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
                    <div className="jsoneditor-react-container" data-theme="spectre" ref={elem => this.container = elem} ></div>
                </Suspense>,faGears)
            ];
    }
}