import logo from './logo.svg';
import './App.css';

import {useState, lazy, Suspense} from 'react';
import PropTypes from 'prop-types';
import Tabs from '@mui/material/Tabs';
import Tab from '@mui/material/Tab';
import Typography from '@mui/material/Typography';
import Box from '@mui/material/Box';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faSpinner } from '@fortawesome/free-solid-svg-icons/faSpinner';

const StorageViewer = lazy(() => import('./components/tabs/Storage/Tab'));
const StatusPage = lazy(() => import('./components/tabs/State/Tab'));
const ConfigPage = lazy(() => import('./components/tabs/Config/Tab'));
const WebSocketManager = lazy(() => import('./components/controls/WebSocket/WebSocketManager'));
const LogLines = lazy(() => import('./components/tabs/Logs/Tab'));
const EventsPage = lazy(() => import('./components/tabs/Events/Tab'));

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
  const [tabStates, updateTabStates ] = useState([
    {name: "Storage"},
    {name:"Status"},
    {name:"Config"},
    {name:"Logs"},
    {name:"Events"}
  ]);

  const handleChange = (event, newValue) => {
    setValue(newValue);
    tabStates.filter((tab,idx)=>idx!==newValue && tab.opened).forEach(tab=>tab.opened=false);
    tabStates[newValue].opened=true;
    tabStates[newValue].loaded=true;
    updateTabStates(tabStates);
  };

  return (
    <Box class="App">
      <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
        <WebSocketManager
          stateCBFns={stateCBFns}
          logCBFns={logCBFns}
          eventCBFns={eventCBFns}
        ></WebSocketManager>
      </Suspense>
      <Box sx={{ borderBottom: 1, borderColor: 'divider' }}>
        <Tabs value={value} onChange={handleChange} aria-label="The tabs">
          { tabStates.map((tab,idx)=><Tab label={tab.name} {...a11yProps(idx)}></Tab>)}
        </Tabs>
      </Box>
      {tabStates.map((tab,idx)=>getTabContent(idx))}
    </Box>
  );

  function buildTabContent(tabNo) {
    tabStates[tabNo].loaded=true;
    switch (tabNo) {
      case 0: return <div 
        id="Storage"
        className={`pageContent ${value===0?"":"hidden"}`}>
        <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
          <StorageViewer 
            path={path || "/"}
            onChangeFolder={(folder)=>setPath(folder)}
          ></StorageViewer>
        </Suspense>
      </div>
      case 1: return <div
        id="Status"
        className={`pageContent ${value === 1 ? "" : "hidden"}`}>
        <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
          <StatusPage
            selectedDeviceId="current"
            className={`pageContent ${value === 1 ? "" : "hidden"}`}
            registerEventInstanceCallback={registerEventInstanceCallback}
            registerStateCallback={registerStateCallback}
          ></StatusPage>
        </Suspense>
        </div>
      case 2: return <div 
        id="Config"
        className={`pageContent ${value===2?"":"hidden"}`}>
        <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
          <ConfigPage 
            onChangeFolder={(folder)=>setPath(folder)}
            hasLoaded={value===2}
          ></ConfigPage>
        </Suspense>
      </div>
      case 3: return <div 
        id="Logs"
        className={`pageContent ${value===3?"":"hidden"}`}>
        <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
          <LogLines 
            registerLogCallback={registerLogCallback}
          ></LogLines>
        </Suspense>
      </div>
      case 4:  return <div 
        id="Events"
        className={`pageContent ${value===4?"":"hidden"}`}>
        <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
          <EventsPage 
            registerEventCallback={registerEventInstanceCallback}
          ></EventsPage>
        </Suspense>
      </div>
    
      default:
        break;
    }
  }

  function getTabContent(tabNo) {
    return tabStates[tabNo]?.loaded || (tabNo===value) ? buildTabContent(tabNo) : undefined;
  }

  function registerStateCallback(stateCBFn) {
    if (!stateCBFns.find(fn => fn.name === stateCBFn.name))
      updateStateCallbacks(cur => [...cur,stateCBFn]);
  }
  
  function registerLogCallback(logCBFn) {
    if (!logCBFns.find(fn => fn.name === logCBFn.name))
      updateLogCallbacks(cur=>[...cur,logCBFn]);
  }
  
  function registerEventInstanceCallback(eventCBFn,instance) {
    if (!eventCBFns.find(fn => fn.fn.name === eventCBFn.name && fn.instance === instance)) {
      updateEventCallbacks(cur=>[...cur, {fn:eventCBFn,instance:instance}]);
    }
  }  
}

//export default App;
