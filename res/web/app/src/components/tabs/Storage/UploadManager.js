import { createRef, Component} from 'react';
import { Button, ClickAwayListener, Tooltip, Zoom } from '@mui/material';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome'
import { faFolderPlus, faFileArrowDown } from '@fortawesome/free-solid-svg-icons'

import { chipRequest } from '../../../utils/utils'

export default class UploadManager extends Component {
    constructor(props) {
        super(props);
        this.state={
            files: [],
            processed: [],
            failed: [],
            skipped: [],
            overriten: []
        };
        this.hiddenFileInput = createRef();
    }

    uploadIt(event) {
        let files=Array.from(event.target.files).map(file => 
            new Promise((resolve,reject)=>{
                let reader = new FileReader();
                reader.readAsArrayBuffer(file);
                reader.onerror = reject;
                reader.onload = data => resolve({
                    srcFile: `${file.webkitRelativePath}`,
                    destFile: `${this.props.destination}/${file.webkitRelativePath === "" ? file.name : file.webkitRelativePath}`,
                    content: reader.result,
                    size: file.size
                })
            })
        );

        this.setState({
            activeFile: undefined,
            files: files,
            numFiles: files.length,
            processed: [],
            failed: [],
            skipped: [],
            overriten: []
        });
    }

    getInput() {
        if (this.props.target === "file"){
            return <input type="file"
                ref={this.hiddenFileInput}
                onChange={this.uploadIt.bind(this)}
                style={{ display: 'none' }} 
                multiple/>;
        } else {
            return <input type="file"
                ref={this.hiddenFileInput}
                directory="" 
                webkitdirectory=""
                onChange={this.uploadIt.bind(this)}
                style={{ display: 'none' }} 
                multiple/>;
        }
    }

    getUploadButon() {
        return <div>
            {this.getInput()}
            <Button
                className="file-operation"
                onClick={_ => this.hiddenFileInput.current.click()}>
                <FontAwesomeIcon icon={this.props.target === "file" ? faFileArrowDown : faFolderPlus}></FontAwesomeIcon>
            </Button>
        </div>;
    }

    getUploadProgress() {
        return (this.state.processed.length + this.state.failed.length * 1.0) / (this.state.numFiles);
    }

    hasDownloads() {
        return (this.state.processed.length + this.state.failed.length + this.state.files.length) > 0;
    }

    getUploadProgressTooltip() {
        const handleTooltipClose = () => {
            if (this.getUploadProgress() === 1.0) {
                this.setState({
                    files: [],
                    processed: [],
                    failed: [],
                    skipped: [],
                    overriten: []
                });
            }
        };
    
        return <ClickAwayListener onClickAway={handleTooltipClose}>
                    <Tooltip
                        TransitionComponent={Zoom}
                        PopperProps={{disablePortal: true}}
                        onClose={handleTooltipClose}
                        open={true}
                        placement="left"
                        disableFocusListener
                        disableHoverListener
                        disableTouchListener
                        title={<div className='uploadSummary'>
                                {this.state.activeFile ? <div className="debug">{`sending ${this.state.activeFile.size} bytes`}</div> : undefined}
                                <div className="warning">{this.state.files.length + (this.state.activeFile ? 1 : 0)} To go<br/></div>
                                <div className="info">{this.state.processed.filter(file=>!this.state.skipped.find(sfile=>sfile === file)).length} Processed {this.state.overriten.length ? `${this.state.overriten.length} overriten`:""}<br/></div>
                                {this.state.skipped.length ? <div className="info">{this.state.skipped.length} Skipped<br/></div>:undefined}
                                {this.state.failed.length ? 
                                    <div className="error">{this.state.failed.length} failed 
                                        {this.state.activeFile?undefined:<Button 
                                                                            title={`Retry uploading ${this.state.failed.map(file=>file.file).join(',')}`}
                                                                            onClick={this.setState({files:this.state.failed.map(fail => fail.file),failed:[],numFiles:this.state.failed.length})}><FontAwesomeIcon icon="user-doctor"></FontAwesomeIcon>
                                                                        </Button>}
                                    </div> : undefined}
                            </div>}
                        arrow
                    ><div className="upload-progress">{Math.floor(this.getUploadProgress()*100)}%</div></Tooltip>
                </ClickAwayListener>;
    }

    componentDidUpdate(prevProps, prevState) {
        if (this.state.files.length && !this.state.activeFile) {
            try{
              this.state.files.shift()?.then(file=>this.setState({activeFile:file}));
            } catch(ex) {
                console.error(ex);
            }
        }
        if (this.state.activeFile && (this.state.activeFile !== prevState?.activeFile)) {
            new Promise((resolve,reject) => {
                chipRequest(`/stat${this.state.activeFile.destFile}`, {
                    method: 'post'
                }).then(response => {
                    if (response.status === 200) {
                        response.json()
                                .then(dfile => {
                                    if (dfile.size === this.state.activeFile.size){
                                        this.setState({skipped: [ ...this.state.skipped, this.state.activeFile]});
                                        resolve(this.state.activeFile.destFile);
                                    } else {
                                        chipRequest(`/stat${this.state.activeFile.destFile}`, {
                                            method: 'post',
                                            headers: {
                                                ftype: "file",
                                                operation: "delete"
                                            }
                                        }).then(this.setState({overriten: [ ...this.state.overriten, this.state.activeFile]}))
                                          .then(this.uploadCurrentFile())
                                          .then(resolve)
                                          .catch(reject)
                                    }
                                }).catch(err => reject({file: this.state.activeFile, result: err}));
                    } else {
                        this.uploadCurrentFile().then(resolve).catch(reject)
                    }
                }).catch(err => reject({file: this.state.activeFile, result: err}));
            }).then(file => this.setState({processed:[...this.state.processed,file]}))
              .catch(err => this.setState({failed: [ ...this.state.failed, {file: this.state.activeFile, result: err}]}))
              .finally(() => this.state.files.length ? this.state.files.shift().then(file => this.setState({activeFile: file})) : 
                                                       this.setState({activeFile:undefined}));
        }
    }

    uploadCurrentFile() {
        return new Promise((resolve,reject)=>{
            try {
                chipRequest(`${this.state.activeFile.destFile}.dwnld`,{
                    method: 'put',
                    body: this.state.activeFile.content,
                    headers: new Headers({'content-type': 'application/octet-stream',
                                        'filename':`${this.state.activeFile.destFile}`})
                }).then(data => data.text())
                .then(res => res === "OK" ? resolve(this.state.activeFile) : 
                                            reject({file: this.state.activeFile, result: res}))
                .catch((err)=>reject({file: this.state.activeFile, result: err}));
            } catch (ex) {
                reject({file:this.state.activeFile, result: ex});
            }
        });
    }

    render(){
        return this.hasDownloads() ? this.getUploadProgressTooltip() : this.getUploadButon();
    }
}
