class Table extends React.Component {
    constructor(props) {
        super(props);
        this.id = this.props.id || genUUID();
        this.state = {
            keyColumn: this.getKeyColumn()
        }
    }

    componentDidUpdate(prevProps, prevState) {
        var keycol = this.getKeyColumn();
        if (this.state.keyColumn != keycol){
            this.setState({keyColumn:keycol});
        }
    }
    getKeyColumn() {
        if (this.props.json && this.props.json.length > 0) {
            if (typeof this.props.json[0] === "object") {
                var keyCol = Object.keys(this.props.json[0])[0]
                return this.props.json.reduce((acc, cur) => acc.find(row=>row[keyCol] === cur[keyCol]) ? acc : [...acc,cur],[]).length === this.props.json.length ? keyCol : null;
            }
        }
        return undefined;
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
        return e("th", { 
            key: `header-col-${fld}`,
            onClick: this.SortTable.bind(this)
        }, fld);
    }

    addRow(e){
        e.stopPropagation();
        e.preventDefault();
        if ((this.props.json.length == 0) || (typeof this.props.json[0] == "object")){
            this.props.json.push({});
        } else if (Array.isArray(this.props.json[0])) {
            this.props.json.push([]);
        } else {
            this.props.json.push("");
        }
        this.setState({json:this.props.json});
    }

    BuildTablePannel(){
        if (this.props.editable) {
            return e("div",{key:genUUID(),className:"popupMenu"},
                    e("div",{key:`removeprop`, onClick:elem=> this.addRow(elem)},"Add Row"));
        }
        return null;
    }

    BuildCaption(){
        if (this.props.editable) {
            return e("caption", { key: `caption` },[
                    e("div",{key:'table-panel',className:"objectButtons"}, this.BuildTablePannel()),
                    e("div",{key:`label`,className:"jsonlabel"},this.props.label)
                   ]);
        }
        return e("caption", { key: 'caption' }, `${this.props.label} - ${this.state.keyColumn}`);
    }

    getValue(fld, val) {
        if (fld === undefined || val === undefined) {
            return "";
        }
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

    DeleteLine(line,e) {
        e.stopPropagation();
        e.preventDefault();
        this.props.json.splice(line,1);
        this.setState({json:this.props.json});
    }

    DuplicateLine(line,e) {
        e.stopPropagation();
        e.preventDefault();
        this.props.json.push(this.props.json[line]);
        this.setState({json:this.props.json});
    }

    BuildLinePannel(idx){
        if (this.props.editable) {
            return e("div",{key:`linepanel`,className:"stackedMenu"},[
                e("div",{key:genUUID(),className:"popupMenu"},
                    e("div",{key:`del`, onClick:elem=> this.DeleteLine(idx,elem)},"Delete")),
                e("div",{key:genUUID(),className:"popupMenu"},
                    e("div",{key:`dup`, onClick:elem=> this.DuplicateLine(idx,elem)},"Duplicate"))
            ])
        }
        return null;
    }

    BuildLine(line,idx) {
        return e("tr", { key: this.getRowKey(line, idx) },[
            [
                this.props.editable?e("td", { key: `${this.getRowKey(line, idx)}-panel`, className: "readonly" },this.BuildLinePannel(idx)):null,
                this.cols.map(fld => e("td", { key: `${this.getRowKey(line, idx)}-${fld}-cell-panel`, className: "readonly" },this.BuildCell(line, fld)))
            ]
        ]);
    }

    getRowKey(line, idx) {
        return `row-${this.state.keyColumn ? line[this.state.keyColumn] : idx}`;
    }

    addString(line,fld) {
        line[fld] = {value:"",version:0};
        this.setState({json:this.props.json});
    }

    clearCell(line,fld) {
        line[fld].value?line[fld].value=null:line[fld]=null;
        this.setState({json:this.props.json});
    }

    BuildCellControlPannel(line,fld){
        if (this.props.editable) {
            var val = line[fld];
            return e("div",{key:genUUID(),className:"popupMenu"},
                        val?
                            e("div",{key:`addpropr`, onClick:elem=> this.clearCell(line,fld)},"Set Null"):
                            e("div",{key:`addpropr`, onClick:elem=> this.addString(line,fld)},"Add String"));
        }
        return null;
    }

    BuildCell(line, fld) {
        return [Array.isArray(line[fld]) ?
                      line[fld].length ? 
                        e(Table, { key: `Table-Array-Line-${this.props.path}/${fld}`, 
                                   path: `${this.props.path}/${fld}`,
                                   editable: this.props.editable, 
                                   sortable: this.props.sortable, 
                                   name: fld, 
                                   json: line[fld], 
                                   name: fld 
                                 }) : 
                        null :
                e(LocalJSONEditor, { key: `JE-${this.props.path}/${fld}`, 
                                path: `${this.props.path}/${fld}`,
                                editable: this.props.editable, 
                                json: line[fld], 
                                name: fld, 
                                registerEventInstanceCallback: this.props.registerEventInstanceCallback 
                              }),
            this.BuildCellControlPannel(line,fld)
        ];
    }

    render() {
        if (this.props.json === undefined) {
            return null;
        }
        this.cols=[];
        return e("label", { key: `label`, id: this.id, className: "table" }, 
                e("table", { key: `table`, className: "greyGridTable" }, [
                    [e("thead", { key: `head` }, 
                        e("tr", { key: `headrow` },
                        [this.props.editable?e("th", { key: `header` }):null,
                            ...this.props.json.flatMap(row => Object.keys(row))
                            .filter((val, idx, arr) => (val !== undefined) && (arr.indexOf(val) === idx))
                            .map(fld => {
                                if (!this.cols.some(col => fld == col)) {
                                    this.cols.push(fld);
                                    var val = this.getValue(fld,this.props.json[0][fld]);
                                    if (!this.sortedOn && !Array.isArray(val) && typeof val != 'object' && isNaN(this.getValue(fld,val))) {
                                        this.sortedOn = fld;
                                    }
                                }
                                return this.BuildHeaderField(fld);
                            })]
                        )), this.props.label !== undefined ? this.BuildCaption():null], 
                    e("tbody", { key: 'body' },
                        this.props.sortable ? 
                        this.props.json.sort((e1,e2)=>(this.getValue(this.sortedOn,e1[this.sortedOn])+"").localeCompare((this.getValue(this.sortedOn,e2[this.sortedOn])+"")))
                            .map(this.BuildLine.bind(this)):
                        this.props.json.map(this.BuildLine.bind(this)))
                ])
            );
    }
}
