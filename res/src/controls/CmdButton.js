class CmdButton extends React.Component {
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
