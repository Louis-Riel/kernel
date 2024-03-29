class CmdButton extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            param1: this.props.param1,
            param2: this.props.param2,
            param3: this.props.param3,
            param4: this.props.param4,
            param5: this.props.param5,
            param6: this.props.param6
        };
    }
    runIt() {
        wfetch(`${httpPrefix}/status/cmd`, {
            method: this.props.HTTP_METHOD,
            body: JSON.stringify({command: this.props.command, className: this.props.className ,name: this.props.name, ...this.state})
        }).then(data => data.text())
          .then(this.props.onSuccess ? this.props.onSuccess : console.log)
          .catch(this.props.onError ? this.props.onError : console.error);
    }

    simpleCommand() {
        return e("button", { key: "simpleCommand", onClick: this.runIt.bind(this) }, this.props.caption)
    }

    GetPropertyEditor(param) {
        return e( MaterialUI.FormControl,{key: param},[
                e(MaterialUI.InputLabel,{
                    key: "label",
                    className: "label",
                    id: `${param}-label`
                },this.props[`${param}_label`]||param),
                e(MaterialUI.Input,{
                    key:"input",
                    id: `${param}-input`,
                    type: typeof this.state[param] === "number" || !isNaN(this.state[param]) ? 'number' : 'text',
                    label: param,
                    value: this.state[param],
                    onChange: elem => {this.state[param] = elem.target.type === 'number' ? parseInt(elem.target.value) : elem.target.value; this.setState(this.state)}
                })]);
    }

    complexCommand() {
        return e("div",{key: "complexCommand", className: "complex-command"},[Object.keys(this.props)
                                                                                   .filter(k => k.startsWith("param") && this.props[k+"_editable"])
                                                                                   .map(k => this.GetPropertyEditor(k)),
               e("button", { key: genUUID(), onClick: this.runIt.bind(this) }, this.props.caption)]);
    }

    hasComplexCommand() {
        return Object.keys(this.props).filter(k => k.endsWith("_editable") && this.props[k]).length > 0
    }

    render() {
        return this.hasComplexCommand() ? this.complexCommand() : this.simpleCommand()
    }
}
