class StateTable extends React.Component {
    constructor(props) {
        super(props);
        this.id = this.props.id || genUUID();
    }
    SortTable(th) {
        var table,tbody;
        Array.from((tbody=(table=th.target.closest("table")).querySelector('tbody')).querySelectorAll('tr:nth-child(n)'))
                 .sort(comparer(Array.from(th.target.parentNode.children).indexOf(th.target), this.asc = !this.asc))
                 .forEach(tr => tbody.appendChild(tr));
    }

    BuildHead(json) {
        if (json) {
            this.cols=[];
            return [e("thead", { key: genUUID(), onClick: this.SortTable.bind(this) }, e("tr", { key: genUUID() },
                json.flatMap(row => Object.keys(row))
                    .filter((val, idx, arr) => (val !== undefined) && (arr.indexOf(val) === idx))
                    .map(fld => {
                        if (!this.cols.some(col => fld == col)) {
                            this.cols.push(fld);
                            if (!this.sortedOn && json[0] && isNaN(json[0][fld])) {
                                this.sortedOn = fld;
                            }
                        }
                        return e("th", { key: genUUID() }, fld);
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
                json.sort((e1,e2)=>e1[this.sortedOn].localeCompare(e2[this.sortedOn]))
                    .map(line => e("tr", { key: genUUID() },
                                         this.cols.map(fld => e("td", { key: genUUID(), className: "readonly" }, 
                                                                             line[fld] !== undefined ? e("div", { key: genUUID(), className: "value" }, this.getValue(fld, line[fld])) : null)))));
        } else {
            return null;
        }
    }

    render() {
        if (!this.props?.json) {
            return e("div", { key: genUUID(), id: `loading${this.id}` }, "Loading...");
        }

        return e("label", { key: genUUID(), id: this.id, className: "table" }, 
               e("table", { key: genUUID(), className: "greyGridTable" }, 
               [this.BuildHead(this.props.json), 
               this.BuildBody(this.props.json)]));
    }
}
