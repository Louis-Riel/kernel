class StateTable extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            json: this.props.json,
            error: null,
            cols: []
        };
        this.id = this.props.id || genUUID();
    }

    BuildHead(json) {
        if (json) {
            return [e("thead", { key: genUUID(), }, e("tr", { key: genUUID() },
                json.flatMap(row => Object.keys(row))
                    .concat(this.props.cols)
                    .filter((val, idx, arr) => (val !== undefined) && (arr.indexOf(val) === idx))
                    .map(fld => {
                        if (!this.state.cols.some(col => fld == col)) {
                            this.state.cols.push(fld);
                            if (!this.state.sortedOn && json[0] && isNaN(json[0][fld])) {
                                this.state.sortedOn = fld;
                            }
                        }
                        return e("td", { key: genUUID() }, fld);
                    }))), e("caption", { key: genUUID() }, this.props.label)];
        } else {
            return null;
        }
    }

    getValue(fld, val) {
        if (fld.endsWith("_sec") && (val > 10000000)) {
            return new Date(val * 1000).toLocaleString();
        } else if (IsNumberValue(val)) {
            if (isFloat(val)) {
                if ((this.props.name.toLowerCase() != "lattitude") &&
                    (this.props.name.toLowerCase() != "longitude") &&
                    (this.props.name != "lat") && (this.props.name != "lng")) {
                    val = parseFloat(val).toFixed(2);
                } else {
                    val = parseFloat(val).toFixed(8);
                }
            }
        }
        if (IsBooleanValue(val)) {
            val = ((val == "true") || (val == "yes") || (val === true)) ? "Y" : "N"
        }
        return val;
    }

    BuildBody(json) {
        if (json) {
            return e("tbody", { key: genUUID() },
                json.sort((e1,e2)=>e1[this.state.sortedOn].localeCompare(e2[this.state.sortedOn]))
                    .map(line => e("tr", { key: genUUID() },
                                         this.state.cols.map(fld => e("td", { key: genUUID(), className: "readonly" }, 
                                                                             line[fld] !== undefined ? e("div", { key: genUUID(), className: "value" }, this.getValue(fld, line[fld])) : null)))));
        } else {
            return null;
        }
    }

    render() {
        if (this.props.json === null || this.props.json === undefined) {
            return e("div", { key: genUUID(), id: `loading${this.id}` }, "Loading...");
        }

        return e("label", { key: genUUID(), id: this.id, className: "table" }, 
               e("table", { key: genUUID(), className: "greyGridTable" }, 
               [this.BuildHead(this.props.json), 
               this.BuildBody(this.props.json)]));
    }
}
