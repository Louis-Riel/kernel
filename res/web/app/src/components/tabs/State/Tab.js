import { Component, Suspense, lazy } from 'react';
import { wfetch } from '../../../utils/utils';
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
import { Area, XAxis, YAxis, Tooltip, AreaChart, CartesianGrid } from 'recharts';

const LocalJSONEditor = lazy(() => import('../../controls/JSONEditor/JSONEditor'));
const FirmwareUpdater = lazy(() => import('../../controls/FirmwareUpdater/FirmwareUpdater'));


export default class StatusPage extends Component {
    constructor(props) {
        super(props);
        this.state={
            httpPrefix:this.props.selectedDevice?.ip ? `${process.env.REACT_APP_API_URI}/${this.props.selectedDevice.config.devName}` : ".",
            refreshRate:"Manual",
            componentOpenState:{},
            threads:[],
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
            return Number(this.state.refreshRate.replace(/(\d+).*/,"$1"))*1000
        } 
        return Number(this.state.refreshRate.replace(/(\d+).*/,"$1"))*60000
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

        if (prevProps?.selectedDevice !== this.props.selectedDevice) {
            if (this.props.selectedDevice?.ip) {
                this.setState({httpPrefix:`${process.env.REACT_APP_API_URI}/${this.props.selectedDevice.config.devName}`});
            } else {
                this.setState({httpPrefix:"."});
            }
        }
        if (prevState.httpPrefix !== this.state.httpPrefix) {
            this.updateAppStatus();
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
            this.setState({status:this.filterProperties(stat)});
        } else {
            this.updateAppStatus();
        }
    }

    filterProperties(props) {
        return Promise.resolve(Object.keys(props).filter(fld=> !fld.match(/^MALLOC_.*/) || props[fld]!==0).reduce((ret,fld)=>{ret[fld]=props[fld];return ret},{}));
    }

    updateStatuses(requests, newState) {
        if (requests.length === 0) {
            return;
        }
        this.updateStatus(requests.pop(), undefined, newState).then(res => {
            document.getElementById("Status").style.opacity = 1;
            if (res.path === "tasks") {
                this.setState(prevState => {return {
                    error: null,
                    threads: [...prevState.threads.filter((_val,idx,arr) => arr.length >= 200 ? idx > arr.length - 200 : true), 
                             res.stat.reduce((ret,thread) => {if (thread.Name !== "IDLE") {ret[thread.Name] = thread.Runtime;}; return ret}, {ts: new Date().getTime()})],
                    status: {
                        ...this.state.status,
                        [res.path]: {...this.state.status[res.path],value: {tasks:res.stat}}
                    }
                }});
            } else {
                this.setState({
                    error: null,
                    status: Object.keys(this.state.status)
                                .reduce((pv,cv)=>{
                                pv[cv]=this.state.status[cv]; 
                                pv[cv].value= Array.isArray(newState[cv]?.value) ? 
                                                                {[cv]:newState[cv].value} : 
                                                                (newState[cv]?.value || pv[cv]?.value);
                                return pv;},{})});
            }

            if (requests.length > 0) {
                this.updateStatuses(requests, newState);
            }
        }).catch(err => {
            document.getElementById("Status").style.opacity = 0.5
            //clearTimeout(timer);
            if (err.code !== 20) {
                let errors = requests.filter(req => req.error);
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
    }

    updateStatus(request, abort, newState) {   
        return new Promise((resolve, reject) => wfetch(`${this.state.httpPrefix}${request.url}`, {
            method: 'post',
            //signal: abort.signal
        }).then(data => data.json())
            .then(jstats => {
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
            let ret = {};
            Object.keys(res).filter(fld => (typeof res[fld] !== 'object') && !Array.isArray(res[fld])).sort((a, b) => a.localeCompare(b)).forEach(fld => ret[fld] = res[fld]);
            Object.keys(res).filter(fld => (typeof res[fld] === 'object') && !Array.isArray(res[fld])).sort((a, b) => a.localeCompare(b)).forEach(fld => ret[fld] = res[fld]);
            Object.keys(res).filter(fld => Array.isArray(res[fld])).forEach(fld => ret[fld] = res[fld]);
            return ret;
        }
        return res;
    }

    SendCommand(body) {
        return wfetch(`${this.state.httpPrefix}/status/cmd`, {
            method: 'PUT',
            body: JSON.stringify(body)
        }).then(res => res.text().then(console.log))
          .catch(console.error)
          .catch(console.error);
    }

    render() {
        return <div>
                {this.renderControlPannel()}
                {this.renderEditors()}
            </div>
    }

    renderControlPannel() {
        return <div className="buttonbar">
            <Button onClick={elem => this.updateAppStatus()}>Refresh</Button>
            <Button onClick={elem => this.SendCommand({ 'command': 'reboot' })}>Reboot</Button>
            <Button onClick={elem => this.SendCommand({ 'command': 'scanNetwork' })}>Scan Network</Button>
            <Button onClick={elem => this.SendCommand({ 'command': 'factoryReset' })}>Factory Reset</Button>
            <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
                <FirmwareUpdater selectedDevice={this.props?.selectedDevice}></FirmwareUpdater>
            </Suspense>
            <FormControl>
                <InputLabel
                    key="label"
                    className="label"
                    id="stat-refresh-label">Refresh Rate</InputLabel>
                <Select
                    key="options"
                    id="stat-refresh"
                    labelId="stat-refresh-label"
                    label="Refresh Rate"
                    value={this.state?.refreshRate}
                    onChange={elem => {this.setState({ refreshRate: elem.target.value });this.forceUpdate();}}>
                    {["Manual",
                        "2 secs",
                        "5 secs",
                        "10 secs",
                        "30 secs",
                        "1 mins",
                        "5 mins",
                        "10 mins",
                        "30 mins",
                        "60 mins"].map((term, idx) => <MenuItem value={term}>{term}</MenuItem>)}
                </Select>
            </FormControl>
        </div>;
    }

    renderEditors() {
        return <List 
            component="nav"
            aria-labelledby="nested-list-subheader">
                {
                    Object.keys(this.state.status).sort((a,b)=>a.localeCompare(b))
                                                  .map(fld => this.renderEditorGroup(fld))
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
                this.setState(prevValue => ({...prevValue,
                                              status: {...prevValue.status, 
                                                       [fld]:{...prevValue.status[fld],
                                                              opened: !prevValue.status[fld].opened}
                                                      }
                                            }));
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
                        selectedDevice={this.props.selectedDevice}
                        registerStateCallback={this.props.registerStateCallback}
                        registerEventInstanceCallback={this.props.registerEventInstanceCallback}
                        unRegisterStateCallback={this.props.unRegisterStateCallback}
                        unRegisterEventInstanceCallback={this.props.unRegisterEventInstanceCallback}>
                    </LocalJSONEditor>
                    { (fld === "tasks") && this.state.status[fld].value ? this.getDaveStyleTaskManager() : undefined
                    }
                </Suspense>
            </Collapse>
        </div>;
    }

    getTooltip(data) {
        return <div className="tooltip-content">
            <div className='tooltip-header'>{data.labelFormatter(data.label)}</div>
            <ul className='threads'>
                {data.payload
                     .sort((a,b) => a.value === b.value ? 0 : (a.value < b.value || -1))
                     .map(stat => <div className='thread'>
                    <div className='thread-name' style={{color:stat.fill === "black" ? "lightgreen" : stat.fill}}>{stat.name}</div>
                    <div className='thread-value'>{stat.value}<div className='thread-summary'>
                        ({(stat.value/data.payload.reduce((ret,stat) => ret + stat.value,0)*100).toFixed(2)}%)
                        </div></div>
                </div>)}
            </ul>
        </div>
    }

    getDaveStyleTaskManager() {
        return <AreaChart 
                data={this.state.threads}
                height={300}
                width={500}
                className="chart"
                stackOffset="expand">
                <CartesianGrid strokeDasharray="3 3"></CartesianGrid>
                <XAxis dataKey="ts"
                       name='Time'
                       color='black'
                       tickFormatter={unixTime => new Date(unixTime).toLocaleTimeString()}></XAxis>
                <YAxis hide={true}></YAxis>
                <Tooltip className="tooltip"
                         content={this.getTooltip.bind(this)}
                         labelFormatter={t => new Date(t).toLocaleString()}></Tooltip>
                {this.getAreaSummaries()}
            </AreaChart>;
    }

    /**
     * @param numOfSteps: Total number steps to get color, means total colors
     * @param step: The step number, means the order of the color
     */
    rainbow(numOfSteps, step, fieldName) {
        // This function generates vibrant, "evenly spaced" colours (i.e. no clustering). This is ideal for creating easily distinguishable vibrant markers in Google Maps and other apps.
        // Adam Cole, 2011-Sept-14
        // HSV to RBG adapted from: http://mjijackson.com/2008/02/rgb-to-hsl-and-rgb-to-hsv-color-model-conversion-algorithms-in-javascript
        if (fieldName === "IDLE") {
            return "black";
        }
        
        let r, g, b;
        let h = step / numOfSteps;
        let i = ~~(h * 6);
        let f = h * 6 - i;
        let q = 1 - f;
        // eslint-disable-next-line default-case
        switch(i % 6){
            case 0: r = 1; g = f; b = 0; break;
            case 1: r = q; g = 1; b = 0; break;
            case 2: r = 0; g = 1; b = f; break;
            case 3: r = 0; g = q; b = 1; break;
            case 4: r = f; g = 0; b = 1; break;
            case 5: r = 1; g = 0; b = q; break;
        }
        let c = "#" + ("00" + (~ ~(r * 255)).toString(16)).slice(-2) + ("00" + (~ ~(g * 255)).toString(16)).slice(-2) + ("00" + (~ ~(b * 255)).toString(16)).slice(-2);
        return (c);
    }

    getAreaSummaries() {
        return Object.keys(this.state.threads.reduce((ret,thread) => {return {...ret,...thread}}, {}))
                     .filter(line => line !== 'ts' && line !== 'IDLE')
                     .sort((a,b)=>this.state.threads[this.state.threads.length-1][a] === this.state.threads[this.state.threads.length-1][b] ? 0 : (this.state.threads[this.state.threads.length-1][a] > this.state.threads[this.state.threads.length-1][b] || -1) )
                     .map((line,idx,arr) => <Area
                                    type="monotone"
                                    fill={this.rainbow(arr.length,idx,line)}
                                    dataKey={line}
                                    stackId="1"></Area>);
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
                this.setState({ status: {
                    ...this.state.status,
                    [fld]:{...this.state.status[fld],opened:!this.state.status[fld]?.opened}
                }});
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
                                this.setState({
                                    componentOpenState:{
                                        ...this.state.componentOpenState,
                                        [entry[0]]: !this.state.componentOpenState[entry[0]]
                                    }
                                })
                            }}>
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
                                        selectedDevice={this.props.selectedDevice}
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
