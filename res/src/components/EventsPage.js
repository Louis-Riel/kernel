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
        if (this.props.registerEventCallback) {
            this.props.registerEventCallback(this.ProcessEvent.bind(this));
        }
        this.mounted=false;
        this.state = {filters:{}};
    }

    componentDidMount() {
        this.mounted=true;
    }

    componentWillUnmount() {
        this.mounted=false;
    }

    ProcessEvent(evt) {
        if (this.mounted){
            var lastEvents = this.state?.lastEvents||[];
            while (lastEvents.length > 100) {
                lastEvents.shift();
            }
            var curFilters = Object.entries(lastEvents.filter(evt=>evt.eventBase && evt.eventId)
                                   .reduce((ret,evt)=>{
                                       if (!ret[evt.eventBase]) {
                                          ret[evt.eventBase] = {filtered: false, eventIds:[{filtered: false, eventId: evt.eventId}]};
                                       } else if (!ret[evt.eventBase].eventIds.find(vevt=>vevt.eventId === evt.eventId)) {
                                        ret[evt.eventBase].eventIds.push({filtered:false, eventId: evt.eventId});
                                       }
                                       return ret;
                                    },{}))
            var hasUpdates = false;
            Object.values(curFilters).forEach(filter => {
                if (!this.state.filters[filter[0]]) {
                    this.state.filters[filter[0]] = filter[1];
                    hasUpdates = true;
                } else if (filter[1].eventIds.find(newEvt => !this.state.filters[filter[0]].eventIds.find(eventId => eventId.eventId === newEvt.eventId))) {
                    this.state.filters[filter[0]].eventIds = this.state.filters[filter[0]].eventIds.concat(filter[1].eventIds.filter(eventId=> !this.state.filters[filter[0]].eventIds.find(eventId2=> eventId.eventId === eventId2.eventId) ))
                    hasUpdates = true;
                }
            });

            if (hasUpdates) {
                this.setState({
                    filters: this.state.filters
                });
            }

            if (!Object.entries(this.state.filters).find(filter => filter[0] === evt.eventBase && filter[1].filtered || (filter[0] === evt.eventBase && filter[1].eventIds.find(eventId=>eventId.filtered && eventId.eventId === evt.eventId)))){
                this.setState({
                    lastEvents:lastEvents.concat(evt)
                });
            }
        }
    }

    parseEvent(event) {
        var eventType = this.eventTypes.find(eventType => (eventType.eventBase == event.eventBase) && (eventType.eventName == event.eventId))
        if (eventType) {
            return {dataType:eventType.dataType,...event};
        } else {
            var req = this.eventTypeRequests.find(req=>(req.eventBase == event.eventBase) && (req.eventId == event.eventId));
            if (!req && this.mounted) {
                this.getEventDescriptor(event)
                    .then(eventType => this.setState({lastState: this.state.lastEvents
                        .map(event => event.eventBase == eventType.eventBase && event.eventId == eventType.eventName ? {dataType:eventType.dataType,...event}:{event})}
                    ));
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
                wfetch(`${httpPrefix}/eventDescriptor/${event.eventBase}/${event.eventId}`, {
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

    updateFilters(eventId, enabled) {
        eventId.filtered = enabled;
        this.setState({filters: this.state.filters});
    }

    filterPanel() {
        return e("div",{key: "filterPanel", className:"filterPanel"},
            Object.entries(this.state.filters)
                   .map(event => e('div',{key:event[0], className: `filter ${event[0]}`}, [
                        e("input",{key:"filtered",
                                 checked: event[1].filtered, 
                                 onChange: event=> this.updateFilters(event[1], event.target.checked),
                                 type:"checkbox"}),
                        e("div",{key:"label", className:`label ${event[0]}`},event[0]),
                        e("div",{key:"filterList", className: `filters`},event[1].eventIds.map(eventId => e("div",{key:eventId.eventId, className:`filteritem ${eventId.eventId}`},[
                            e("input",{key:"filtered",
                                       checked: eventId.filtered, 
                                       onChange: event=> this.updateFilters(eventId, event.target.checked),
                                       type:"checkbox"}),
                            e("div",{key:"chklabel",className:"label"},eventId.eventId)
                        ])))
                        ])
                    )
        );
    }

    render() {
        return  e("div", { key: "eventPanel" ,className: "eventPanel" }, [
            e("div", { key: "control", className:"control" }, [
                e("div",{ key: "header"}, `${this.state?.lastEvents?.length || "Waiting on "} event${this.state?.lastEvents?.length?'s':''}`),
                e("button",{ key: "clearbtn", onClick: elem => this.setState({lastEvents:[]})},"Clear")
            ]),
            this.filterPanel(),
            e("div",{ key: "eventList", className:"eventList"},this.state?.lastEvents?.map((event,idx) => e(LiveEvent,{ key: idx, event:this.parseEvent(event)})).reverse())
        ])

    }
}

class EventsPage extends React.Component {

    componentWillUnmount() {
        this.mounted=false;
    }

    componentDidMount(){
        this.mounted=true;
        if (window.location.hostname || httpPrefix)
            this.getJsonConfig().then(cfg => this.mounted?this.setState({events: cfg.events,programs:cfg.programs}):null);
    }

    getJsonConfig() {
        return new Promise((resolve, reject) => {
            if (window.location.hostname || httpPrefix){
                const timer = setTimeout(() => this.props.pageControler.abort(), 3000);
                wfetch(`${httpPrefix}/config${this.props.selectedDeviceId == "current"?"":`/${this.props.selectedDeviceId}`}`, {
                    method: 'post',
                    signal: this.props.pageControler.signal
                }).then(data => {
                    clearTimeout(timer);
                    return data.json();
                }).then( data => resolve(fromVersionedToPlain(data)
                )).catch((err) => {
                    clearTimeout(timer);
                    reject(err);
                });
            } else {
                reject({error:"Not connected"});
            }
        });
    }

    render() {
        if (this.state?.events){
            return [
                e(LiveEventPannel,{ key: "eventpannel",registerEventCallback:this.props.registerEventCallback})
            ];
        } else {
            return e("div",{key: "loading"},"Loading.....");
        }
    }
}
