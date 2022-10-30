import './Programs.css';

import {useState, lazy, Suspense} from 'react';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { Collapse,List,ListItem,ListItemButton,ListItemIcon,ListItemText, MenuItem, Select, TextField } from '@mui/material';
import { faChevronDown, faChevronUp, faComputer, faSquarePen, faTasks } from '@fortawesome/free-solid-svg-icons';
import { methods } from '../../../config/config'

export default function Programs(params) {
    const [programs, setPrograms] = useState(params.programs || []);
    const [state, setState] = useState(false);

    return  <List className='pgms'>
        <ListItem className='pgm'>
            <ListItemButton onClick={_ => setState(!state) }>
                <ListItemIcon>
                    <FontAwesomeIcon icon={faComputer}></FontAwesomeIcon>
                </ListItemIcon>
                <ListItemText primary={"Programs"} />
                {(state || false) === false ? <FontAwesomeIcon icon={faChevronUp}></FontAwesomeIcon> :
                                            <FontAwesomeIcon icon={faChevronDown}></FontAwesomeIcon>}
            </ListItemButton>
            <Collapse in={state} timeout="auto">
                <List className='programs'>
                    {programs.map(program => <Program program={program} config={params}></Program>)}
                </List>
            </Collapse>
        </ListItem>
    </List>;
}

export function Program(props) {
    const [program, setProgram] = useState(props.program);
    const [state, setState] = useState(false);

    if (program === undefined) {
        return undefined;
    }

    return <ListItem className='program'>
        <ListItemButton onClick={_ => setState(!state) }>
            <ListItemIcon>
                <FontAwesomeIcon icon={faComputer}></FontAwesomeIcon>
            </ListItemIcon>
            <ListItemText primary={program.name} />
            {(state || false) === false ? <FontAwesomeIcon icon={faChevronUp}></FontAwesomeIcon> :
                                        <FontAwesomeIcon icon={faChevronDown}></FontAwesomeIcon>}
        </ListItemButton>
        <Collapse in={state} timeout="auto">
                <InlineThreads inLineThreads={program.inLineThreads} config={props.config}></InlineThreads>
        </Collapse>
    </ListItem>;
}

export function InlineThreads(props) {
    const [threads,setThreads] = useState(props.inLineThreads);

    if (threads === undefined) {
        return undefined;
    }

    return <List className="inLineThreads">
        {threads.map(thread => <InLineThread inLineThread={thread} config={props.config}></InLineThread>)}
    </List>;
}

export function InLineThread(props) {
    const [thread,setThread] = useState(props.inLineThread);
    const [state, setState] = useState(false);

    if (thread === undefined) {
        return undefined;
    }

    return <ListItem className='inLineThread'>
        <ListItemButton onClick={_ => setState(!state) }>
        <ListItemIcon>
                {thread.program ? <FontAwesomeIcon icon={faComputer}></FontAwesomeIcon> : <FontAwesomeIcon icon={faTasks}></FontAwesomeIcon>}
            </ListItemIcon>
            <ListItemText primary={thread.method ? getMethodSummary(thread) : getProgramSummary(thread)} />
            {(state || false) === false ? <FontAwesomeIcon icon={faChevronUp}></FontAwesomeIcon> :
                                        <FontAwesomeIcon icon={faChevronDown}></FontAwesomeIcon>}
        </ListItemButton>
        <Collapse in={state} timeout="auto">
            {thread.program ? 
                Program(thread, props, setThread):
                Method(thread, props, setThread)
            }
        </Collapse>
    </ListItem>;
}

function getMethodSummary(thread) {
    return <div className='method-summary'>
        <div className='name'>{thread.method}</div>
        {thread.params && Object.keys(thread.params).length > 0 ? <div className='detail'>With {Object.keys(thread.params).length} Parameters</div> : undefined}
    </div>
}

function getProgramSummary(thread) {
    return <h3>{thread.program}</h3>
}

function getProgram(thread, props, setThread) {
    return <div className='thread-program'>
        <Select
            value={thread.program}
            onChange={(event) => { props.inLineThread.program = event.target.value; setThread({ ...props.inLineThread }); } }>
            {props.config.programs.map(prog => <MenuItem value={prog.name}>{prog.name}</MenuItem>)}
        </Select>
    </div>;
}

export function Method(thread, props, setThread) {
    return <div className='thread-method'>
        <Select
            value={thread.method}
            onChange={(event) => { props.inLineThread.method = event.target.value; setThread({ ...props.inLineThread }); } }>
            {methods.map(prog => <MenuItem value={prog}>{prog}</MenuItem>)}
        </Select>
        {thread.method && thread.params && Object.keys(thread.params).length ?
        <List className='method-args'>
            {Object.entries(thread.params).map(param => <MethodArg param={param} thread={thread} props={props} setThread={setThread}></MethodArg>)}
        </List>:undefined}
    </div>;
}

export function MethodArg(params) {
    const param = params.param;
    const thread = params.thread;
    const props = params.props;
    const setThread = params.setThread;
    const [state, setState] = useState(false);
    return [<ListItemButton onClick={_ => setState(!state) }>
        <ListItemIcon>
            <FontAwesomeIcon icon={faSquarePen}></FontAwesomeIcon>
        </ListItemIcon>
        <ListItemText primary={<div className="param-entry"><div className='param'>{param[0]}</div><div className="value">{param[1]}</div></div>} />
        {(state || false) === false ? <FontAwesomeIcon icon={faChevronUp}></FontAwesomeIcon> :
            <FontAwesomeIcon icon={faChevronDown}></FontAwesomeIcon>}
    </ListItemButton>,
    <Collapse in={state} timeout="auto">
        <TextField value={param[1]} label={param[0]} type={getType(param)} onChange={event =>{ thread.params[param[0]]=event.target.value; setThread({ ...props.inLineThread });} } ></TextField>
    </Collapse>];
}

function getType(param) {
    if ((param[0].toUpperCase() === "PINNO") || !isNaN(param[1])) {
        return "number";
    }
    return "text";
}

