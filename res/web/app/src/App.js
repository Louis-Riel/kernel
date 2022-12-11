import './App.css';

import {useState, lazy, Suspense} from 'react';
import Tabs from '@mui/material/Tabs';
import Tab from '@mui/material/Tab';
import Box from '@mui/material/Box';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faSpinner } from '@fortawesome/free-solid-svg-icons/faSpinner';
import { setSelectedDevice } from './utils/utils';

const StorageViewer = lazy(() => import('./components/tabs/Storage/Tab'));
const StatusPage = lazy(() => import('./components/tabs/State/Tab'));
const ConfigPage = lazy(() => import('./components/tabs/Config/Tab'));
const WebSocketManager = lazy(() => import('./components/controls/WebSocket/WebSocketManager'));
const LogLines = lazy(() => import('./components/tabs/Logs/Tab'));
const EventsPage = lazy(() => import('./components/tabs/Events/Tab'));
const DeviceList = lazy(() => import('./components/controls/DeviceList/DeviceList'));

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
  const [selectedDevice, updateSelectedDevice] = useState({});
  const [tabStates, updateTabStates ] = useState([
    {name:"Storage"},
    {name:"Status"},
    {name:"Config"},
    {name:"Logs"},
    {name:"Events"}
  ]);

  const tabChange = (event, newValue) => {
    setValue(newValue);
    updateTabStates(tabStates.map((tabState,idx) => {return {...tabState, opened:idx!==newValue}}));
  };

  return (
    <Box class="App">
      <div className="control-bar">
        {selectedDevice.config ? 
        <Box sx={{ borderBottom: 1, borderColor: 'divider' }}>
          <Tabs value={value} onChange={tabChange} aria-label="The tabs">
            { tabStates.map((tab,idx)=><Tab label={tab.name} {...a11yProps(idx)}></Tab>)}
          </Tabs>
        </Box>:null}
        <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
          <DeviceList
              selectedDevice= {selectedDevice}
              onSet={dev=>{
                updateSelectedDevice(dev);
                setSelectedDevice(dev);
              }}></DeviceList>
        </Suspense>
        {selectedDevice.config ? 
        <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
          <WebSocketManager
            enabled={window.location.pathname.indexOf("sdcard") === -1}
            selectedDevice= {selectedDevice}
            stateCBFns={stateCBFns}
            logCBFns={logCBFns}
            eventCBFns={eventCBFns}
          ></WebSocketManager>
        </Suspense>:null}
      </div>
      {selectedDevice.config?tabStates.map((tab,idx)=>getTabContent(idx)):null}
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
            selectedDevice={selectedDevice}
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
            selectedDevice={selectedDevice}
            className={`pageContent ${value === 1 ? "" : "hidden"}`}
            registerEventInstanceCallback={registerEventInstanceCallback}
            registerStateCallback={registerStateCallback}
            unRegisterEventInstanceCallback={unRegisterEventInstanceCallback}
            unRegisterStateCallback={unRegisterStateCallback}
          ></StatusPage>
        </Suspense>
        </div>
      case 2: return <div 
        id="Config"
        className={`pageContent ${value===2?"":"hidden"}`}>
        <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
          <ConfigPage 
            selectedDevice={selectedDevice}
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
            unRegisterLogCallback={unRegisterLogCallback}
          ></LogLines>
        </Suspense>
      </div>
      case 4:  return <div 
        id="Events"
        className={`pageContent ${value===4?"":"hidden"}`}>
        <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
          <EventsPage 
            selectedDevice={selectedDevice}
            registerEventCallback={registerEventInstanceCallback}
            unRegisterEventCallback={unRegisterEventInstanceCallback}
          ></EventsPage>
        </Suspense>
      </div>
    
      default:
        break;
    }
  }

  function getTabContent(tabNo) {
    return (tabStates[tabNo]?.loaded || (window.location.pathname.indexOf("sdcard") === -1)) || (tabNo===value) ? buildTabContent(tabNo) : undefined;
  }

  function registerStateCallback(stateCBFn) {
    if (!stateCBFns.find(fn => fn.name === stateCBFn.name))
      updateStateCallbacks(cur => [...cur,stateCBFn]);
  }

  function unRegisterStateCallback(stateCBFn) {
    updateStateCallbacks(stateCBFns.filter(fn => fn.name !== stateCBFn.name));
  }

  function registerLogCallback(logCBFn) {
    if (!logCBFns.find(fn => fn.name === logCBFn.name))
      updateLogCallbacks(cur=>[...cur,logCBFn]);
  }

  function unRegisterLogCallback(logCBFn) {
    updateLogCallbacks(logCBFns.filter(fn => fn.name !== logCBFn.name));
  }

  function registerEventInstanceCallback(eventCBFn,instance) {
    if (!eventCBFns.find(fn => fn.fn.name === eventCBFn.name && fn.instance === instance)) {
      updateEventCallbacks(cur=>[...cur, {fn:eventCBFn,instance:instance}]);
    }
  }  

  function unRegisterEventInstanceCallback(eventCBFn,instance) {
    updateEventCallbacks(eventCBFns.filter(fn => fn.fn.name !== eventCBFn.name && fn.instance !== instance));
  }  
}

