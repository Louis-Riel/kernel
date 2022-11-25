import {createElement as e, Component} from 'react';
import Chip from '@mui/material/Chip';
import { Button } from '@mui/material';
import './Logs.css';

export default class LogLines extends Component {
    constructor(props) {
        super(props);
        this.state={logLines:[],logLevels:{}};
        if (this.props.registerLogCallback) {
            this.props.registerLogCallback(this.AddLogLine.bind(this));
        }
    }

    AddLogLine(logln) {
        if (this.state.logLines.some(clogln => clogln === logln)) {
            return;
        }

        let recIdx=-1;
        let msg = logln.match(/^[^IDVEW]*(.+)/)[1];
        let lvl = msg.substr(0, 1);
        let ts = msg.substr(3, 12);
        let func = msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");

        if (this.state.logLines.some((clogln,idx) => {
            let nmsg = clogln.match(/^[^IDVEW]*(.+)/)[1];
            let nlvl = nmsg.substr(0, 1);
            let nts = nmsg.substr(3, 12);
            let nfunc = msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");
            return nlvl === lvl && nts === ts && nfunc === func;
        })) {
            recIdx=this.state.logLines.length;
        }
        while (this.state.logLines.length > 500) {
            this.state.logLines.shift();
        }

        if (this.state.logLevels[lvl] === undefined) {
            this.state.logLevels[lvl] = {visible:true};
        }

        if (this.state.logLevels[lvl][func] === undefined) {
            this.state.logLevels[lvl][func] = true;
        }

        if (recIdx >= 0) {
            this.state.logLines[recIdx] = logln;
        } else {
            this.state.logLines= [...(this.state?.logLines||[]),logln];
        }
        this.setState(this.state);
    }

    renderLogFunctionFilter(lvl,func,logLines) {    
        let label =  <div className={`log LOG${lvl}`}>{`${func} (${logLines.filter(logln => logln.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1") === func).length})`}</div>;
        return <Chip label={label} disabled={!this.state.logLevels[lvl].visible} className={this.state.logLevels[lvl][func] ? "enabled" : "filtered"} onClick={() => {this.state.logLevels[lvl][func] = !this.state.logLevels[lvl][func]; this.setState(this.state);}} />;
    }

    renderLogLevelFilterControl(lvl,logLines) {   
        let label =  <div className={`log LOG${lvl}`}>{`${lvl}(${logLines.length})`}</div>;
        return <Chip label={label} className={this.state.logLevels[lvl].visible ? "enabled" : "filtered"} onClick={() => {this.state.logLevels[lvl].visible = !this.state.logLevels[lvl].visible; this.setState(this.state);}} />;
    }

    renderLogFunctionFilters(lvl, logLines) {
        return Object.keys(this.state.logLevels[lvl])
                     .filter(func => func !== "visible")
                     .map(func => {
            return this.renderLogFunctionFilter(lvl,func,logLines)
        })
    }

    renderLogLevelFilter(lvl,logLines) {
        return e("div", { key: "logLevelFilter" + lvl, className: "logLevelFilter" }, [
            e("div", { key: "logLevelFilterTitle" + lvl, className: "logLevelFilterTitle" }, this.renderLogLevelFilterControl(lvl,logLines)), 
            e("div", { key: "logLevelFilterContent" + lvl, className: "logLevelFilterContent" }, 
                this.renderLogFunctionFilters(lvl, logLines))
        ]);
    }

    renderLogLevelFilters() {
        return Object.keys(this.state.logLevels).map(lvl => {
            return this.renderLogLevelFilter(lvl,this.state.logLines.filter(logln => logln.match(/^[^IDVEW]*(.+)/)[1].substr(0, 1) === lvl))
        })
    }

    renderFilterPannel() {
        return e("div", { key: "filterPannel", className: "filterPannel" }, [
            e("div", { key: "filterPannelContent",className: "filterPannelContent" }, this.renderLogLevelFilters())
        ]);
    }

    renderControlPannel() {
        return e("div", { key: "logControlPannel", className: "logControlPannel" }, [
            e(Button, { key: "clear", onClick: elem => this.setState({ logLines: [] }) }, "Clear Logs"),
            this.renderFilterPannel()
        ]);
    }

    isLogVisible(logLine) {
        let msg = logLine.match(/^[^IDVEW]*(.+)/)[1];
        let lvl = msg.substr(0, 1);
        let func = msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");
        return this.state.logLevels[lvl].visible && this.state.logLevels[lvl][func];
    }

    render() {
        return e("div", { key: "logContainer", className: "logContainer" },[
            this.renderControlPannel(),
            e("div", { key: "logLines", className: "loglines" }, this.state?.logLines ? this.state.logLines.filter(this.isLogVisible.bind(this)).map((logln,idx) => e(LogLine,{ key: idx, logln:logln})):null)            
        ]);
    }
}

class LogLine extends Component {
    isScrolledIntoView()
    {
        let rect = this.logdiv.getBoundingClientRect();
        let elemTop = rect.top;
        let elemBottom = rect.bottom;
    
        return (elemTop >= 0) && (elemBottom <= window.innerHeight);
    }

    render() {
        let msg = this.props.logln.match(/^[^IDVEW]*(.+)/)[1];
        let lvl = msg.substr(0, 1);
        let func = msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");
        let logLn = this.props.logln.substr(this.props.logln.indexOf(func) + func.length + 2).replaceAll(/^[\r\n]*/g, "").replaceAll(/[^0-9a-zA-Z ]\[..\n.*/g, "");

        return e("div", { key: "logLine" , className: `log LOG${lvl}` }, [
            e("div", { key: "level", ref: ref => this.logdiv = ref, className: "LOGLEVEL" }, lvl),
            e("div", { key: "date", className: "LOGDATE" }, msg.substr(3, 12)),
            e("div", { key: "source", className: "LOGFUNCTION" }, func),
            e("div", { key: "message", className: "LOGLINE" }, logLn)
        ]);
    }
}
