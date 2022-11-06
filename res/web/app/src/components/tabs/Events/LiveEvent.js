import { createElement as e, Component } from 'react';

export class LiveEvent extends Component {
    render() {
        if (this.props?.event?.dataType) {
            return this.renderComponent(this.props.event);
        } else {
            return e("summary", { key: "summary", className: "liveEvent" },
                e("div", { key: "description", className: "description" }, [
                    e("div", { key: "base", className: "eventBase" }, "Loading"),
                    e("div", { key: "id", className: "eventId" }, "...")
                ]));
        }
    }

    renderComponent(event) {
        return e("summary", { key: "event", className: "liveEvent" }, [
            e("div", { key: "description", className: "description" }, [
                e("div", { key: "base", className: "eventBase" }, event.eventBase),
                e("div", { key: "id", className: "eventId" }, event.eventId)
            ]), event.data ? e("details", { key: "details", className: "data" }, this.parseData(event)) : null
        ]);
    }

    parseData(props) {
        if (props.dataType !== "JSON") {
            return (
                <div className="description">
                    <div className="propName">data</div>
                    <div className="propValue">{props.data}</div>
                </div>
            );
        }
        return Object.keys(props.data)
            .filter(prop => typeof props.data[prop] !== 'object' && !Array.isArray(props.data[prop]))
            .map(prop => e("div", { key: prop, className: "description" }, [
                e("div", { key: "name", className: "propName" }, prop),
                e("div", { key: "data", className: prop }, props.data[prop])
            ]));
    }
}
