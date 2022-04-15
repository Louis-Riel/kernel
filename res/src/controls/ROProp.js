class ROProp extends React.Component {
    constructor(props) {
        super(props);
        this.id = this.props.id || genUUID();
        if (IsNumberValue(this.props.value)) {
            this.state = {
                maxLastStates: 50,
                lastStates:[
                    {
                        value:this.props.value,
                        ts: Date.now()
                    }
                ]
            };
        }
    }

    getValue(fld,val) {
        if (val?.value !== undefined) {
            return this.getValue(fld,val.value);
        }

        if (IsNumberValue(val) && isFloat(val)) {
            return parseFloat(val).toFixed(this.isGeoField() ? 8 : 2).replace(/0+$/,'');
        }
        
        if (IsBooleanValue(val)) {
            return ((val === "true") || (val === "yes") || (val === true)) ? "Y" : "N"
        }

        if ((this.props.name === "name") && (val.match(/\/.*\.[a-z]{3}$/))) {
            return e("a", { href: `${httpPrefix}${val}` }, val.split('/').reverse()[0]);
        }
        return val;
    }

    isGeoField() {
        return (this.props.name.toLowerCase() === "lattitude") ||
            (this.props.name.toLowerCase() === "longitude") ||
            (this.props.name === "lat") || 
            (this.props.name === "lng");
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (IsDatetimeValue(this.props.name)) {
            this.renderTime(document.getElementById(`vel${this.id}`), this.props.name, this.getValue(this.props.name,this.props.value));
        }
        if (this.state?.lastStates && (this.props.value !== prevProps?.value)) {
            this.setState({lastStates: [...this.getActiveLastStates(), {
                value: this.getValue(this.props.name,this.props.value),
                ts: Date.now()
            }]});
        }
    }

    getActiveLastStates() {
        if (this.state.lastStates.length > (this.state.maxLastStates - 1)) {
            return this.state.lastStates.splice(this.state.lastStates.length - this.state.maxLastStates);
        }
        return this.state.lastStates;
    }

    renderTime(input, fld, val) {
        if (input == null) {
            return;
        }
        var now = fld.endsWith("_us") ? new Date(val / 1000) : fld.endsWith("_sec") ? new Date(val*1000) : new Date(val);

        if (now.getFullYear() <= 1970) 
            now.setTime(now.getTime() + now.getTimezoneOffset() * 60 * 1000);
            
        var today = now.toLocaleDateString('en-US',{dateStyle:"short"});
        var time = now.toLocaleTimeString('en-US',{hour12:false});
        var hrs = now.getHours();
        var min = now.getMinutes();
        var sec = now.getSeconds();
        var mil = now.getMilliseconds();
        var smoothsec = sec + (mil / 1000);
        var smoothmin = min + (smoothsec / 60);

        if (now.getFullYear() <= 1970) {
            today =  (now.getDate()-1) + ' Days';
            if (hrs == 0)
                time = (min ? min + ":" : "") + ('0'+sec).slice(-2) + "." + mil;
            else
                time = ('0'+hrs).slice(-2) + ":" + ('0'+min).slice(-2) + ":" + ('0'+sec).slice(-2);
        }

        var canvas = input.querySelector(`canvas`) || input.appendChild(document.createElement("canvas"));
        canvas.height = 100;
        canvas.width = 110;
        var ctx = canvas.getContext("2d");
        ctx.strokeStyle = '#00ffff';
        ctx.lineWidth = 4;
        ctx.shadowBlur = 2;
        ctx.shadowColor = '#00ffff'
        var rect = input.getBoundingClientRect();
        rect.height = canvas.height;
        rect.width = canvas.width;

        this.drawBackground(ctx, rect, hrs, smoothmin);
        this.drawClock(ctx, today, rect, time);
    }

    drawClock(ctx, today, rect, time) {
        //Date
        ctx.font = "12px Helvetica";
        ctx.fillStyle = 'rgba(00, 255, 255, 1)';
        var txtbx = ctx.measureText(today);
        ctx.fillText(today, (rect.width / 2) - txtbx.width / 2, rect.height * .65);

        //Time
        ctx.font = "12px Helvetica";
        ctx.fillStyle = 'rgba(00, 255, 255, 1)';
        txtbx = ctx.measureText(time);
        ctx.fillText(time, (rect.width * 0.50) - txtbx.width / 2, rect.height * 0.45);
    }

    drawBackground(ctx, rect, hrs, smoothmin) {
        var gradient = ctx.createRadialGradient(rect.width / 2, rect.height / 2, 5, rect.width / 2, rect.height / 2, rect.height + 5);
        gradient.addColorStop(0, "#03303a");
        gradient.addColorStop(1, "black");
        ctx.fillStyle = gradient;
        ctx.clearRect(0, 0, rect.width, rect.height);

        ctx.beginPath();
        ctx.arc(rect.width / 2, rect.height / 2, rect.height * 0.44, degToRad(270), degToRad(270 + ((hrs > 12 ? hrs - 12 : hrs) * 30)));
        ctx.stroke();

        ctx.beginPath();
        ctx.arc(rect.width / 2, rect.height / 2, rect.height * 0.38, degToRad(270), degToRad(270 + (smoothmin * 6)));
        ctx.stroke();
    }

    getGraphButton() {
        var ret = null;
        if (this.hasStats()) {
            ret = e(MaterialUI.Tooltip,{
                      className: "graphtooltip",
                      key:"tooltip",
                      width: "200",
                      height: "100",
                      title: this.state.graph ? "Close" : this.getReport(true)
                    },e('i',{className: "reportbtn fa fa-line-chart", key: "graphbtn", onClick: elem=>this.setState({"graph":this.state.graph?false:true})})
                  );
        }
        return ret;
    }

    hasStats() {
        return !IsDatetimeValue(this.props.name) &&
            IsNumberValue(this.props.value) &&
            this.state?.lastStates &&
            this.state.lastStates.reduce((ret, state) => ret.find(it => it === state.value) ? ret : [state.value, ...ret], []).length > 2;
    }

    getReport(summary) {
        return e(Recharts.ResponsiveContainer,{key:"chartcontainer", className: "chartcontainer"},
                e(Recharts.LineChart,{key:"chart", data: this.state.lastStates, className: "chart", margin: {left:20}},[
                    e(Recharts.Line, {key:"line", dot: !summary, type:"monotone", dataKey:"value", stroke:"#8884d8", isAnimationActive: false}),
                    summary?null:e(Recharts.CartesianGrid, {key:"grid", hide:summary, strokeDasharray:"5 5", stroke:"#ccc"}),
                    e(Recharts.XAxis, {key:"thexs", hide:summary, dataKey:"ts",type: 'number', domain: ['auto', 'auto'],name: 'Time', tickFormatter: (unixTime) => new Date(unixTime).toLocaleTimeString(), type: "number"}),
                    e(Recharts.YAxis, {key:"theys", hide:summary, dataKey:"value", domain: ['auto', 'auto']}),
                    e(Recharts.Tooltip, {key:"tooltip", contentStyle: {backgroundColor: "black"}, labelStyle: {backgroundColor: "black"}, className:"tooltip", labelFormatter: t => new Date(t).toLocaleString()})
        ]));
    }

    getLabeledField() {
        return e('label', { className: `readonly ${ this.state?.graph ? 'graph' : '' }`, id: `lbl${this.id}`, key: this.id }, [
            e("div", { key: 'label', className: "label", id: `dlbl${this.id}` }, [this.props.label, this.getGraphButton()]),
            e("div", { key: 'value', className: "value", id: `vel${this.id}` }, IsDatetimeValue(this.props.name) ? "" : this.getValue(this.props.name, this.props.value)),
            this.state?.graph && this.hasStats() ? this.getReport(false) : null
        ]);
    }

    getTableField() {
        return e("div", { key: 'timevalue', className: "value", id: `vel${this.id}` }, IsDatetimeValue(this.props.name) ? "" : [this.getValue(this.props.name, this.props.value),this.getGraphButton()]);
    }

    render() {
        return this.props.label ? this.getLabeledField() : this.getTableField();
    }
}
