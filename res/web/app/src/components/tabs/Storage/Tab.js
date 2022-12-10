import {createElement as e, Component, lazy, Suspense} from 'react';
import {wfetch,dirname,comparer} from '../../../utils/utils'
import './Storage.css';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faSpinner } from '@fortawesome/free-solid-svg-icons/faSpinner';
import { SFile } from './SFile'

const UploadManager = lazy(() => import('./UploadManager'));

export default class StorageViewer extends Component {
    constructor(props) {
        super(props);
        this.state = { 
            httpPrefix:this.props.selectedDevice?.ip ? `${process.env.REACT_APP_API_URI}/${this.props.selectedDevice.config.devName}` : ".",
            loaded: false, 
            files: null,
            cache:{images:{}},
            fileWork: [] 
        };
    }

    getSystemFolders() {
        return [
        this.props.path !== "/" ?
            {
                ftype: "folder",
                name: "..",
                folder: dirname(this.props.path)
            }: null
        ];
    }

    GetFileStat(fileStatsToFetch) {
        if (this.mounted){
            fileStatsToFetch.forEach(fileToFetch => {
                this.registerFileWork(new Promise((resolve, reject) => {
                    wfetch(`${this.state.httpPrefix}/stat${fileToFetch.folder}/${fileToFetch.name}`, {
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
        let abort = new AbortController();
    
        let quitItNow = setTimeout(() => abort.abort(), 8000);
        wfetch(`${this.state.httpPrefix}/files` + this.props.path, {
            method: 'post',
            signal: abort.signal
        }).then(data => {
            clearInterval(quitItNow);
            data.json()
                .then(files => files.sort((f1, f2) => {
                    if (f1.ftype !== f2.ftype) {
                        return f1.ftype === "folder" ? 1 : -1;
                    }
                    return f1.name === f2.name ? 0 : f1.name > f2.name ? 1 : -1;
                })).then(files => {
                    if (this.mounted){
                        this.setState({ loaded: true, files: files, total: files.reduce((e1,e2) => e1 + e2.size,0) });
                        let fileStatsToFetch = files.filter(file => file.ftype === "file" && !file.size);
                        for (let idx = 0; idx < Math.min(3, fileStatsToFetch.length); idx++) {
                            if (fileStatsToFetch.length) {
                                this.GetFileStat(fileStatsToFetch);
                            }
                        }
                    }
                });
        }).catch(console.error);
    }

    componentDidUpdate(prevProps, prevState) {
        if (!this.state.loaded) {
            this.state.loaded = true;
        }

        if (prevProps && (prevProps.path !== this.props.path)) {
            this.fetchFiles();
        }

        if (prevProps?.selectedDevice !== this.props.selectedDevice) {
            if (this.props.selectedDevice?.ip) {
                this.setState({httpPrefix:`${process.env.REACT_APP_API_URI}/${this.props.selectedDevice.config.devName}`});
            } else {
                this.setState({httpPrefix:"."});
            }
        }
        if (prevState.httpPrefix !== this.state.httpPrefix) {
            if ((this.state.path || '/') === "/") {
                this.fetchFiles();
            } else {
                this.props.onChangeFolder&&this.props.onChangeFolder("/");
            }
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
        let tbody;
        Array.from((tbody=(th.target.closest("div.file-table")).querySelector('tbody')).querySelectorAll('tr:nth-child(n+2)'))
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
            return <FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />;
        } else {
            return e("div", { key: "Files", 
                className: `file-table ${this.state.loaded?"":"loading"}` }, 
                    e("table", { key: "table", className: "greyGridTable" }, [
                        this.getTableCaption(),
                        this.getTableHeader(),
                        this.getTableBody(),
                        this.getTableFooter()
                    ]));
        }
    }

    getTableFooter() {
        return e("tfoot", { key: "tfoot" }, e("tr", { key: "trow" }, [
            e("td", { key: "totallbl" }, "Total"),
            e("td", { key: "total" }, this.state.total)
        ]));
    }

    getTableBody() {
        return <tbody>
            {this.getSystemFolders().concat(this.state.files)
                                    .filter(file => file)
                                    .map(file => 
                    <SFile
                        file={file}
                        cache={this.state.cache}
                        selectedDevice={this.props.selectedDevice}
                        path= {this.props.path}
                        registerFileWork= {this.registerFileWork.bind(this)}
                        onChangeFolder= {(folder) => this.props.onChangeFolder(folder)}
                        OnDelete= {() => this.fetchFiles()}></SFile>)}
        </tbody>
    }

    getTableCaption() {
        return <caption>
                  <div className="file-operations">
                      {this.props.path}
                      <Suspense fallback={<FontAwesomeIcon className='fa-spin-pulse' icon={faSpinner} />}>
                        <UploadManager 
                                selectedDevice={this.props.selectedDevice}
                                destination={this.props.path}
                                target="file">
                        </UploadManager>
                        <UploadManager 
                                selectedDevice={this.props.selectedDevice}
                                destination={this.props.path}
                                target="folder">
                        </UploadManager>
                      </Suspense>
                  </div>
                </caption>;
    }
}
