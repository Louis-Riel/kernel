import { createElement as e, Component } from 'react';
import { chipRequest } from '../../../utils/utils';
import { FileViewer } from './FileViewer';

export class SFile extends Component {
    render() {
        return e("tr", { key: "tr", className: this.props.file.ftype }, [
            e("td", { key: "link" }, this.getLink(this.props.file)),
            e("td", { key: "size" }, this.props.file.ftype !== "file" ? "" : this.props.file.size),
            e("td", { key: "delete" }, this.getDeleteLink())
        ]);
    }

    getDeleteLink() {
        return this.props.path === "/" || this.props.file.name === ".." ? null : e("a", {
            key: "delete",
            href: "#",
            onClick: () => {
                chipRequest(`/stat${this.props.path === "/" ? "" : this.props.path}/${this.props.file.name}`, {
                    method: 'post',
                    headers: {
                        ftype: this.props.file.ftype === "file" ? "file" : "directory",
                        operation: "delete"
                    }
                }).then(this.props.OnDelete ? this.props.OnDelete() : null)
                    .catch(err => {
                        console.error(err);
                    });
            }
        }, "Del");
    }

    getLink(file) {
        if (file.ftype === "folder") {
            return e("a", {
                key: file.name,
                href: "#",
                onClick: () => this.props.onChangeFolder ? this.props.onChangeFolder(`${file.folder || "/"}${file.name === ".." ? "" : "/" + file.name}`.replaceAll("//", "/")) : null
            }, file.name);
        } else {
            return e(FileViewer, { key: file.name, 
                                   selectedDevice: this.props.selectedDevice,
                                   cache: this.props.cache, 
                                   registerFileWork: this.props.registerFileWork, ...file });
        }
    }
}
