class TripWithin extends React.Component {
    getIconColor() {
        if (!this.state?.points) {
            return "#00ff00";
        } else if (this.props.points.length) {
            return "aquamarine";
        } else {
            return "#ff0000";
        }
    }

    getIcon() {
        return e("svg", { key: "svg", xmlns: "http://www.w3.org/2000/svg", viewBox: "0 0 64 64",onClick:elem=>this.setState({viewer:"trip"}) }, [
            e("path", { key: "path1", style: { fill: this.getIconColor() }, d: "M17.993 56h20v6h-20zm-4.849-18.151c4.1 4.1 9.475 6.149 14.85 6.149 5.062 0 10.11-1.842 14.107-5.478l.035.035 1.414-1.414-.035-.035c7.496-8.241 7.289-20.996-.672-28.957A20.943 20.943 0 0 0 27.992 2a20.927 20.927 0 0 0-14.106 5.477l-.035-.035-1.414 1.414.035.035c-7.496 8.243-7.289 20.997.672 28.958zM27.992 4.001c5.076 0 9.848 1.976 13.437 5.563 7.17 7.17 7.379 18.678.671 26.129L15.299 8.892c3.493-3.149 7.954-4.891 12.693-4.891zm12.696 33.106c-3.493 3.149-7.954 4.892-12.694 4.892a18.876 18.876 0 0 1-13.435-5.563c-7.17-7.17-7.379-18.678-.671-26.129l26.8 26.8z" }),
            e("path", { key: "path2", style: { fill: this.getIconColor() }, d: "M48.499 2.494l-2.828 2.828c4.722 4.721 7.322 10.999 7.322 17.678s-2.601 12.957-7.322 17.678S34.673 48 27.993 48s-12.957-2.601-17.678-7.322l-2.828 2.828C12.962 48.983 20.245 52 27.993 52s15.031-3.017 20.506-8.494c5.478-5.477 8.494-12.759 8.494-20.506S53.977 7.97 48.499 2.494z" })
        ]);
    }

    render() {
        return e("div",{key:"renderedtrip",className:"rendered trip"},[
            this.getIcon(),
            this.state?.viewer == "trip"?e(TripViewer,{key:"tripviewer",points:this.props.points,cache:this.props.cache,onClose:()=>this.setState({"viewer":""})}):null
        ]
        );
    }
}
class FileViewer extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            renderers: []
        }
    }

    componentDidMount() {
        if (this.props.registerFileWork) {
            this.props.registerFileWork(this.buildRenderers(0));
        }
    }

    buildRenderers(retryCount) {
        return new Promise((resolve, reject) => {
            if (this.props.name.endsWith(".csv") && !this.state?.renderers?.some("trip")){
                this.setState({renderers:[{name:"loading"}]});
                this.parseCsv(resolve, retryCount, reject);
            } else if (this.props.name.endsWith(".log") && !this.state?.renderers?.some("trip")){
                this.setState({renderers:[{name:"loading"}]});
                this.parseLog(resolve, retryCount, reject);
            } else {
                resolve();
            }
        });
    }

    parseLog(resolve, retryCount, reject) {
        wfetch(`${httpPrefix}${this.props.folder}/${this.props.name}`)
            .then(resp => resp.text())
            .then(content => resolve(this.setState({
                renderers: [{
                    name: "trip",
                    points: content.split("\n")
                        .filter(ln => ln.match(/[ID] \([0-9]{2}:[0-9]{2}:[0-9]{2}[.][0-9]{3}\).* Location:.*/))
                        .map(ln => ln.match(/[ID] \((?<timestamp>.*)\).*Location:\s*(?<latitude>[0-9.-]+),\s*(?<longitude>[0-9.-]+).*speed:\s*(?<speed>[0-9.]+).*altitude:\s*(?<altitude>[0-9.]+).*course:\s*(?<course>[0-9.]+).*bat:\s*(?<Battery>[0-9.]+)/i)?.groups)
                        .filter(point => point)
                        .map(point => {
                            Object.keys(point).forEach(fld => point[fld] = isNaN(point[fld]) ? point[fld] : parseFloat(point[fld]));
                            point.timestamp = `1970-01-01 ${point.timestamp}`;
                            point.altitude *= 100;
                            point.speed *= 100;
                            return point;
                        })
                }]
            }))
            ).catch(err => {
                if (retryCount++ < 3) {
                    this.buildRenderers(retryCount);
                } else {
                    reject(err);
                }
            });
    }

    parseCsv(resolve, retryCount, reject) {
        wfetch(`${httpPrefix}${this.props.folder}/${this.props.name}`)
            .then(resp => resp.text())
            .then(content => {
                var cols = content.split(/\n|\r\n/)[0].split(",");
                resolve(this.setState({
                    renderers: [{
                        name: "trip",
                        points: content.split(/\n|\r\n/)
                            .splice(1).map(ln => {
                                var ret = {};
                                ln.split(",").forEach((it, idx) => ret[cols[idx]] = isNaN(it) ? it : parseFloat(it));
                                return ret;
                            }).filter(item => item.timestamp && item.timestamp.match(/[0-9]{4}\/[0-9]{2}\/[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}/))
                    }]
                }));
            }).catch(err => {
                if (retryCount++ < 3) {
                    this.buildRenderers(retryCount);
                } else {
                    reject(err);
                }
            });
    }

    getRenderers() {
        if (!this.state?.renderers || !this.state.renderers.length) {
            return null;
        }
        return this.state.renderers.map(renderer => {
            switch (renderer.name) {
                case "trip":
                    return renderer.points.length?e(TripWithin,{key:"tripwithin",points:renderer.points,cache:this.props.cache}):null;

                case "loading":
                    return e('i',{className: "rendered reportbtn fa fa-spinner", key: "graphbtn", })
    
                default:
                    return null;
            }
        });
    }

    render() {
        return [e("a", { key:"filelink", href: `${httpPrefix}${this.props.folder}/${this.props.name}` }, this.props.name.split('/').reverse()[0]),this.getRenderers()];
    }
}

class SFile extends React.Component {
    render() {
        return  e("tr", { key: "tr", className: this.props.file.ftype }, [
            e("td", { key: "link" }, this.getLink(this.props.file)),
            e("td", { key: "size" }, this.props.file.ftype != "file" ? "" : this.props.file.size),
            e("td", { key: "delete" }, this.getDeleteLink())]);
    }

    getDeleteLink() {
        return this.props.path == "/" || this.props.file.name == ".." ? null : e("a", {
            key: "delete",
            href: "#",
            onClick: () => {
                wfetch(`${httpPrefix}/stat${this.props.path === "/" ? "" : this.props.path}/${this.props.file.name}`, {
                    method: 'post',
                    headers: {
                        ftype: this.props.file.ftype == "file" ? "file" : "directory",
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
        if (file.ftype == "folder") {
            return e("a", {
                key: file.name,
                href: "#",
                onClick: () => this.props.onChangeFolder ? this.props.onChangeFolder(`${file.folder || "/"}${file.name == ".." ? "" : "/" + file.name}`.replaceAll("//", "/")):null
            }, file.name);
        } else {
            return e(FileViewer,{key:file.name,cache:this.props.cache,registerFileWork:this.props.registerFileWork,...file});
        }
    }
}

class StorageViewer extends React.Component {
    constructor(props) {
        super(props);
        this.state = { 
            loaded: false, 
            path: "/", 
            files: null,
            cache:{images:{}},
            fileWork: [] 
        };
    }

    getSystemFolders() {
        return [
        this.state.path != "/" ?
            {
                ftype: "folder",
                name: "..",
                folder: dirname(this.state.path)
            }: null
        ];
    }

    GetFileStat(fileStatsToFetch) {
        if (this.mounted){
            fileStatsToFetch.forEach(fileToFetch => {
                this.registerFileWork(new Promise((resolve, reject) => {
                    wfetch(`${httpPrefix}/stat${fileToFetch.folder}/${fileToFetch.name}`, {
                        method: 'post'
                    }).then(data => {
                        data.json().then(jdata => {
                            fileToFetch.size = jdata.size;
                            resolve(this.setState({ loaded: true, files: this.state.files, total: this.state?.total + jdata.size }));
                        });
                    }).catch(ex => {
                        reject(ex);
                    });
                }));
            });
        }
    }

    fetchFiles() {
        if (window.location.host || httpPrefix){
            var abort = new AbortController();
        
            var quitItNow = setTimeout(() => abort.abort(), 8000);
            wfetch(`${httpPrefix}/files` + this.state.path, {
                method: 'post',
                signal: abort.signal
            }).then(data => {
                clearInterval(quitItNow);
                data.json()
                    .then(files => files.sort((f1, f2) => {
                        if (f1.ftype != f2.ftype) {
                            return f1.ftype == "folder" ? 1 : -1;
                        }
                        return f1.name == f2.name ? 0 : f1.name > f2.name ? 1 : -1;
                    })).then(files => {
                        if (this.mounted){
                            this.setState({ loaded: true, files: files, total: files.reduce((e1,e2) => e1 + e2.size,0) });
                            var fileStatsToFetch = files.filter(file => file.ftype == "file" && !file.size);
                            for (var idx = 0; idx < Math.min(3, fileStatsToFetch.length); idx++) {
                                if (fileStatsToFetch.length) {
                                    this.GetFileStat(fileStatsToFetch);
                                }
                            }
                        }
                    });
            }).catch(console.error);
        }
    }

    componentDidUpdate(prevProps, prevState) {
        if (!this.state.loaded) {
            this.state.loaded = true;
        }

        if (prevState && (prevState.path != this.state.path)) {
            this.fetchFiles();
        }
    }

    componentDidMount(){
        this.mounted=true;
        if (!this.state?.files) {
            this.fetchFiles();
        }
    }

    componentWillUnmount() {
        this.mounted=false;
    }

    SortTable(th) {
        var table,tbody;
        Array.from((tbody=(table=th.target.closest("div.file-table")).querySelector('tbody')).querySelectorAll('tr:nth-child(n+2)'))
                 .sort(comparer(Array.from(th.target.parentNode.children).indexOf(th.target), this.asc = !this.asc))
                 .forEach(tr => tbody.appendChild(tr));
                }

    getTableHeader() {
        return e("thead", { key: "head" }, e("tr", { key: "row" }, ["Name", "Size"].map(col => e("th", { key: col, onClick: this.SortTable.bind(this) }, col)).concat(e("th", { key: "op" }, "Op"))));
    }

    registerFileWork(fn) {
        this.setState({ fileWork: [...this.state.fileWork,fn] });
    }

    render() {
        if (!this.state?.files) {
            return e("div", { key: "Loading" }, "Loading......");
        } else {
            return e("div", { key: "Files", 
                className: `file-table ${this.state.loaded?"":"loading"}` }, 
                    e("table", { key: "table", className: "greyGridTable" }, [
                        e("caption", { key: "caption" }, this.state.path),
                        this.getTableHeader(),
                        e("tbody", { key: "body" }, 
                            this.getSystemFolders().concat(this.state.files).filter(file => file).map(file => 
                                e(SFile,{ 
                                    key: file.name, 
                                    file:file, 
                                    cache: this.state.cache,
                                    path:this.state.path,
                                    registerFileWork: this.registerFileWork.bind(this),
                                    onChangeFolder: (folder) => this.setState({path:folder, files:[]}),
                                    OnDelete: ()=>this.fetchFiles()
                                }))
                            ),
                        e("tfoot", { key: "tfoot" }, e("tr", { key: "trow" }, [
                            e("td", { key: "totallbl" }, "Total"), 
                            e("td", { key: "total" }, this.state.total)
                        ]))
                    ]));
        }
    }
}
