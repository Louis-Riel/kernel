class ConfigGroup extends React.Component {
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
            e(MaterialUI.Tabs, {
                value: this.state.currentTab,
                onChange: (_, v) => { this.setState({ currentTab: v }); },
                key: "ConfigTypes"
            }, [...tabs.map(this.renderTypeTab.bind(this)),
            e(MaterialUI.Tab, { key: "full-config", label: "Configuration", value: "Configuration" })]),
            tabs.map(this.renderConfigType.bind(this))
        ];
    }

    renderTypeTab(key) {
        return key.isArray ? 
                e( MaterialUI.Tab, { key: key.collectionName, label: this.supportedTypes[key.collectionName]?.caption || key.collectionName, value: key.collectionName }):
                e( MaterialUI.Tab, { key: key.class, label: this.supportedTypes[key.class]?.caption || key.class, value: key.class });
    }

    renderConfigType(key) {
        return e("div",{key:`${key.class}-control-panel`,className:`edior-pannel ${this.state.currentTab === key.collectionName ? "":"hidden"}`},[
            e("button", { key: "add", onClick: evt=> {this.props.config[key.collectionName] ? this.props.config[key.collectionName].push({}) : this.props.config[key.collectionName] = [{}]; this.props.onChange()} }, e("i",{key:"add", className:"fa fa-plus-square"})),
            e("div",{key:"items", className:`config-cards`}, this.props.config[key.collectionName] ? Object.keys(this.props.config[key.collectionName]).map(idx =>
                this.renderEditor(key,this.props.config[key.collectionName],idx)) : null)
        ]);
    }

    renderConfigItemTab(key, item, idx) {
        return e( MaterialUI.Tab, { key: item, label: idx+1, value: key });
    }

    renderEditor(key, item, idx) {
        return  e("div",{key:`${key.collectionName}-${idx}-control-editor`,className:`control-editor`},[
                    e( this.supportedTypes[key.collectionName]?.component || ConfigItem, { key: key.collectionName + idx, value: key, role: "tabpanel", item: item[idx], onChange: this.props.onChange}),
                    e("button", { key: "dup", onClick: evt=> {this.props.config[key.collectionName].push(JSON.parse(JSON.stringify(this.props.config[key.collectionName][idx]))); this.props.onChange()} }, e("i",{key:"copy", className:"fa fa-clone"})),
                    e("button", { key: "delete", onClick: evt=> {this.props.config[key.collectionName].splice(idx,1); this.props.onChange()} }, e("i",{key:"copy", className:"fa fa-trash-o"})),
                ]);
    }

    isSupported(key) {
        return Object.keys(this.supportedTypes).indexOf(key)>-1;
    }
}