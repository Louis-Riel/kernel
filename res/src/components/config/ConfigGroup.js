class ConfigGroup extends React.Component {
    constructor(props) {
        super(props);
        this.supportedTypes = {
            analogPins:{
                caption:"Analog Pins",
                isArray: true,
                component: AnalogPinConfig
            },pins:{
                caption:"Digital Pins",
                isArray: true,
                component: DigitalPinConfig
            }
        };
        this.state = {
            currentTab: props.config ? Object.keys(props.config).filter(this.isSupported.bind(this))[0] : undefined
        };
    }

    render() {
        if (this.props.config) {
            var tabs = ["pins","analogPins"];
            return [
                e( MaterialUI.Tabs, { 
                    value: this.state.currentTab ? this.state.currentTab : tabs[0], 
                    onChange: (e,v)=>{this.setState({currentTab:v})},
                    key: "ConfigTypes" 
                }, [...tabs.map(this.renderTypeTab.bind(this)),
                       e( MaterialUI.Tab, { key: "full-config", label: "Configuration", value: "Configuration" })]),
                tabs.map(this.renderConfigType.bind(this))
            ];
        } else {
            return e("div", {key: "loading"}, "Loading...");
        }
    }

    renderTypeTab(key) {
        return e( MaterialUI.Tab, { key: key, label: this.supportedTypes[key].caption, value: key });
    }

    renderConfigType(key) {
        return e("div",{key:`${key}-control-panel`,className:`edior-pannel ${this.state.currentTab === key ? "":"hidden"}`},[
            e("button", { key: "add", onClick: evt=> {this.props.config[key] ? this.props.config[key].push({}) : this.props.config[key] = [{}]; this.props.onChange()} }, e("i",{key:"add", className:"fa fa-plus-square"})),
            e("div",{key:"items", className:`config-cards`}, this.props.config[key] ? Object.keys(this.props.config[key]).map(idx =>
                this.renderEditor(key,this.props.config[key],idx)) : null)
        ]);
    }

    renderConfigItemTab(key, item, idx) {
        return e( MaterialUI.Tab, { key: item, label: idx+1, value: key });
    }

    renderEditor(key, item, idx) {
        return  e("div",{key:`${key}-${idx}-control-editor`,className:`control-editor`},[
                    e( this.supportedTypes[key].component, { key: key + idx, value: key, role: "tabpanel", item: item[idx], onChange: this.props.onChange, ...this.supportedTypes[key].properties}),
                    e("button", { key: "dup", onClick: evt=> {this.props.config[key].push(JSON.parse(JSON.stringify(this.props.config[key][idx]))); this.props.onChange()} }, e("i",{key:"copy", className:"fa fa-clone"})),
                    e("button", { key: "delete", onClick: evt=> {this.props.config[key].splice(idx,1); this.props.onChange()} }, e("i",{key:"copy", className:"fa fa-trash-o"})),
                ]);
    }

    isArray(key) {
        return this.isSupported(key) && this.supportedTypes[key].isArray;
    }

    isSupported(key) {
        return Object.keys(this.supportedTypes).indexOf(key)>-1;
    }
}