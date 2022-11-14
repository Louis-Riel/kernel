import { createElement as e, Component } from 'react';
import { Button, FormControlLabel, Checkbox } from '@mui/material';
import { wfetch } from '../../../utils/utils';
import { LiveEvent } from './LiveEvent';

export class LiveEventPannel extends Component {
    constructor(props) {
        super(props);
        this.eventTypes = [];
        this.eventTypeRequests = [];
        if (this.props.registerEventCallback) {
            this.props.registerEventCallback(this.ProcessEvent.bind(this));
        }
        this.state = { 
            httpPrefix:this.props.selectedDevice?.ip ? `http://${this.props.selectedDevice.ip}` : ".",
            filters: {} 
        };
    }

    componentWillUnmount() {
        if (this.props.unRegisterEventInstanceCallback && this.props.name) {
            this.props.unRegisterEventInstanceCallback(this.ProcessEvent.bind(this),`${this.props.class}-${this.props.name}`);
        }
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (prevProps?.selectedDevice !== this.props.selectedDevice) {
            if (this.props.selectedDevice?.ip) {
                this.setState({httpPrefix:`http://${this.props.selectedDevice.ip}`});
            } else {
                this.setState({httpPrefix:"."});
            }
        }
    }

    ProcessEvent(evt) {
        if (evt && this.isEventVisible(evt)) {
            let lastEvents = (this.state?.lastEvents || []).concat(evt);
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
        let curFilters = Object.entries(lastEvents
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
        let eventType = this.eventTypes.find(eventType => (eventType.eventBase === event.eventBase) && (eventType.eventName === event.eventId));
        if (eventType) {
            return { dataType: eventType.dataType, ...event };
        } else {
            let req = this.eventTypeRequests.find(req => (req.eventBase === event.eventBase) && (req.eventId === event.eventId));
            if (!req) {
                this.getEventDescriptor(event)
                    .then(eventType => this.setState({
                        lastState: this.state.lastEvents
                            .map(event => event.eventBase === eventType.eventBase && event.eventId === eventType.eventName ? { dataType: eventType.dataType, ...event } : { event })
                    }
                    ));
            }
            return { ...event };
        }
    }

    getEventDescriptor(event) {
        let curReq;
        this.eventTypeRequests.push((curReq = { waiters: [], ...event }));
        return new Promise((resolve, reject) => {
            let eventType = this.eventTypes.find(eventType => (eventType.eventBase === event.eventBase) && (eventType.eventName === event.eventId));
            if (eventType) {
                resolve({ dataType: eventType.dataType, ...event });
            } else {
                let toControler = new AbortController();
                const timer = setTimeout(() => toControler.abort(), 8000);
                wfetch(`${this.state.httpPrefix}/eventDescriptor/${event.eventBase}/${event.eventId}`, {
                    method: 'post',
                    signal: toControler.signal
                }).then(data => {
                    clearTimeout(timer);
                    return data.json();
                }).then(eventType => {
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
        });
    }

    updateEventIdFilter(eventId, enabled) {
        eventId.visible = enabled;
        this.setState({ filters: this.state.filters });
    }

    filterPanel() {
        return e("div", { key: "filterPanel", className: "filterPanel" }, [
            e("div", { key: "filters", className: "filters" }, [
                e("div", { key: "label", className: "header" }, `Filters`),
                e("div", { key: "filterlist", className: "filterlist" }, Object.entries(this.state.filters).map(this.renderFilter.bind(this)))
            ]),
            e("div", { key: "control", className: "control" }, [
                this.state?.lastEvents ? e("div", { key: "label", className: "header" }, `${this.state.lastEvents.length}/${this.state.lastEvents.filter(this.isEventVisible.bind(this)).length} event${this.state?.lastEvents?.length ? 's' : ''}`) : "No events",
                e(Button, { key: "clearbtn", onClick: elem => this.setState({ lastEvents: [] }) }, "Clear")
            ])
        ]);
    }

    renderFilter(filter) {
        return e('div', { key: filter[0], className: `filter ${filter[0]}` },
            e("div", { key: "evbfiltered", className: "evbfiltered" },
                e(FormControlLabel, {
                    key: "visible",
                    className: "ebfiltered",
                    label: filter[0],
                    control: e(Checkbox, {
                        key: "ctrl",
                        checked: filter[1].visible,
                        onChange: event => this.updateEventIdFilter(filter[1], event.target.checked)
                    })
                })
            ),
            e("div", { key: "filterList", className: `eventIds` }, 
                filter[1].eventIds.map(eventId => e("div", { key: eventId.eventId, className: `filteritem ${eventId.eventId}` }, [
                    e("div", { key: "evifiltered", className: "evifiltered" },
                        e(FormControlLabel, {
                            key: eventId.eventId,
                            className: "eifiltered",
                            label: eventId.eventId,
                            control: e(Checkbox, {
                                key: "ctrl",
                                checked: eventId.visible,
                                onChange: event => this.updateEventIdFilter(eventId, event.target.checked),
                            })
                        }
                        )
                    )
                ]))
            )
        );
    }

    isEventVisible(event) {
        return event && (((Object.keys(this.state.filters).length === 0) || !this.state.filters[event.eventBase]?.eventIds?.some(eventId => eventId.eventId === event.eventId)) ||
            (Object.keys(this.state.filters).some(eventBase => event.eventBase === eventBase && this.state.filters[eventBase].visible) &&
                Object.keys(this.state.filters).some(eventBase => event.eventBase === eventBase &&
                    this.state.filters[eventBase].eventIds
                        .some(eventId => eventId.eventId === event.eventId && eventId.visible))));
    }

    getFilteredEvents() {
        return e("div", { key: "eventList", className: "eventList" },
            this.state?.lastEvents?.filter(this.isEventVisible.bind(this))
                .map((event, idx) => e(LiveEvent, { key: idx, event: this.parseEvent(event) })).reverse());
    }

    render() {
        return e("div", { key: "eventPanel", className: "eventPanel" }, [
            this.filterPanel(),
            this.getFilteredEvents()
        ]);

    }
}
