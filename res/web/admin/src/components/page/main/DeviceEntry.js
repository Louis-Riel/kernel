import { Avatar, Box, Card, CardContent, CardHeader, TextField, Typography, CardActions } from '@mui/material';
import { styled } from '@mui/material/styles';
import Lock from '@mui/icons-material/Lock';
import LockOpen from '@mui/icons-material/LockOpen';
import Pending from '@mui/icons-material/Pending';
import VpnKey from '@mui/icons-material/VpnKey';
import VpnKeyOff from '@mui/icons-material/VpnKeyOff';
import Http from '@mui/icons-material/Http';
import ExpandMoreIcon from '@mui/icons-material/ExpandMore';
import forge from 'node-forge';
import * as React from 'react';
import './DeviceEntry.css';
import { grey } from '@mui/material/colors';
import IconButton from '@mui/material/IconButton';
import Collapse from '@mui/material/Collapse';
import Link from '@mui/material/Link';

const ExpandMore = styled((props) => {
    const { expand, ...other } = props;
    return <IconButton {...other} />;
  })(({ theme, expand }) => ({
    transform: !expand ? 'rotate(0deg)' : 'rotate(180deg)',
    marginLeft: 'auto',
    transition: theme.transitions.create('transform', {
      duration: theme.transitions.duration.shortest,
    }),
}));

export default function DeviceEntry(props) {
    const responseState = ["waiting","good","bad"];
    const [secureResponse, setSecureResponse] = React.useState(2);
    const [expanded, setExpanded] = React.useState(false);
    if (!props.client) {
        return undefined;
    }
    return GetDevice();

    function getStatusIcon(state,secure) {
        switch (state) {
            case 0:
                return <Pending className={responseState[state]}/>;
            case 1:
                return secure?<VpnKey className={responseState[state]}/>:<Http className={responseState[state]}/>;
            case 2:
                return secure?<VpnKeyOff className={responseState[state]}/>:<Http className={responseState[state]}/>;
            default:
                break;
        }
    }

    function EndPoint(ep) {
        if (!ep || !Object.keys(ep).length) {
            return undefined;
        }
        return <Box key={ep.path+ep.method}>
            {Object.entries(Object.values(ep).reduce((ret,ep)=>{
                if (ret[ep.path]){
                    ret[ep.path][ep.method] = ep;
                } else {
                    ret[ep.path]={[ep.method]: ep};
                }
                return ret;        
            },{})).map(path => {
                return <Box key={ep.path}>
                    <Typography variant='httppath'>{path[0]}</Typography>
                    {Object.entries(path[1]).map(method => <Box key={ep.method}>
                        <Typography paragraph>
                            <Typography variant='httpmethod' display='inline'>{method[0]}</Typography><Typography> {method[1].keys.length} keys refreshed {method[1].keys.reduce((ret,key)=>ret+key.NumRefreshes,0)} times, invoked {method[1].keys.reduce((ret,key)=>ret+key.NumValid+key.NumInvalid,0)} times with {method[1].keys.reduce((ret,key)=>ret+key.NumInvalid,0)} bad checksums and {method[1].NumInvalid} revoked calls.</Typography>
                        </Typography>
                    </Box>)}
                </Box>
            })}
        </Box>
    }

    function getSubtitle(client) {
        return <Typography variant='subtitle2'>{client.ip} {sumarizeKeys(client?.status?.Rest?.Keys)}</Typography>;
        function sumarizeKeys(keyentries) {
            if (keyentries) {
                const keys = Object.values(keyentries);
                return `(${keys.reduce((ret,key)=>ret+key.NumValid,0)}/${keys.reduce((ret,key)=>ret+key.NumInvalid,0)})`
            }
        }
    }

    function GetDevice() {
        if (!props.client?.config) {
            return undefined;
        }
        if (props.client.refresh) {
            props.client.refresh=false;
            refreshStatus(props.client?.config?.Rest?.KeyServer !== undefined);
        }

        const cert = props.client?.config?.Rest?.KeyServer?.serverCert ? forge.pki.certificateFromPem(props.client.config.Rest.KeyServer.serverCert) : undefined;
        return <Box key={props.client.config.deviceid} className='device secure'>
            <Card variant='outlined'>
                <CardHeader
                    avatar={<Avatar sx={{ bgcolor: grey[700]}}>{props.client.config?.Rest?.KeyServer ? <Lock /> : <LockOpen />}</Avatar>}
                    title={<Link href={`https://${process.env.REACT_APP_SECURE_BASE}:${process.env.REACT_APP_SECURE_PORT}/`}>{props.client.config.devName}</Link>}
                    subheader={<Box key="subtitle"><Typography variant='subtitle1'>{props.client.config.deviceid}</Typography>
                                    {getSubtitle(props.client)}</Box>} />
                <CardContent>
                    {cert && [
                    <TextField
                        key='Key Server'
                        label='Key Server'
                        defaultValue={`https://${props.client.config.Rest.KeyServer.keyServer}:${props.client.config.Rest.KeyServer.keyServerPort}${props.client.config.Rest.KeyServer.keyServerPath}`}
                        size='small'
                        InputProps={{ readOnly: true }} />,
                    <TextField
                        key='Issued on'
                        label='Issued on'
                        defaultValue={cert.validity.notBefore}
                        size='small'
                        InputProps={{ readOnly: true }} />,
                    <TextField
                        key='Expires on'
                        label='Expires on'
                        defaultValue={cert.validity.notAfter}
                        size='small'
                        InputProps={{ readOnly: true }} />]}
                </CardContent>
                <CardActions>
                    <IconButton onClick={_evt=>refreshStatus(props.client?.config?.Rest?.KeyServer !== undefined)}>{getStatusIcon(secureResponse,props.client?.config?.Rest?.KeyServer !== undefined)}</IconButton>
                    {props.client?.status?.Rest?.Keys && <ExpandMore
                        className="card-expand"
                        expand={expanded}
                        onClick={_evt => setExpanded(!expanded)}
                        aria-expanded={expanded}
                        aria-label="show more"
                        >
                        <ExpandMoreIcon />
                    </ExpandMore>}
                    {props.client?.config?.Rest?.KeyServer === undefined && 
                        <IconButton onClick={_evt => secureDevice()} >
                            <Typography variant='subtitle1'>Secure It</Typography>
                        </IconButton>}
                </CardActions>
                {props.client?.status?.Rest?.Keys && <Collapse in={expanded} timeout="auto" unmountOnExit>
                    <CardContent>
                        {props.client?.status?.Rest?.Keys && EndPoint(props.client.status.Rest.Keys)}
                    </CardContent>
                </Collapse>}
            </Card>
        </Box>;

        function secureDevice() {
            fetch(`${process.env.REACT_APP_FINDER_SERVICE}/device/secure`,{
                method:'post',
                headers: {
                  'Accept': 'application/json',
                  'Content-Type': 'application/json'
                },
                body: JSON.stringify(props.client)
            }).then(response => {
                if (!response.ok) {
                    throw new Error(`Request failed with status ${response.status}`)
                }
            }).catch(err => {throw new Error(err)});
        }

        function refreshStatus(secure) {
            if (secure) {
                setSecureResponse(0);
                fetch(`https://${process.env.REACT_APP_SECURE_BASE}:${process.env.REACT_APP_SECURE_PORT}/${props.client.config.devName}/status/app`,{
                    method: 'post',
                    protocol: 'https'
                }).then(response => {
                    if (!response.ok) {
                      throw new Error(`Request failed with status ${response.status}`)
                    }
                    return response.json()
                }).then(status => {
                    if (status.Rest !== undefined) {
                        setSecureResponse(1);
                        props.client.status = status;
                    } else {
                        setSecureResponse(2);
                        console.error(status);
                    }
                }).catch(_err => {
                    delete props.client.status;
                    setSecureResponse(2);
                });
            } else {
                if (props.client.status) {
                    props.client.refresh=true;
                    delete props.client.status;
                }
                setSecureResponse(2);
            }
        }
    }
}