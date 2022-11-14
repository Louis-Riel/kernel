import { createElement as e, Component } from 'react';
import { wfetch } from '../../../utils/utils';
import { TripWithin } from './TripWithin';

export class FileViewer extends Component {
    constructor(props) {
        super(props);
        this.state = {
            httpPrefix:this.props.selectedDevice?.ip ? `http://${this.props.selectedDevice.ip}` : ".",
            renderers: []
        };
    }

    componentDidUpdate(prevProps,prevState) {
        if (prevProps?.selectedDevice !== this.props.selectedDevice) {
            if (this.props.selectedDevice?.ip) {
                this.setState({httpPrefix:`http://${this.props.selectedDevice.ip}`});
            } else {
                this.setState({httpPrefix:"."});
            }
        }
    }

    componentDidMount() {
        if (this.props.registerFileWork) {
            this.props.registerFileWork(this.buildRenderers(0));
        }
    }

    buildRenderers(retryCount) {
        return new Promise((resolve, reject) => {
            if (this.props.name.endsWith(".csv") && !this.state?.renderers?.some("trip")) {
                this.setState({ renderers: [{ name: "loading" }] });
                this.parseCsv(resolve, retryCount, reject);
            } else if (this.props.name.endsWith(".log") && !this.state?.renderers?.some("trip")) {
                this.setState({ renderers: [{ name: "loading" }] });
                this.parseLog(resolve, retryCount, reject);
            } else {
                resolve();
            }
        });
    }

    parseLog(resolve, retryCount, reject) {
        wfetch(`${this.state.httpPrefix}${this.props.folder}/${this.props.name}`)
            .then(resp => resp.text())
            .then(content => resolve(this.setState({
                renderers: [{
                    name: "trip",
                    points: content.split("\n")
                        .filter(ln => ln.match(/[ID] \(\d{2}:\d{2}:\d{2}[.]\d{3}\).* Location:.*/))
                        .map(ln => ln.match(/[ID] \((?<timestamp>.*)\).*Location:\s*(?<latitude>[0-9.-]+),\s*(?<longitude>[0-9.-]+).*speed:\s*(?<speed>[0-9.]+).*altitude:\s*(?<altitude>[0-9.]+).*course:\s*(?<course>[0-9.]+).*bat:\s*(?<Battery>[0-9.]+)/i)?.groups)
                        .filter(point => point)
                        .map(point => {
                            Object.keys(point).forEach(fld => point[fld] = isNaN(point[fld]) ? point[fld] : parseFloat(point[fld]));
                            point.timestamp = `1970-01-01 ${point.timestamp}`;
                            point.altitude *= 100;
                            point.speed *= 100;
                            return point;
                        })
                }]
            }))
            ).catch(err => {
                if (retryCount++ < 3) {
                    this.buildRenderers(retryCount);
                } else {
                    reject(err);
                }
            });
    }

    parseCsv(resolve, retryCount, reject) {
        wfetch(`${this.state.httpPrefix}${this.props.folder}/${this.props.name}`)
            .then(resp => resp.text())
            .then(content => {
                let cols = content.split(/\n|\r\n/)[0].split(",");
                resolve(this.setState({
                    renderers: [{
                        name: "trip",
                        points: content.split(/\n|\r\n/)
                            .splice(1).map(ln => {
                                let ret = {};
                                ln.split(",").forEach((it, idx) => ret[cols[idx]] = isNaN(it) ? it : parseFloat(it));
                                return ret;
                            }).filter(item => item.timestamp && item.timestamp.match(/\d{4}\/\d{2}\/\d{2} \d{2}:\d{2}:\d{2}/))
                    }]
                }));
            }).catch(err => {
                if (retryCount++ < 3) {
                    this.buildRenderers(retryCount);
                } else {
                    reject(err);
                }
            });
    }

    getRenderers() {
        if (!this.state?.renderers || !this.state.renderers.length) {
            return null;
        }
        return this.state.renderers.map(renderer => {
            switch (renderer.name) {
                case "trip":
                    return renderer.points.length ? e(TripWithin, { key: "tripwithin", selectedDevice:this.props.selectedDevice, points: renderer.points, cache: this.props.cache }) : null;

                case "loading":
                    return e('i', { className: "rendered reportbtn fa fa-spinner", key: "graphbtn", });

                default:
                    return null;
            }
        });
    }

    render() {
        return [e("a", { key: "filelink", href: `${this.state.httpPrefix}${this.props.folder}/${this.props.name}` }, this.props.name.split('/').reverse()[0]), this.getRenderers()];
    }
}
