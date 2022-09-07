import { createElement as e, Component } from 'react';
import CmdButton from './CmdButton';

export class StateCommands extends Component {
    render() {
        return e("div", { key: 'commands', name: "commands", className: "commands" }, this.props.commands.map(cmd => e(CmdButton, {
            key: `${cmd.command}-${cmd.param1}`,
            name: this.props.name,
            onSuccess: this.props.onSuccess,
            onError: this.props.onError,
            ...cmd
        })));
    }
}
