class LiveEvent extends React.Component {
    render() {
        return e("summary",{key: genUUID() ,className: "liveEvent"},[
            e("div",{key: genUUID() ,className: "description"},[
                e("div",{key: genUUID() ,className: "eventBase"},this.props.eventBase),
                e("div",{key: genUUID() ,className: "eventId"},this.props.eventId)
            ]),
            this.props.data ? 
                e("details",{key: genUUID() ,className: "data"},Object.keys(this.props.data).map(prop => 
                    e("div",{key: genUUID() ,className: "description"},[
                        e("div",{key: genUUID() ,className: "propName"},prop),
                        e("div",{key: genUUID() ,className: prop},this.props.data[prop])
                    ])
                )): null
        ]);
    }
}

class LiveEventPannel extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            lastEvents: []
        };
        if (this.props.registerEventCallback) {
            this.props.registerEventCallback(this.ProcessEvent.bind(this));
        }
    }

    ProcessEvent(evt) {
        this.state.lastEvents.push(evt);
        while (this.state.lastEvents.length > 100) {
            this.state.lastEvents.shift();
        }
        this.setState({lastEvents:this.state.lastEvents});
    }

    render() {
        return  e("div", { key: genUUID() ,className: "eventPanel" }, [
            e("div", { key: genUUID(), className:"control" }, [
                e("div",{ key: genUUID()}, `${this.state.lastEvents.length} event${this.state.lastEvents.length?'s':''}`),
                e("button",{ key: genUUID(), onClick: elem => this.setState({lastEvents:[]})},"Clear")
            ]),
            e("div",{ key: genUUID(), className:"eventList"},this.state.lastEvents.map(event => e(LiveEvent,{ key: genUUID(), ...event})).reverse())
        ])

    }
}

class EventsPage extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            events: [],
            programs: []
        };
    }
    
    componentDidMount() {
        if (this.props.active) {
            document.getElementById("Events").scrollIntoView()
        }
        this.getJsonConfig().then(cfg => this.setState({events: cfg.events,programs:cfg.programs}));
    }

    getJsonConfig() {
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => this.props.pageControler.abort(), 3000);
            fetch(`${httpPrefix}/config${this.props.selectedDeviceId == "current"?"":`/${this.props.selectedDeviceId}`}`, {
                method: 'post',
                signal: this.props.pageControler.signal
            }).then(data => {
                clearTimeout(timer);
                return data.json();
            }).then( data => resolve(fromVersionedToPlain(data))).catch((err) => {
                clearTimeout(timer);
                reject(err);
            });
        });
    }

    render() {
        return [
            e("div", { key: genUUID() ,className: "designer" },[
                e("details",{ key: genUUID() ,className: "configuredEvents" }, [e("summary",{ key: genUUID()},`${this.state.events?.length} Events`), this.state.events?.map(event => e(Event,{ key: genUUID(),...event}))]),
                e("details",{ key: genUUID() ,className: "programs"},[e("summary",{ key: genUUID()},`${this.state.programs?.length} Programs`), this.state.programs?.map(program => e(Program,{ key: genUUID(),...program}))])
            ]),
            e(LiveEventPannel,{ key: genUUID(),registerEventCallback:this.props.registerEventCallback})
        ];
    }
}
