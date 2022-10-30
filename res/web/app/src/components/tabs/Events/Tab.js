import {createElement as e, Component} from 'react';
import './Events.css';
import { wfetch, fromVersionedToPlain } from '../../../utils/utils';
import { LiveEventPannel } from './LiveEventPannel';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faSpinner } from '@fortawesome/free-solid-svg-icons/faSpinner'
import Programs from '../../controls/Programs/Programs';
import Events from '../../controls/Events/Events';

export default class EventsPage extends Component {
    constructor(props){
        super(props);
        this.state = {
            httpPrefix:""
        }
    }

    componentDidMount(){
        this.getJsonConfig();
    }

    getJsonConfig() {
        var abort = new AbortController();
        const timer = setTimeout(() => abort.abort(), 8000);
        wfetch(`${this.state.httpPrefix}/config${!this.props.selectedDevice?.config?.ip?"":`/${this.props.selectedDevice.config.deviceid}`}`, {
            method: 'post',
            signal: abort.signal
        }).then(data =>  data.json())
          .then(fromVersionedToPlain)
          .then(cfg => this.setState({events: cfg.events,programs:cfg.programs}
        )).finally(() => clearTimeout(timer));
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (prevProps?.selectedDevice !== this.props.selectedDevice) {
            if (this.props.selectedDevice?.ip) {
                this.setState({httpPrefix:`http://${this.props.selectedDevice.ip}`});
            } else {
                this.setState({httpPrefix:""});
            }
        }
        if (prevState.httpPrefix !== this.state.httpPrefix) {
            this.getJsonConfig();
        }
    }

    render() {
        if (this.state?.events){
            return [
                <Programs programs={this.state.programs}></Programs>,
                <Events events={this.state.events} programs={this.state.programs}></Events>,
                e(LiveEventPannel,{ key: "eventpannel",selectedDevice:this.props.selectedDevice, registerEventCallback:this.props.registerEventCallback})
            ];
        } else {
            return <FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />;
        }
    }
}
