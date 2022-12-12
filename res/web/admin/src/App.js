import React from 'react';
import './App.css';
import SideDrawer from './components/page/sidebar/SideBar';
import AppTheme from './components/page/theme/Theme';
import {
  Experimental_CssVarsProvider as CssVarsProvider
} from '@mui/material/styles';
import { Box } from '@mui/material';
import CssBaseline from '@mui/material/CssBaseline';

const finder = new WebSocket(process.env.REACT_APP_FINDER_SERVICE_WS);

function App() {
  
  const [clients, setClients] = React.useState(undefined);
  const [mode, setMode] = React.useState('dark');
  
  finder.onmessage = (evt) => {
    try {
        let msg = JSON.parse(evt.data);
        if (msg.clients) {
            setClients(msg.clients);
        }
    } catch (ex) {
        console.error(ex);
    }
  };

  const theme = AppTheme(mode);
  document.firstElementChild.attributes['data-mui-color-scheme'] && (document.firstElementChild.attributes['data-mui-color-scheme'].value = mode);
  return (
    <CssVarsProvider theme={theme}>
      <CssBaseline />
      <Box data-mui-color-scheme={mode}>
        {clients && <SideDrawer theme={theme} clients={clients} mode={mode} setMode={setMode}></SideDrawer>}
      </Box>
    </CssVarsProvider>
  );
}

export default App;
