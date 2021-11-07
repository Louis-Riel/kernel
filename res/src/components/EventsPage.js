class LiveEvent extends React.Component {
    render() {
        if (this.props?.event?.dataType) {
            return this.renderComponent(this.props.event);
        } else {
            return e("summary",{key: genUUID() ,className: "liveEvent"},
                    e("div",{key: genUUID() ,className: "description"},[
                        e("div",{key: genUUID() ,className: "eventBase"},"Loading"),
                        e("div",{key: genUUID() ,className: "eventId"},"...")
                    ]));
        }
    }

    renderComponent(event) {
        return e("summary",{key: genUUID() ,className: "liveEvent"},[
            e("div",{key: genUUID() ,className: "description"},[
                e("div",{key: genUUID() ,className: "eventBase"},event.eventBase),
                e("div",{key: genUUID() ,className: "eventId"},event.eventId)
            ]), event.data ? e("details",{key: genUUID() ,className: "data"},this.parseData(event)): null
        ]);
    }

    parseData(props) {
        if (props.dataType == "Number") {
            return e("div", { key: genUUID(), className: "description" }, 
                        e("div", { key: genUUID(), className: "propNumber" }, props.data)
                    );
        }
        return Object.keys(props.data)
            .filter(prop => typeof props.data[prop] != 'object' && !Array.isArray(props.data[prop]))
            .map(prop => e("div", { key: genUUID(), className: "description" }, [
                e("div", { key: genUUID(), className: "propName" }, prop),
                e("div", { key: genUUID(), className: prop }, props.data[prop])
            ]));
    }
}

class LiveEventPannel extends React.Component {
    constructor(props) {
        super(props);
        this.eventTypes=[];
        this.eventTypeRequests=[];
        this.state = {lastEvents:[]}
    }

    componentDidMount(){
        if (this.props.registerEventCallback) {
            this.props.registerEventCallback(this.ProcessEvent.bind(this));
        }
    }

    ProcessEvent(evt) {
        var lastEvents = this.state.lastEvents;
        while (lastEvents.length > 100) {
            lastEvents.shift();
        }
        this.setState({lastEvents:lastEvents.concat(evt)});
    }

    parseEvent(event) {
        var eventType = this.eventTypes.find(eventType => (eventType.eventBase == event.eventBase) && (eventType.eventName == event.eventId))
        if (eventType) {
            return {dataType:eventType.dataType,...event};
        } else {
            var req = this.eventTypeRequests.find(req=>(req.eventBase == event.eventBase) && (req.eventId == event.eventId));
            if (!req) {
                this.getEventDescriptor(event)
                    .then(eventType => this.setState({lastState: this.state.lastEvents
                        .map(event => event.eventBase == eventType.eventBase && event.eventId == eventType.eventName ? {dataType:eventType.dataType,...event}:{event})}));
            }
            return {...event};
        }
    }

    getEventDescriptor(event) {
        var curReq;
        this.eventTypeRequests.push((curReq={waiters:[],...event}));
        return new Promise((resolve,reject) => {
            var eventType = this.eventTypes.find(eventType => (eventType.eventBase == event.eventBase) && (eventType.eventName == event.eventId))
            if (eventType) {
                resolve({dataType:eventType.dataType,...event});
            } else {
                var toControler = new AbortController();
                const timer = setTimeout(() => toControler.abort(), 3000);
                fetch(`${httpPrefix}/eventDescriptor/${event.eventBase}/${event.eventId}`, {
                    method: 'post',
                    signal: toControler.signal
                }).then(data => {
                    clearTimeout(timer);
                    return data.json();
                }).then( eventType => {
                    this.eventTypes.push(eventType);
                    resolve(eventType);
                }).catch((err) => {
                    console.error(err);
                    clearTimeout(timer);
                    curReq.waiters.forEach(waiter => {
                        waiter.reject(err);
                    });
                    reject(err);
                });
            }
        })
    }

    render() {
        return  e("div", { key: genUUID() ,className: "eventPanel" }, [
            e("div", { key: genUUID(), className:"control" }, [
                e("div",{ key: genUUID()}, `${this.state?.lastEvents?.length || "Waiting on "} event${this.state?.lastEvents?.length?'s':''}`),
                e("button",{ key: genUUID(), onClick: elem => this.setState({lastEvents:[]})},"Clear")
            ]),
            e("div",{ key: genUUID(), className:"eventList"},this.state?.lastEvents?.map(event => e(LiveEvent,{ key: genUUID(), event:this.parseEvent(event)})).reverse())
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
