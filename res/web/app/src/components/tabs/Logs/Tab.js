import {createElement as e, Component} from 'react';
import { FormControlLabel, Checkbox, Button } from '@mui/material';
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

        var recIdx=-1;
        var msg = logln.match(/^[^IDVEW]*(.+)/)[1];
        var lvl = msg.substr(0, 1);
        var ts = msg.substr(3, 12);
        var func = msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");

        if (this.state.logLines.some((clogln,idx) => {
            var nmsg = clogln.match(/^[^IDVEW]*(.+)/)[1];
            var nlvl = nmsg.substr(0, 1);
            var nts = nmsg.substr(3, 12);
            var nfunc = msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");
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
        return e(FormControlLabel,{
            key:"ffiltered" + lvl + func,
            className:"effiltered",
            label: `${func} (${logLines.filter(logln => logln.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1") === func).length})`,
            control:e(Checkbox, {
                key: "ctrl",
                checked: this.state.logLevels[lvl][func],
                onChange: event => {this.state.logLevels[lvl][func] = event.target.checked; this.setState(this.state);}
            })});
    }

    renderLogLevelFilterControl(lvl,logLines) {   
        return e(FormControlLabel,{
            key:"lfiltered" + lvl,
            className:"elfiltered",
            label: `Log Level ${lvl}(${logLines.length})`,
            control:e(Checkbox, {
                key: "ctrl",
                checked: this.state.logLevels[lvl].visible,
                onChange: event => {this.state.logLevels[lvl].visible = event.target.checked; this.setState(this.state);}
            })});
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
            e("div", { key: "filterPannelTitle",className: "filterPannelTitle" }, "Filters"),
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
        var msg = logLine.match(/^[^IDVEW]*(.+)/)[1];
        var lvl = msg.substr(0, 1);
        var func = msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");
        if (this.state.logLevels[lvl].visible && this.state.logLevels[lvl][func]) {
            return true;
        }
        return false;
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
        var rect = this.logdiv.getBoundingClientRect();
        var elemTop = rect.top;
        var elemBottom = rect.bottom;
    
        return (elemTop >= 0) && (elemBottom <= window.innerHeight);
    }

    render() {
        var msg = this.props.logln.match(/^[^IDVEW]*(.+)/)[1];
        var lvl = msg.substr(0, 1);
        var func = msg.match(/.*\) ([^:]*)/g)[0].replaceAll(/^.*\) (.*)/g, "$1");
        var logLn = this.props.logln.substr(this.props.logln.indexOf(func) + func.length + 2).replaceAll(/^[\r\n]*/g, "").replaceAll(/[^0-9a-zA-Z ]\[..\n.*/g, "");

        return e("div", { key: "logLine" , className: `log LOG${lvl}` }, [
            e("div", { key: "level", ref: ref => this.logdiv = ref, className: "LOGLEVEL" }, lvl),
            e("div", { key: "date", className: "LOGDATE" }, msg.substr(3, 12)),
            e("div", { key: "source", className: "LOGFUNCTION" }, func),
            e("div", { key: "message", className: "LOGLINE" }, logLn)
        ]);
    }
}
