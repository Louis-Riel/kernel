class ConfigGroup extends React.Component {
    constructor(props) {
        super(props);
        this.state = {};
        this.supportedTypes = {analogPins:{
            isArray: true,
            component: AnalogPinConfig
        }};
    }

    render() {
        var tabs = Object.keys(this.props.config).filter(this.isSupported.bind(this));
        return [
            e( MaterialUI.Tabs, { 
                value: tabs, 
                onChange: (e,v)=>{this.setValue(v)},
                key: "ConfigTypes" 
            }, tabs.map(this.renderTypeTab.bind(this))),
            e(MaterialUI.Switch,{key:"router"},
            tabs.map(key => e(MaterialUI.Route,{
                                    key:key, 
                                    role:"tabpanel", 
                                    value: key
                                },this.renderConfigType(key))))
        ];
    }

    renderTypeTab(key) {
        return e( MaterialUI.Tab, { key: key, label: key, value: key });
    }

    renderConfigType(key) {
        if (this.isArray(key)) {
            e( MaterialUI.Tabs, { 
                key: key, 
                value: Object.keys(this.props.config[key]) 
            }, this.props.config[key].map(this.renderConfigItemTab.bind(this, key)));
        }
        return null;
    }

    renderConfigItemTab(key, item, idx) {
        return e( MaterialUI.Tab, { key: item, label: item.name, value: idx });
    }

    renderEditor(key, item) {
        return item.name;
        return e( this.supportedTypes[key].component, { key: item.name, ...item });
    }

    isArray(key) {
        return this.isSupported(key) && this.supportedTypes[key].isArray;
    }

    isSupported(key) {
        return Object.keys(this.supportedTypes).indexOf(key)>-1;
    }
}