import {createElement as e, Component} from 'react';
import './Events.css';
import { wfetch,fromVersionedToPlain } from '../../../utils/utils';
import { LiveEventPannel } from './LiveEventPannel';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faSpinner } from '@fortawesome/free-solid-svg-icons/faSpinner'

export var httpPrefix = "";

export default class EventsPage extends Component {

    componentWillUnmount() {
        this.mounted=false;
    }

    componentDidMount(){
        this.mounted=true;
        if (window.location.hostname || httpPrefix)
            this.getJsonConfig().then(cfg => this.mounted?this.setState({events: cfg.events,programs:cfg.programs}):null);
    }

    getJsonConfig() {
        return new Promise((resolve, reject) => {
            if (window.location.hostname || httpPrefix){
                const timer = setTimeout(() => this.props.pageControler.abort(), 8000);
                wfetch(`${httpPrefix}/config${this.props.selectedDeviceId === "current"?"":`/${this.props.selectedDeviceId}`}`, {
                    method: 'post'
                }).then(data => {
                    clearTimeout(timer);
                    return data.json();
                }).then( data => resolve(fromVersionedToPlain(data)
                )).catch((err) => {
                    clearTimeout(timer);
                    reject(err);
                });
            } else {
                reject({error:"Not connected"});
            }
        });
    }

    render() {
        if (this.state?.events){
            return [
                e(LiveEventPannel,{ key: "eventpannel",registerEventCallback:this.props.registerEventCallback})
            ];
        } else {
            return <FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />;
        }
    }
}
