import {createElement as e, Component} from 'react';
import { Button } from '@mui/material';
import {wfetch, fromVersionedToPlain,fromPlainToVersionned, genUUID} from '../../../utils/utils'
import LocalJSONEditor from '../../controls/JSONEditor/JSONEditor'
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
            if (!this.jsoneditor) {
                import('jsoneditor').then(({default: JSONEditor})=> {
                    this.jsoneditor = new JSONEditor(this.container, {
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

    componentDidMount() {
        this.buildEditor();
    }

    componentDidUpdate(prevProps,prevState) {
        if (this.jsoneditor) {
            if (this.state.newconfig && (prevState.newconfig !== this.state.newconfig)) {
                this.jsoneditor.set(this.state.newconfig)
            } else if (this.state.config && (prevState.config !== this.state.config)) {
                this.jsoneditor.set(this.state.config)
            }
        }
    }

    getEditor() {
        return [
            e("div", { key: 'fancy-editor', ref: (elem) => this.container = elem, id: `${this.props.id || genUUID()}`, "data-theme": "spectre" }),
            this.nativejsoneditor
        ]
    }

    getEditorGroups() {
        return e(ConfigGroup, { key: "configGroups", config: this.state?.newconfig, fullEditor:this.getEditor.bind(this), onChange: (_) => {this.jsoneditor?.set(this.state.newconfig); this.setState(this.state) } });
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

    render() {
        return [e("div", { key: 'button-bar', className: "button-bar" }, [
                    e(Button, { key: "refresh", onClick: elem => this.fetchConfig.bind(this) }, "Refresh"),
                    e(Button, { key: "save", onClick: this.saveChanges.bind(this) }, "Save"),
                ]),
                this.getEditorGroups(),
                this.getEditor()
                ];
    }
}