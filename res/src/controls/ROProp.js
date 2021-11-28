class ROProp extends React.Component {
    constructor(props) {
        super(props);
        this.id = this.props.id || genUUID();
    }

    getValue(fld,val) {
        if (val?.value !== undefined) {
            return this.getValue(fld,val.value);
        } else {
            if (IsNumberValue(val)) {
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

            if ((this.props.name == "name") && (val.match(/\/.*\.[a-z]{3}$/))) {
                return e("a", { href: `${httpPrefix}${val}` }, val);
            }

            return val;
        }
    }

    componentDidMount() {
        if (IsDatetimeValue(this.props.name)) {
            this.renderTime(document.getElementById(`vel${this.id}`), this.props.name, this.getValue(this.props.name,this.props.value));
        }
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

        //Background
        var gradient = ctx.createRadialGradient(rect.width / 2, rect.height / 2, 5, rect.width / 2, rect.height / 2, rect.height + 5);
        gradient.addColorStop(0, "#03303a");
        gradient.addColorStop(1, "black");
        ctx.fillStyle = gradient;
        ctx.clearRect(0, 0, rect.width, rect.height);

        ctx.beginPath();
        ctx.arc(rect.width / 2, rect.height / 2, rect.height * 0.44, degToRad(270), degToRad(270+((hrs>12?hrs-12:hrs)*30)));
        ctx.stroke();

        ctx.beginPath();
        ctx.arc(rect.width / 2, rect.height / 2, rect.height * 0.38, degToRad(270), degToRad(270+(smoothmin * 6)));
        ctx.stroke();

        //Date
        ctx.font = "12px Helvetica";
        ctx.fillStyle = 'rgba(00, 255, 255, 1)'
        var txtbx = ctx.measureText(today);
        ctx.fillText(today, (rect.width / 2) - txtbx.width / 2, rect.height * .65);

        //Time
        ctx.font = "12px Helvetica";
        ctx.fillStyle = 'rgba(00, 255, 255, 1)';
        txtbx = ctx.measureText(time);
        ctx.fillText(time, (rect.width * 0.50) - txtbx.width / 2, rect.height * 0.45);

    }

    render() {
        return e('label', { className: "readonly", id: `lbl${this.id}`, key: this.id }, [
            e("div", { key: genUUID(), className: "label", id: `dlbl${this.id}` }, this.props.label),
            e("div", { key: genUUID(), className: "value", id: `vel${this.id}` }, IsDatetimeValue(this.props.name) ? "" : this.getValue(this.props.name,this.props.value))
        ]);
    }
}
