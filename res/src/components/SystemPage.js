class LogLine extends React.Component {
    constructor(props) {
        super(props);
        var msg = this.props.logln.match(/^[^IDVEW]*(.+)/)[1];
        this.state = {
            level: msg.substr(0, 1),
            date: msg.substr(3, 12),
            function: msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1"),
        }
        this.state.msg = this.props.logln.substr(this.props.logln.indexOf(this.state.function) + this.state.function.length + 2).replaceAll(/^[\r\n]*/g, "").replaceAll(/.\[..\n$/g, "")
    }

    render() {
        return e("div", { key: genUUID(), className: `log LOG${this.state.level}` }, [
            e("div", { key: genUUID(), className: "LOGLEVEL" }, this.state.level),
            e("div", { key: genUUID(), className: "LOGDATE" }, this.state.date),
            e("div", { key: genUUID(), className: "LOGFUNCTION" }, this.state.function),
            e("div", { key: genUUID(), className: "LOGLINE" }, this.state.msg)
        ]);
    }
}

class SystemPage extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            logLines: []
        }
        if (this.props.registerLogCallback) {
            this.props.registerLogCallback(this.AddLogLine.bind(this));
        }
    }

    AddLogLine(logln) {
        this.state.logLines.push(e(LogLine, { key: genUUID(), logln: logln }));
        if (this.loaded)
            this.setState({ logLines: this.state.logLines });
    }
    componentDidMount() {
        this.loaded = true;
    }

    SendCommand(body) {
        return fetch(`${httpPrefix}/status/cmd`, {
            method: 'PUT',
            body: JSON.stringify(body)
        }).then(res => res.text().then(console.log))
          .catch(console.error)
          .catch(console.error);
    }

    render() {
        return [
            e("div", { key: genUUID() }, [
                e("button", { key: genUUID(), onClick: elem => this.setState({ logLines: [] }) }, "Clear Logs"),
                e("button", { key: genUUID(), onClick: elem => this.SendCommand({ 'command': 'reboot' }) }, "Reboot"),
                e("button", { key: genUUID(), onClick: elem => this.SendCommand({ 'command': 'parseFiles' }) }, "Parse Files"),
                e("button", { key: genUUID(), onClick: elem => this.SendCommand({ 'command': 'factoryReset' }) }, "Factory Reset"),
            ]),
            e(FirmwareUpdater, { key: genUUID() }),
            e("div", { key: genUUID(), className: "loglines" }, this.state.logLines)
        ];
    }
}
