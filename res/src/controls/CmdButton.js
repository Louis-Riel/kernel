class CmdButton extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            checked: this.props.initialState ? this.props.initialState() : false
        };
        this.id = this.props.id || genUUID();
    }

    runIt() {
        fetch(`${httpPrefix}/status/cmd`, {
            method: this.props.HTTP_METHOD,
            body: JSON.stringify({command: this.props.command,name: this.props.name, param1: this.props.param1, param2: this.props.param2})
        }).then(data => data.text())
          .then(this.props.onSuccess ? this.props.onSuccess : console.log)
          .catch(this.props.onError ? this.props.onError : console.error);
    }

    render() {
        return e("button", { key: genUUID(), onClick: this.runIt.bind(this) }, this.props.caption)
    }
}
