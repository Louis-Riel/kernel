class Table extends React.Component {
    constructor(props) {
        super(props);
        this.id = this.props.id || genUUID();
        this.state = {json:this.props.json};
    }
    SortTable(th) {
        if (this.props.sortable){
            var table,tbody;
            Array.from((tbody=(table=th.target.closest("table")).querySelector('tbody')).querySelectorAll('tr:nth-child(n)'))
                    .sort(comparer(Array.from(th.target.parentNode.children).indexOf(th.target), this.asc = !this.asc))
                    .forEach(tr => tbody.appendChild(tr));
        }
    }

    BuildHeaderField(fld) {
        return fld;
    }

    addRow(e){
        e.stopPropagation();
        e.preventDefault();
        if ((this.state.json.length == 0) || (typeof this.state.json[0] == "object")){
            this.state.json.push({});
        } else if (Array.isArray(this.state.json[0])) {
            this.state.json.push([]);
        } else {
            this.state.json.push("");
        }
        this.setState({json:this.state.json});
    }

    BuildTablePannel(){
        if (this.props.editable) {
            return e("div",{key:genUUID(),className:"popupMenu"},[
                e("div",{key:genUUID()},"*"),
                e("div",{key:genUUID(),className:"menuItems"},[
                    e("div",{key:genUUID(),className:"menuItem", onClick:elem=> this.addRow(elem)},"Add Empty Row")
                ])
            ])
        }
        return null;
    }

    BuildCaption(){
        if (this.props.editable) {
            return e("caption", { key: genUUID() },[
                    e("div",{key:genUUID(),className:"objectButtons"}, this.BuildTablePannel()),
                    e("div",{key:genUUID(),className:"jsonlabel"},this.props.label)
                   ]);
        }
        return e("caption", { key: genUUID() }, this.props.label);
    }

    BuildHead(json) {
        if (json) {
            this.cols=[];
            return [e("thead", { key: genUUID(), onClick: this.SortTable.bind(this) }, 
                      e("tr", { key: genUUID() },
                        [this.props.editable?e("th", { key: genUUID() }):null,
                         ...json.flatMap(row => Object.keys(row))
                            .filter((val, idx, arr) => (val !== undefined) && (arr.indexOf(val) === idx))
                            .map(fld => {
                                if (!this.cols.some(col => fld == col)) {
                                    this.cols.push(fld);
                                    var val = this.getValue(fld,json[0][fld]);
                                    if (!this.sortedOn && !Array.isArray(val) && typeof val != 'object' && isNaN(this.getValue(fld,val))) {
                                        this.sortedOn = fld;
                                    }
                                }
                                return e("th", { key: genUUID() }, this.BuildHeaderField(fld));
                            })]
                        )), this.props.label !== undefined ? this.BuildCaption():null];
        } else {
            return null;
        }
    }

    getValue(fld, val) {
        if (val?.value !== undefined) {
            return val.value;
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
                this.props.sortable ? 
                    json.sort((e1,e2)=>(this.getValue(this.sortedOn,e1[this.sortedOn])+"").localeCompare((this.getValue(this.sortedOn,e2[this.sortedOn])+"")))
                        .map(this.BuildLine.bind(this)):
                    json.map(this.BuildLine.bind(this))
                )
        } else {
            return null;
        }
    }

    DeleteLine(line,e) {
        e.stopPropagation();
        e.preventDefault();
        this.state.json.splice(this.state.json.indexOf(line),1);
        this.setState({json:this.state.json});
    }

    DuplicateLine(line,e) {
        e.stopPropagation();
        e.preventDefault();
        this.state.json.push(line);
        this.setState({json:this.state.json});
    }

    BuildLinePannel(line){
        if (this.props.editable) {
            return e("div",{key:genUUID(),className:"popupMenu"},[
                e("div",{key:genUUID()},"*"),
                e("div",{key:genUUID(),className:"menuItems"},[
                    e("div",{key:genUUID(),className:"menuItem", onClick:elem=> this.DeleteLine(line,elem)},"Delete"),
                    e("div",{key:genUUID(),className:"menuItem", onClick:elem=> this.DuplicateLine(line,elem)},"Duplicate")
                ])
            ])
        }
        return null;
    }

    BuildLine(line) {
        return e("tr", { key: genUUID() },[
            [
                this.props.editable?e("td", { key: genUUID(), className: "readonly" },this.BuildLinePannel(line)):null,
                this.cols.map(fld => e("td", { key: genUUID(), className: "readonly" },this.BuildCell(line, fld)))
            ]
        ]);
    }

    addString(line,fld) {
        line[fld] = {value:"",version:0};
        this.setState({json:this.state.json});
    }

    clearCell(line,fld) {
        line[fld].value?line[fld].value=null:line[fld]=null;
        this.setState({json:this.state.json});
    }

    BuildCellControlPannel(line,fld){
        if (this.props.editable) {
            var val = line[fld];
            return e("div",{key:genUUID(),className:"popupMenu"},[
                e("div",{key:genUUID()},"*"),
                e("div",{key:genUUID(),className:"menuItems"},[
                    val?null:e("div",{key:genUUID(),className:"menuItem", onClick:elem=> this.addString(line,fld)},"Add String"),
                    val?e("div",{key:genUUID(),className:"menuItem",onClick:elem=>this.clearCell(line,fld) },"Clear"):null
                ])
            ])
        }
        return null;
    }

    BuildCell(line, fld) {
        return [Array.isArray(line[fld]) ?
            line[fld].length ? e(Table, { key: genUUID(), editable: this.props.editable, sortable: this.props.sortable, name: fld, json: line[fld], name: fld }) : null :
            e(JSONEditor, { key: genUUID(), editable: this.props.editable, json: line[fld], name: fld, registerEventInstanceCallback: this.props.registerEventInstanceCallback }),
            this.BuildCellControlPannel(line,fld)
        ];
    }

    render() {
        if (this.state.json === undefined) {
            return null;
        }

        return e("label", { key: genUUID(), id: this.id, className: "table" }, 
                e("table", { key: genUUID(), className: "greyGridTable" }, [
                    this.BuildHead(this.state.json), 
                    this.BuildBody(this.state.json)
                ])
               );
    }
}
