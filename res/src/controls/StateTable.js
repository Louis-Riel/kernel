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
                            var val = this.getValue(fld,json[0][fld]);
                            if (!this.sortedOn && !Array.isArray(val) && typeof val != 'object' && isNaN(this.getValue(fld,val))) {
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
        if (val?.value !== undefined) {
            return this.getValue(fld,val.value);
        } else {
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
    
            if ((fld == "name") && (val.match(/\/.*\.[a-z]{3}$/))) {
                return e("a", { href: `${httpPrefix}${val}` }, val);
            }
            return val;
        }
    }

    BuildBody(json) {
        if (json) {
            return e("tbody", { key: genUUID() },
                json.sort((e1,e2)=>(this.getValue(this.sortedOn,e1[this.sortedOn])+"").localeCompare((this.getValue(this.sortedOn,e2[this.sortedOn])+"")))
                    .map(line => e("tr", { key: genUUID() },
                        this.cols.map(fld => e("td", { key: genUUID(), className: "readonly" }, 
                            typeof this.getValue(fld, line[fld]) != 'object' ? 
                                e("div", { key: genUUID(), className: "value" }, this.getValue(fld, line[fld])) : 
                                Array.isArray(line[fld]) ? 
                                    e(StateTable,{key: genUUID(), name:fld, label:fld, json:line[fld]}):
                                    e(AppState,{key: genUUID(),json:line[fld]})
                        )))
                    ));
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
