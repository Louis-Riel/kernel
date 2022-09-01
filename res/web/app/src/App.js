import logo from './logo.svg';
import './App.css';

import {useState} from 'react';
import PropTypes from 'prop-types';
import Tabs from '@mui/material/Tabs';
import Tab from '@mui/material/Tab';
import Typography from '@mui/material/Typography';
import Box from '@mui/material/Box';
import StorageViewer from "./components/tabs/Storage/Tab";
import StatusPage from './components/tabs/State/Tab';
import ConfigPage from './components/tabs/Config/Tab';
import WebSocketManager from './components/controls/WebSocket/WebSocketManager';
import LogLines from './components/tabs/Logs/Tab';
import EventsPage from './components/tabs/Events/Tab';

function TabPanel(props) {
  const { children, value, index, ...other } = props;

  return (
    <div
      role="tabpanel"
      hidden={value !== index}
      id={`simple-tabpanel-${index}`}
      aria-labelledby={`simple-tab-${index}`}
      {...other}
    >
      {value === index && (
        <Box sx={{ p: 3 }}>
          <Typography>{children}</Typography>
        </Box>
      )}
    </div>
  );
}

TabPanel.propTypes = {
  children: PropTypes.node,
  index: PropTypes.number.isRequired,
  value: PropTypes.number.isRequired,
};

function a11yProps(index) {
  return {
    id: `simple-tab-${index}`,
    'aria-controls': `simple-tabpanel-${index}`,
  };
}

export default function BasicTabs() {
  const [value, setValue] = useState(0);
  const [path, setPath] = useState(0);
  const [stateCBFns, updateStateCallbacks] = useState([]);
  const [logCBFns, updateLogCallbacks] = useState([]);
  const [eventCBFns, updateEventCallbacks] = useState([]);

  const handleChange = (event, newValue) => {
    setValue(newValue);
  };

  return (
    <Box class="App">
      <WebSocketManager
        stateCBFns={stateCBFns}
        logCBFns={logCBFns}
        eventCBFns={eventCBFns}
      ></WebSocketManager>
      <Box sx={{ borderBottom: 1, borderColor: 'divider' }}>
        <Tabs value={value} onChange={handleChange} aria-label="basic tabs example">
          <Tab label="Storage" {...a11yProps(0)} />
          <Tab label="Status" {...a11yProps(1)} />
          <Tab label="Config" {...a11yProps(2)} />
          <Tab label="Logs" {...a11yProps(3)} />
          <Tab label="Events" {...a11yProps(4)} />
        </Tabs>
      </Box>
      <div 
        id="Storage"
        className={`pageContent ${value===0?"":"hidden"}`}>
        <StorageViewer 
          path={path || "/"}
          onChangeFolder={(folder)=>setPath(folder)}
        ></StorageViewer>
      </div>
      <div 
        id="Status"
        className={`pageContent ${value===1?"":"hidden"}`}>
        <StatusPage 
          selectedDeviceId="current"
          hasLoaded={value===1}
          registerEventInstanceCallback={registerEventInstanceCallback}
          registerStateCallback={registerStateCallback}
        ></StatusPage>
      </div>
      <div 
        id="Config"
        className={`pageContent ${value===2?"":"hidden"}`}>
        <ConfigPage 
          onChangeFolder={(folder)=>setPath(folder)}
          hasLoaded={value===2}
        ></ConfigPage>
      </div>
      <div 
        id="Logs"
        className={`pageContent ${value===3?"":"hidden"}`}>
        <LogLines 
          registerLogCallback={registerLogCallback}
        ></LogLines>
      </div>
      <div 
        id="Events"
        className={`pageContent ${value===4?"":"hidden"}`}>
        <EventsPage 
          registerEventCallback={registerEventInstanceCallback}
        ></EventsPage>
      </div>
    </Box>
  );

  function registerStateCallback(stateCBFn) {
    if (!stateCBFns.find(fn => fn.name == stateCBFn.name))
      updateStateCallbacks(cur => [...cur,stateCBFn]);
  }
  
  function registerLogCallback(logCBFn) {
    if (!logCBFns.find(fn => fn.name == logCBFn.name))
      updateLogCallbacks(cur=>[...cur,logCBFn]);
  }
  
  function registerEventInstanceCallback(eventCBFn,instance) {
    if (!eventCBFns.find(fn => fn.fn.name == eventCBFn.name && fn.instance === instance)) {
      updateEventCallbacks(cur=>[...cur, {fn:eventCBFn,instance:instance}]);
    }
  }  
}

//export default App;
