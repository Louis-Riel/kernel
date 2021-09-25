class Event extends React.Component {
    render() {
        return e("details",{key: genUUID() ,className: "event"},[
            e("summary",{key: genUUID()},[
                e("div",{key: genUUID(), className:"eventBase"},this.props.eventBase),
                e("div",{key: genUUID(), className:"eventId"},this.props.eventId)
            ]),
            Object.keys(this.props)
                  .filter(fld => !Array.isArray(this.props[fld]) && 
                                 (typeof this.props[fld] !== 'object') &&
                                 (fld != "eventBase") && (fld != "eventId"))
                  .map(fld => e("details", {key: genUUID(),className:fld},[e("summary",{key: genUUID(),className:fld},fld),this.props[fld]]))
        ]);
    }
}