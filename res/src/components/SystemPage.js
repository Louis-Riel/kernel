class LogLine extends React.Component {
    isScrolledIntoView()
    {
        var rect = this.logdiv.getBoundingClientRect();
        var elemTop = rect.top;
        var elemBottom = rect.bottom;
    
        return (elemTop >= 0) && (elemBottom <= window.innerHeight);
    }

    render() {
        var msg = this.props.logln.match(/^[^IDVEW]*(.+)/)[1];
        var lvl = msg.substr(0, 1);
        var func = msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");
        var logLn = this.props.logln.substr(this.props.logln.indexOf(func) + func.length + 2).replaceAll(/^[\r\n]*/g, "").replaceAll(/.\[..\n$/g, "");

        return e("div", { key: genUUID() , className: `log LOG${lvl}` }, [
            e("div", { key: genUUID(), ref: ref => this.logdiv = ref, className: "LOGLEVEL" }, lvl),
            e("div", { key: genUUID(), className: "LOGDATE" }, msg.substr(3, 12)),
            e("div", { key: genUUID(), className: "LOGFUNCTION" }, func),
            e("div", { key: genUUID(), className: "LOGLINE" }, logLn)
        ]);
    }
}

class LogLines extends React.Component {
    AddLogLine(logln) {
        var logLines = (this.state?.logLines||[]);
        logLines.push(logln);
        this.setState({ logLines: logLines });
    }

    render() {
        if (this.props.registerLogCallback) {
            this.props.registerLogCallback(this.AddLogLine.bind(this));
        }
        return e("div", { key: genUUID(), className: "loglines" }, this.state?.logLines ? this.state.logLines.map(logln => e(LogLine,{ key: genUUID(), logln:logln})):null)
    }
}

class SystemPage extends React.Component {
    componentDidMount() {
        if (this.props.active) {
            document.getElementById("Logs").scrollIntoView()
        }
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
                e(FirmwareUpdater, { key: genUUID() })
            ]),
            e(LogLines, { key: genUUID(), registerLogCallback:this.props.registerLogCallback })
        ];
    }
}
