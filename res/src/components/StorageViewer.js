
class StorageViewer extends React.Component {
    constructor(props) {
        super(props);
        this.state = { loaded: false, path: this.props.path, files: null };

        this.id = this.props.id || genUUID();
        this.fetchFiles();
        this.mainDiv = document.createElement("div");
    }

    getSystemFolders() {
        return [e("tr", { key: genUUID(), className: "folder" }, [e("td", { key: genUUID() }, this.getFileLink({
            name: "..",
            folder: dirname(this.state.path),
            ftype: "folder",
            size: 0,
        })), e("td", { key: genUUID() }), , e("td", { key: genUUID() })])]
    }

    getFileLink(file) {
        if (file.ftype == "folder") {
            return e("a", {
                key: genUUID(),
                href: "#",
                onClick: () => {
                    this.setState({ total: 0, loaded: false, path: `${file.folder || "/"}${file.name == ".." ? "" : "/" + file.name}`.replaceAll("//", "/") });
                }
            }, file.name);
        } else {
            return e("a", { href: `${httpPrefix}${this.state.path}/${file.name}` }, file.name);
        }
    }

    GetFileStat(fileStatsToFetch) {
        if (fileStatsToFetch.length) {
            var fileToFetch = fileStatsToFetch.pop()
            var quitItNow = setTimeout(() => this.props.pageControler.abort(), 3000);
            fetch(`${httpPrefix}/stat${fileToFetch.folder}/${fileToFetch.name}`, {
                method: 'post',
                signal: this.props.pageControler.signal
            }).then(data => {
                clearTimeout(quitItNow);
                data.json().then(jdata => {
                    fileToFetch.size = jdata.size;
                    this.setState({ files: this.state.files, total: this.state.total + jdata.size });
                });
                if (fileStatsToFetch.length && !this.props.pageControler.signal.aborted) {
                    this.GetFileStat(fileStatsToFetch);
                }
            }).catch(ex => {
                clearTimeout(quitItNow);
                if (!this.props.pageControler.signal.aborted)
                    fileStatsToFetch.push(fileToFetch);
            });
        }
    }

    fetchFiles() {
        var quitItNow = setTimeout(() => this.props.pageControler.abort(), 3000);
        fetch(`${httpPrefix}/files` + this.state.path, {
            method: 'post',
            signal: this.props.pageControler.signal
        }).then(data => {
            clearInterval(quitItNow);
            data.json()
                .then(files => files.sort((f1, f2) => {
                    if (f1.ftype != f2.ftype) {
                        return f1.ftype == "folder" ? 1 : -1;
                    }
                    return f1.name == f2.name ? 0 : f1.name > f2.name ? 1 : -1;
                }))
                .then(files => {
                    this.setState({ loaded: true, files: files });
                    var fileStatsToFetch = files.filter(file => file.ftype == "file" && file.size==0);
                    for (var idx = 0; idx < Math.min(3, fileStatsToFetch.length); idx++) {
                        if (fileStatsToFetch.length) {
                            this.GetFileStat(fileStatsToFetch);
                        }
                    }
                });
        }).catch(console.error);
    }

    componentDidUpdate(prevProps, prevState) {
        if (!this.state.loaded) {
            this.state.loaded = true;
            this.fetchFiles();
        }
    }

    componentDidMount() {
        if (this.props.active) {
            document.getElementById("Storage").scrollIntoView()
        }
        document.getElementById(this.id).querySelectorAll("thd").forEach(th => th.addEventListener('click', (() => {
            const table = document.getElementsByClassName("file-table")[0].getElementsByTagName("tbody")[0];
            Array.from(table.querySelectorAll('tr:nth-child(n+2)'))
                .sort(comparer(Array.from(th.parentNode.children).indexOf(th), this.asc = !this.asc))
                .forEach(tr => table.appendChild(tr));
        })));
    }

    getFiles() {
        if (!this.state.files) {
            return e("tr", { key: genUUID() }, [e("td", { key: genUUID() }, "Loading..."), e("td", { key: genUUID() })])
        } else {
            return this.getSystemFolders()
                .concat(this.state.files.map(file => e("tr", { key: genUUID(), className: file.ftype }, [
                    e("td", { key: genUUID() }, this.getFileLink(file)),
                    e("td", { key: genUUID() }, file.ftype != "file" ? "" : file.size),
                    e("td", { key: genUUID() }, this.state.path == "/" ? null : e("a", {
                        key: genUUID(),
                        href: "#",
                        onClick: () => {
                            fetch(`${httpPrefix}/stat${this.state.path === "/" ? "" : this.state.path}/${file.name}`, {
                                method: 'post',
                                headers: {
                                    ftype: file.ftype == "file" ? "file" : "directory",
                                    operation: "delete"
                                }
                            })
                                .then(this.setState({ loaded: false }))
                                .catch(err => {
                                    console.error(err);
                                });
                        }
                    }, "Del"))
                ])));
        }
    }

    getTableHeader() {
        return e("thead", { key: genUUID() }, e("tr", { key: genUUID() }, this.props.cols.map(col => e("th", { key: genUUID() }, col)).concat(e("th", { key: genUUID() }, "Op"))));
    }

    render() {
        return [
            this.mainDiv = e("div", { key: genUUID(), id: this.id, className: "file-table" }, e("table", { key: genUUID(), className: "greyGridTable" }, [
                e("caption", { key: genUUID() }, this.state.path),
                this.getTableHeader(),
                e("tbody", { key: genUUID() }, this.getFiles()),
                e("tfoot", { key: genUUID() }, e("tr", { key: genUUID() }, [e("td", { key: genUUID() }, "Total"), e("td", { key: genUUID() }, this.state.total)]))
            ]))

        ];
    }
}
