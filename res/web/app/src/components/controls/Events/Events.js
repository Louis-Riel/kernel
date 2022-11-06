import './Events.css';

import { useState } from 'react';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { Collapse,List,ListItem,ListItemButton,ListItemIcon,ListItemText,TextField,Select,MenuItem,FormControl,InputLabel,ToggleButtonGroup,ToggleButton } from '@mui/material';
import { faChevronDown, faChevronUp, faJedi, faMagicWandSparkles } from '@fortawesome/free-solid-svg-icons';
import { methods } from '../../../config/config'

export default function Events(params) {
    const [events, setEvents] = useState(params.events || []);
    const [state, setState] = useState(false);

    return <List className='pgms'>
        <ListItem className='pgm'>
            <ListItemButton onClick={_ => setState(!state) }>
                <ListItemIcon>
                    <FontAwesomeIcon icon={faJedi}></FontAwesomeIcon>
                </ListItemIcon>
                <ListItemText primary={"Events"} />
                {(state || false) === false ? <FontAwesomeIcon icon={faChevronUp}></FontAwesomeIcon> :
                                            <FontAwesomeIcon icon={faChevronDown}></FontAwesomeIcon>}
            </ListItemButton>
            <Collapse in={state} timeout="auto">
                {events.map(event => <Event event={event} config={params} programs={params.programs}></Event>)}
            </Collapse>
        </ListItem>
    </List>;
}

export function Event(props) {
    const [event, setEvent] = useState(props.event);
    const [state, setState] = useState(false);
    const [isProgram , setIsProgram ] = useState(props.event.program !== undefined)

    return <List className='pgms'>
        <ListItem className='pgm'>
            <ListItemButton onClick={_ => setState(!state) }>
                <ListItemIcon>
                    <FontAwesomeIcon icon={faMagicWandSparkles}></FontAwesomeIcon>
                </ListItemIcon>
                <ListItemText primary={<div className="event-header"><div className="event-base">{event.eventBase}</div><div className="event-id">{event.eventId}</div></div>} />
                {(state || false) === false ? <FontAwesomeIcon icon={faChevronUp}></FontAwesomeIcon> :
                                            <FontAwesomeIcon icon={faChevronDown}></FontAwesomeIcon>}
            </ListItemButton>
            <Collapse className="event" in={state} timeout="auto">
                <div className="event-name">
                    <TextField value={event.eventBase} label="Event Base" type="text" onChange={evt =>{ props.event.eventBase=evt.target.value; setEvent({ ...props.event });} } ></TextField>
                    <TextField value={event.eventId} label="Event Id" type="text" onChange={evt =>{ props.event.eventId=evt.target.value; setEvent({ ...props.event });} } ></TextField>
                </div>
                <ToggleButtonGroup
                    value={isProgram?"program":"method"}
                    exclusive
                    onChange={evt => {
                        setIsProgram(evt.target.value === "program");
                        if (evt.target.value === "program") {
                            props.event.method = undefined;
                            props.event.params = undefined;
                        } else {
                            props.event.program = undefined;
                        }
                        setEvent(props.event);
                    } }
                    aria-label="Run"
                    >
                    <ToggleButton value="program">Program</ToggleButton>
                    <ToggleButton value="method">Method</ToggleButton>
                </ToggleButtonGroup>
                {isProgram ? <FormControl>
                    <InputLabel
                        key="label"
                        className="label"
                        id="stat-refresh-label">Program</InputLabel>
                    <Select
                        label="Program"
                        value={event.program}
                        onChange={event => { props.event.program = event.target.value; setEvent({ ...props.event }); } }>
                        {props.programs.map(prog => <MenuItem value={prog.name}>{prog.name}</MenuItem>)}
                    </Select>
                </FormControl> : <FormControl>
                    <InputLabel
                        key="label"
                        className="label"
                        id="stat-refresh-label">Method</InputLabel>
                    <Select
                        label="Method"
                        value={event.method}
                        onChange={event => { props.event.method = event.target.value; setEvent({ ...props.event }); } }>
                        {methods.map(method => <MenuItem value={method}>{method}</MenuItem>)}
                    </Select>
                </FormControl>}
            </Collapse>
        </ListItem>
    </List>;
}