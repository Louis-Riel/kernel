import { createElement as e, Component } from 'react';
import TripViewer from './TripViewer';

export class TripWithin extends Component {
    getIconColor() {
        if (!this.state?.points) {
            return "#00ff00";
        } else if (this.props.points.length) {
            return "aquamarine";
        } else {
            return "#ff0000";
        }
    }

    getIcon() {
        return e("svg", { key: "svg", xmlns: "http://www.w3.org/2000/svg", viewBox: "0 0 64 64", onClick: elem => this.setState({ viewer: "trip" }) }, [
            e("path", { key: "path1", style: { fill: this.getIconColor() }, d: "M17.993 56h20v6h-20zm-4.849-18.151c4.1 4.1 9.475 6.149 14.85 6.149 5.062 0 10.11-1.842 14.107-5.478l.035.035 1.414-1.414-.035-.035c7.496-8.241 7.289-20.996-.672-28.957A20.943 20.943 0 0 0 27.992 2a20.927 20.927 0 0 0-14.106 5.477l-.035-.035-1.414 1.414.035.035c-7.496 8.243-7.289 20.997.672 28.958zM27.992 4.001c5.076 0 9.848 1.976 13.437 5.563 7.17 7.17 7.379 18.678.671 26.129L15.299 8.892c3.493-3.149 7.954-4.891 12.693-4.891zm12.696 33.106c-3.493 3.149-7.954 4.892-12.694 4.892a18.876 18.876 0 0 1-13.435-5.563c-7.17-7.17-7.379-18.678-.671-26.129l26.8 26.8z" }),
            e("path", { key: "path2", style: { fill: this.getIconColor() }, d: "M48.499 2.494l-2.828 2.828c4.722 4.721 7.322 10.999 7.322 17.678s-2.601 12.957-7.322 17.678S34.673 48 27.993 48s-12.957-2.601-17.678-7.322l-2.828 2.828C12.962 48.983 20.245 52 27.993 52s15.031-3.017 20.506-8.494c5.478-5.477 8.494-12.759 8.494-20.506S53.977 7.97 48.499 2.494z" })
        ]);
    }

    render() {
        return e("div", { key: "renderedtrip", className: "rendered trip" }, [
            this.getIcon(),
            this.state?.viewer === "trip" ? e(TripViewer, { key: "tripviewer", points: this.props.points, cache: this.props.cache, onClose: () => this.setState({ "viewer": "" }) }) : null
        ]
        );
    }
}
