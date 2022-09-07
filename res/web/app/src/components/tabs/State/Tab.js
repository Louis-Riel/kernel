import { Component, Suspense, lazy } from 'react';
import {wfetch, fromVersionedToPlain } from '../../../utils/utils';
import { Button, FormControl, InputLabel, Select, MenuItem, List, ListItemButton, ListItemIcon, ListItemText, Collapse  } from '@mui/material';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faSpinner } from '@fortawesome/free-solid-svg-icons/faSpinner'
import { faGears } from '@fortawesome/free-solid-svg-icons/faGears'
import { faChevronDown } from '@fortawesome/free-solid-svg-icons/faChevronDown'
import { faChevronUp } from '@fortawesome/free-solid-svg-icons/faChevronUp'
import { faMemory } from '@fortawesome/free-solid-svg-icons/faMemory'
import { faMicrochip } from '@fortawesome/free-solid-svg-icons/faMicrochip'
import { faRepeat } from '@fortawesome/free-solid-svg-icons/faRepeat'
import { faListCheck } from '@fortawesome/free-solid-svg-icons/faListCheck'
import { faSdCard } from '@fortawesome/free-solid-svg-icons/faSdCard'
import { faGear } from '@fortawesome/free-solid-svg-icons/faGear'
import { faWifi } from '@fortawesome/free-solid-svg-icons/faWifi'
import { faNetworkWired } from '@fortawesome/free-solid-svg-icons/faNetworkWired'
import { faComments } from '@fortawesome/free-solid-svg-icons/faComments'
import { faToggleOn } from '@fortawesome/free-solid-svg-icons/faToggleOn'
import { faGauge } from '@fortawesome/free-solid-svg-icons/faGauge'
import './State.css';

var httpPrefix = "";
const LocalJSONEditor = lazy(() => import('../../controls/JSONEditor/JSONEditor'));
const FirmwareUpdater = lazy(() => import('../../controls/FirmwareUpdater'));


export default class StatusPage extends Component {
    constructor(props) {
        super(props);
        this.state={
            refreshRate:"Manual",
            componentOpenState:{},
            status:{
                system:{
                    label: "System",
                    url: "/status/",
                    icon: faGears
                },
                components:{
                    label: "Components",
                    url: "/status/app",
                    icon: faListCheck
                },
                tasks:{
                    label: "Tasks",
                    url: "/status/tasks",
                    icon: faMicrochip
                },
                mallocs:{
                    label: "Memory Allocation",
                    url: "/status/mallocs",
                    icon: faMemory
                },
                repeating:{
                    label: "Repeating Tasks",
                    url: "/status/repeating_tasks",
                    icon: faRepeat
                },
                storage: {
                    label: "Storage",
                    icon: faSdCard
                }
            }
        }

        this.updateAppStatus();
        if (this.props.registerStateCallback) {
            this.props.registerStateCallback(this.refreshStatus.bind(this));
        }
    }

    getRefreshRate() {
        if (this.state.refreshRate.indexOf("secs")>0) {
            return Number(this.state.refreshRate.replace(/([0-9]+).*/,"$1"))*1000
        } 
        return Number(this.state.refreshRate.replace(/([0-9]+).*/,"$1"))*60000
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (this.state.refreshRate !== prevState.refreshRate) {
            if (this.refreshTimer) {
                clearInterval(this.refreshTimer);
            }

            if (this.state.refreshRate !== "Manual"){
                this.refreshTimer = setInterval(()=>{this.updateAppStatus()},this.getRefreshRate());
            }
        }
    }

    updateAppStatus() {
        this.updateStatuses(Object.keys(this.state.status)
                                  .filter(fld=>this.state.status[fld].url)
                                  .map(fld=>{return {
                                                        path: fld,
                                                        url: this.state.status[fld].url
                                                    }}), {});
    }

    refreshStatus(stat) {
        if (stat){
            this.filterProperties(stat).then(stat=>{
                const flds = Object.keys(stat);
                var status = this.state?.status || {};
                for (const fld in flds) {
                    status[flds[fld]] = stat[flds[fld]];    
                }
                this.setState({status:status});
            });
        } else {
            this.updateAppStatus();
        }
    }

    filterProperties(props) {
        return new Promise((resolve,reject)=>{
            resolve(Object.keys(props).filter(fld=> !fld.match(/^MALLOC_.*/) || props[fld]!==0).reduce((ret,fld)=>{ret[fld]=props[fld];return ret},{}));
        });
    }

    updateStatuses(requests, newState) {
        if (requests.length === 0) {
            return;
        }
        //var abort = new AbortController()
        //var timer = setTimeout(() => abort.abort(), 8000);
        if (this.props.selectedDeviceId === "current") {
            this.updateStatus(requests.pop(), undefined, newState).then(_res => {
                //(timer);
                document.getElementById("Status").style.opacity = 1;
                if (requests.length > 0) {
                    this.updateStatuses(requests, newState);
                } else {
                    this.setState({
                        error: null,
                        status: Object.keys(this.state.status)
                                      .reduce((pv,cv)=>{
                                        pv[cv]=this.state.status[cv]; 
                                        pv[cv].value= Array.isArray(newState[cv]?.value) ? 
                                                                        {[cv]:newState[cv].value} : 
                                                                        (newState[cv]?.value || pv[cv]?.value);
                                        return pv;},{})
                    });
                }
            }).catch(err => {
                document.getElementById("Status").style.opacity = 0.5
                //clearTimeout(timer);
                if (err.code !== 20) {
                    var errors = requests.filter(req => req.error);
                    document.getElementById("Status").style.opacity = 0.5
                    if (errors[0]?.waitFor) {
                        setTimeout(() => {
                            if (err.message !== "Failed to wfetch")
                                console.error(err);
                            this.updateStatuses(requests, newState);
                        }, errors[0].waitFor);
                    } else {
                        this.updateStatuses(requests, newState);
                    }
                }
            });
        } else if (this.props.selectedDeviceId) {
            wfetch(`${httpPrefix}/lfs/status/${this.props.selectedDeviceId}.json`, {
                method: 'get',
                //signal: abort.signal
            }).then(data => data.json()).then(fromVersionedToPlain)
                .then(status => this.setState({ status: this.orderResults(status) }))
        }
    }

    updateStatus(request, abort, newState) {   
        return new Promise((resolve, reject) => wfetch(`${httpPrefix}${request.url}`, {
            method: 'post',
            //signal: abort.signal
        }).then(data => data.json())
            .then(jstats => {
                if (request.url === "/status/app") {
                    this.state.status.storage.value=Object.keys(jstats)
                                                          .filter(fld=>["sdcard","lfs"].includes(fld))
                                                          .reduce((pv,cv)=>{pv[cv]=jstats[cv];return pv;},{});
                    jstats = Object.keys(jstats)
                                   .filter(fld=>!["sdcard","lfs"].includes(fld))
                                   .reduce((pv,cv)=>{pv[cv]=jstats[cv];return pv;},{});
                }
                (newState[request.path]=newState[request.path]||{}).value = jstats;
                resolve({ path: request.path, stat: jstats });
            }).catch(err => {
                request.retryCnt = (request.retryCnt | 0) + 1;
                request.waitFor = 1000;
                request.error = err;
                reject(err);
            }));
    }

    orderResults(res) {
        if (res){
            var ret = {};
            Object.keys(res).filter(fld => (typeof res[fld] !== 'object') && !Array.isArray(res[fld])).sort((a, b) => a.localeCompare(b)).forEach(fld => ret[fld] = res[fld]);
            Object.keys(res).filter(fld => (typeof res[fld] === 'object') && !Array.isArray(res[fld])).sort((a, b) => a.localeCompare(b)).forEach(fld => ret[fld] = res[fld]);
            Object.keys(res).filter(fld => Array.isArray(res[fld])).forEach(fld => ret[fld] = res[fld]);
            return ret;
        }
        return res;
    }

    SendCommand(body) {
        return wfetch(`${httpPrefix}/status/cmd`, {
            method: 'PUT',
            body: JSON.stringify(body)
        }).then(res => res.text().then(console.log))
          .catch(console.error)
          .catch(console.error);
    }

    render() {
        return <div>
                <div className="buttonbar">
                    <Button onClick={elem => this.updateAppStatus() }>Refresh</Button>
                    <Button onClick={elem => this.SendCommand({ 'command': 'reboot' }) }>Reboot</Button>
                    <Button onClick={elem => this.SendCommand({ 'command': 'factoryReset' }) }>Factory Reset</Button>
                    <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}><FirmwareUpdater></FirmwareUpdater></Suspense>
                    <FormControl>
                        <InputLabel
                            key="label"
                            className="label"
                            id="stat-refresh-label">Refresh Rate</InputLabel>
                        <Select
                            key="options"
                            id= "stat-refresh"
                            labelId="stat-refresh-label"
                            label= "Refresh Rate"
                            value= {this.state.refreshRate}
                            onChange= {elem => this.setState({refreshRate:elem.target.value})}>
                         { ["Manual", 
                            "2 secs", 
                            "5 secs", 
                            "10 secs",
                            "30 secs",
                            "1 mins",
                            "5 mins",
                            "10 mins",
                            "30 mins",
                            "60 mins"].map((term,idx)=><MenuItem value={term}>{term}</MenuItem>)}
                        </Select>
                    </FormControl>
                </div>
                {this.renderEditors()}
            </div>
    }

    renderEditors() {
        return <List 
            component="nav"
            aria-labelledby="nested-list-subheader">
                {
                    Object.keys(this.state.status).sort((a,b)=>a.localeCompare(b)).map(fld => this.renderEditorGroup(fld))
                }
            </List>
    }

    renderEditorGroup(fld) {
        if (fld === "components" && this.state.status[fld].value) {
            return this.renderComponents(fld);
        } else {
            return this.renderGroup(fld);
        }
    }

    renderGroup(fld) {
        return <div>
            <ListItemButton onClick={_ => {
                this.state.status[fld] || (this.state.status[fld] = {});
                this.state.status[fld].opened = !this.state.status[fld]?.opened;
                this.setState({ status: this.state.status });
            } }>
                <ListItemIcon>
                    <FontAwesomeIcon icon={this.state.status[fld].icon}></FontAwesomeIcon>
                </ListItemIcon>
                <ListItemText primary={this.state.status[fld].label} />
                {this.state.status[fld]?.opened ? <FontAwesomeIcon icon={faChevronUp}></FontAwesomeIcon> :
                    <FontAwesomeIcon icon={faChevronDown}></FontAwesomeIcon>}
            </ListItemButton>
            <Collapse in={this.state.status[fld]?.opened} timeout="auto">
                <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
                    <LocalJSONEditor
                        path='/'
                        json={this.orderResults(this.state.status[fld].value)}
                        editable={false}
                        sortable={true}
                        selectedDeviceId={this.props.selectedDeviceId}
                        registerStateCallback={this.props.registerStateCallback}
                        registerEventInstanceCallback={this.props.registerEventInstanceCallback}>
                    </LocalJSONEditor>
                </Suspense>
            </Collapse>
        </div>;
    }

    getComponentIcon(name) {
        switch (name) {
            case "wifi":
                return faWifi;
            case "Rest":
                return faNetworkWired;
            case "WebsocketManager":
                return faComments;
            case "AnalogPin":
                return faGauge;
            case "DigitalPin":
                return faToggleOn;
            default:
                break;
        }
        return faGear
    }

    renderComponents(fld) {
        return <div>
            <ListItemButton onClick={_ => {
                this.state.status[fld] || (this.state.status[fld] = {});
                this.state.status[fld].opened = !this.state.status[fld]?.opened;
                this.setState({ status: this.state.status });
            } }>
                <ListItemIcon>
                    <FontAwesomeIcon icon={this.state.status[fld].icon}></FontAwesomeIcon>
                </ListItemIcon>
                <ListItemText primary={this.state.status[fld].label} />
                {this.state.status[fld]?.opened ? <FontAwesomeIcon icon={faChevronUp}></FontAwesomeIcon> :
                    <FontAwesomeIcon icon={faChevronDown}></FontAwesomeIcon>}
            </ListItemButton>
            <Collapse in={this.state.status[fld]?.opened} timeout="auto">
                {Object.entries(
                    Object.entries(this.state.status[fld].value)
                        .reduce((pv, cv) => {
                            if (cv[1]?.class) {
                                if (!pv[cv[1].class]) {
                                    pv[cv[1]?.class] = [];
                                }
                                pv[cv[1].class].push(cv[1]);
                            }
                            return pv;
                        }, {}))
                    .map(entry => {
                        return <div className='subitem'>
                            <ListItemButton onClick={_ => {
                                this.state.componentOpenState[entry[0]] || (this.state.componentOpenState[entry[0]] = false);
                                this.state.componentOpenState[entry[0]] = !this.state.componentOpenState[entry[0]];
                                this.setState({ componentOpenState: this.state.componentOpenState });
                            } }>
                                <ListItemIcon>
                                    <FontAwesomeIcon icon={this.getComponentIcon(entry[0])}></FontAwesomeIcon>
                                </ListItemIcon>
                                <ListItemText primary={entry[0]} />
                                {this.state.componentOpenState[entry[0]] ? <FontAwesomeIcon icon={faChevronUp}></FontAwesomeIcon> :
                                    <FontAwesomeIcon icon={faChevronDown}></FontAwesomeIcon>}
                            </ListItemButton>
                            <Collapse in={this.state.componentOpenState[entry[0]]} timeout="auto">
                                <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
                                    <LocalJSONEditor
                                        path='/'
                                        json={entry[1]?.reduce((pv, cv) => { pv[cv.name] = cv; return pv; }, {})}
                                        editable={false}
                                        sortable={true}
                                        selectedDeviceId={this.props.selectedDeviceId}
                                        registerStateCallback={this.props.registerStateCallback}
                                        registerEventInstanceCallback={this.props.registerEventInstanceCallback}>
                                    </LocalJSONEditor>
                                </Suspense>
                            </Collapse>
                        </div>;
                    })}
            </Collapse>
        </div>;
    }
}
