import {createElement as e, Component} from 'react';
import { Button, FormControlLabel, Checkbox} from '@mui/material';
import './Events.css';
import { wfetch,fromVersionedToPlain } from '../../../utils/utils';

var httpPrefix = "";

class LiveEvent extends Component {
    render() {
        if (this.props?.event?.dataType) {
            return this.renderComponent(this.props.event);
        } else {
            return e("summary",{key: "summary" ,className: "liveEvent"},
                    e("div",{key: "description" ,className: "description"},[
                        e("div",{key: "base" ,className: "eventBase"},"Loading"),
                        e("div",{key: "id" ,className: "eventId"},"...")
                    ]));
        }
    }

    renderComponent(event) {
        return e("summary",{key: "event" ,className: "liveEvent"},[
            e("div",{key: "description" ,className: "description"},[
                e("div",{key: "base" ,className: "eventBase"},event.eventBase),
                e("div",{key: "id" ,className: "eventId"},event.eventId)
            ]), event.data ? e("details",{key: "details" ,className: "data"},this.parseData(event)): null
        ]);
    }

    parseData(props) {
        if (props.dataType != "JSON") {
            return (
                <div  className= "description">  
                    <div className= "propName">data</div>
                    <div className= "propValue">{ props.data }</div>
                </div>
            );
        }
        return Object.keys(props.data)
            .filter(prop => typeof props.data[prop] != 'object' && !Array.isArray(props.data[prop]))
            .map(prop => e("div", { key: prop, className: "description" }, [
                e("div", { key: "name", className: "propName" }, prop),
                e("div", { key: "data", className: prop }, props.data[prop])
            ]));
    }
}

class LiveEventPannel extends Component {
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
        if (evt && this.mounted && this.isEventVisible(evt)) {
            var lastEvents = (this.state?.lastEvents||[]).concat(evt);
            while (lastEvents.length > 100) {
                lastEvents.shift();
            }
            this.setState({
                filters: this.updateFilters(lastEvents),
                lastEvents: lastEvents
            });
        }
    }

    updateFilters(lastEvents) {
        var curFilters = Object.entries(lastEvents
            .filter(evt => evt && evt.eventBase && evt.eventId)
            .reduce((ret, evt) => {
                if (!ret[evt.eventBase]) {
                    ret[evt.eventBase] = { visible: true, eventIds: [{ visible: true, eventId: evt.eventId }] };
                } else if (!ret[evt.eventBase].eventIds.find(vevt => vevt.eventId === evt.eventId)) {
                    ret[evt.eventBase].eventIds.push({ visible: true, eventId: evt.eventId });
                }
                return ret;
            }, {}));
        Object.values(curFilters).forEach(filter => {
            if (!this.state.filters[filter[0]]) {
                this.state.filters[filter[0]] = filter[1];
            } else if (filter[1].eventIds.find(newEvt => !this.state.filters[filter[0]].eventIds.find(eventId => eventId.eventId === newEvt.eventId))) {
                this.state.filters[filter[0]].eventIds = this.state.filters[filter[0]].eventIds.concat(filter[1].eventIds.filter(eventId => !this.state.filters[filter[0]].eventIds.find(eventId2 => eventId.eventId === eventId2.eventId)));
            }
        });
        return this.state.filters;
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
                const timer = setTimeout(() => toControler.abort(), 8000);
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

    updateEventIdFilter(eventId, enabled) {
        eventId.visible = enabled;
        this.setState({filters: this.state.filters});
    }

    updateEventBaseFilter(eventBase, enabled) {
        eventBase.visible = enabled;
        this.setState({filters: this.state.filters});
    }

    filterPanel() {
        return e("div",{key: "filterPanel", className:"filterPanel"},[
                e("div",{key:"filters",className:"filters"},[
                    e("div",{ key: "label", className:"header"}, `Filters`),
                    e("div",{key:"filterlist",className:"filterlist"},Object.entries(this.state.filters).map(this.renderFilter.bind(this)))
                ]),
                e("div", { key: "control", className:"control" }, [
                    this.state?.lastEvents ? e("div",{ key: "label", className:"header"}, `${this.state.lastEvents.length}/${this.state.lastEvents.filter(this.isEventVisible.bind(this)).length} event${this.state?.lastEvents?.length?'s':''}`) : "No events",
                    e(Button,{ key: "clearbtn", onClick: elem => this.setState({lastEvents:[]})},"Clear")
                ])
               ]);
    }

    renderFilter(filter) {
        return e('div', { key: filter[0], className: `filter ${filter[0]}` },
            e("div",{key:"evbfiltered",className:"evbfiltered"},
                e(FormControlLabel,{
                    key:"visible",
                    className:"ebfiltered",
                    label: filter[0],
                    control:e(Checkbox, {
                        key: "ctrl",
                        checked: filter[1].visible,
                        onChange: event => this.updateEventIdFilter(filter[1], event.target.checked)
                    })})
            ),
            e("div", { key: "filterList", className: `eventIds` }, filter[1].eventIds.map(eventId => e("div", { key: eventId.eventId, className: `filteritem ${eventId.eventId}` }, [
                e("div",{key:"evifiltered",className:"evifiltered"},
                    e(FormControlLabel,{
                        key:eventId.eventId,
                        className:"eifiltered",
                        label: eventId.eventId,
                        control:e(Checkbox, {
                            key: "ctrl",
                            checked: eventId.visible,
                            onChange: event => this.updateEventIdFilter(eventId, event.target.checked),
                        })}
                    )
                )
            ]))));
    }

    isEventVisible(event) {
        return event && ((Object.keys(this.state.filters).length === 0) || !this.state.filters[event.eventBase]?.eventIds?.some(eventId=> eventId.eventId === event.eventId)) || 
               (Object.keys(this.state.filters).some(eventBase => event.eventBase === eventBase && this.state.filters[eventBase].visible) &&
               Object.keys(this.state.filters).some(eventBase => event.eventBase === eventBase && 
                                                                  this.state.filters[eventBase].eventIds
                                                                    .some(eventId=> eventId.eventId === event.eventId && eventId.visible)));
    }

    getFilteredEvents() {
        return e("div", { key: "eventList", className: "eventList" }, 
                this.state?.lastEvents?.filter(this.isEventVisible.bind(this))
                           .map((event, idx) => e(LiveEvent, { key: idx, event: this.parseEvent(event) })).reverse());
    }

    render() {
        return  e("div", { key: "eventPanel" ,className: "eventPanel" }, [
            this.filterPanel(),
            this.getFilteredEvents()
        ])

    }
}

export default class EventsPage extends Component {

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
                const timer = setTimeout(() => this.props.pageControler.abort(), 8000);
                wfetch(`${httpPrefix}/config${this.props.selectedDeviceId == "current"?"":`/${this.props.selectedDeviceId}`}`, {
                    method: 'post'
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
