import {createElement as e, Component} from 'react';
import {chipRequest} from '../../../utils/utils'

export default class TripViewer extends Component {
    constructor(props) {
        super(props);
        this.state={
            cache:this.props.cache,
            zoomlevel:15,
            latitude:0,
            longitude:0
        };
        this.testing = false;
    }

    componentDidMount() {
        this.getTripTiles();
        this.firstRender = true;
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        if (!prevProps.points || !prevProps.points.length || !prevProps.points.length || prevProps.points.length !== this.props.points.length){
            this.firstRender=true;
        }
        this.mapCanvas = this.mapWidget.getContext("2d");                    
        this.tripCanvas = this.tripWidget.getContext("2d");                    
        this.popupCanvas = this.popupWidget.getContext("2d");                    
        this.popupWidget.addEventListener("mousemove", this.mouseEvent.bind(this));
        window.requestAnimationFrame(this.drawMap.bind(this));
    }

    drawMap() {
        if (!this.mapWidget || !this.props.points || !this.props.points.length || this.state.closed){
            return;
        } else {
            this.drawTripVisibleTiles();
            this.drawTrip();
            this.drawPointPopup();
        }
    }
   
    getTripTiles() {
        let lastPoint=undefined;
        this.points=this.props.points.map(this.pointToCartesian.bind(this))
                              .filter(point => point.latitude && point.longitude)
                              .filter(point => point.latitude > -90 && point.latitude < 90)
                              .filter(point => point.longitude > -180 && point.longitude < 180)
                              .filter(point => {
                                  if (lastPoint === undefined) {
                                      lastPoint = point;
                                      return true;
                                  }
                                  if (Math.abs(lastPoint.longitude - point.longitude) > 1.0) {
                                      return false;
                                  }
                                  lastPoint = point;
                                  return true;
                              });
        this.trip={
                leftTile: this.points.reduce((a,b)=>a<b.tileX?a:b.tileX,99999),
                rightTile: this.points.reduce((a,b)=>a>b.tileX?a:b.tileX,-99999),
                bottomTile: this.points.reduce((a,b)=>a<b.tileY?a:b.tileY,99999),
                topTile: this.points.reduce((a,b)=>a>b.tileY?a:b.tileY,-99999)
        };
        this.maxXTiles= Math.ceil(window.innerWidth/256)+1;
        this.maxYTiles= Math.ceil(window.innerHeight/256)+1;
        this.trip.XTiles = this.trip.rightTile-this.trip.leftTile+1;
        this.trip.YTiles = this.trip.topTile-this.trip.bottomTile+1;
        this.margin = {
            left: this.maxXTiles>this.trip.XTiles?Math.floor((this.maxXTiles-this.trip.XTiles)/2):0,
            bottom: this.maxYTiles>this.trip.YTiles?Math.floor((this.maxYTiles-this.trip.YTiles)/2):0
        };
        this.margin.right=this.maxXTiles>this.trip.XTiles?this.maxXTiles-this.trip.XTiles-this.margin.left:0;
        this.margin.top= this.maxYTiles>this.trip.YTiles?this.maxYTiles-this.trip.YTiles-this.margin.bottom:0;
        this.leftTile=Math.max(0,this.trip.leftTile-this.margin.left);
        this.rightTile=this.trip.rightTile+this.margin.right;
        this.bottomTile=Math.max(0,this.trip.bottomTile-this.margin.bottom);
        this.topTile=this.trip.topTile + this.margin.top;
        this.leftlatitude = this.points.reduce((a,b)=>a<b.latitude?a:b.latitude,99999);
        this.rightlatitude = this.points.reduce((a,b)=>a>b.latitude?a:b.latitude,-99999);
        this.toplongitude = this.points.reduce((a,b)=>a<b.longitude?a:b.longitude,99999);
        this.bottomlongitude = this.points.reduce((a,b)=>a>b.longitude?a:b.longitude,-99999);
        this.latitude= this.state.leftlatitude+(this.state.rightlatitude-this.state.leftlatitude)/2;
        this.longitude= this.state.toplongitude+(this.state.bottomlongitude-this.state.toplongitude)/2;
        this.windowTiles = {
            center: {
                tileX: this.lon2tile(this.longitude, this.state.zoomlevel),
                tileY: this.lat2tile(this.latitude, this.state.zoomlevel),
            },
            windowTiles: {
                leftTile: this.lon2tile(this.longitude, this.state.zoomlevel) - Math.floor(this.maxXTiles/2),
                rightTile: this.lon2tile(this.longitude, this.state.zoomlevel) + Math.floor(this.maxXTiles/2),
                bottomTile: this.lat2tile(this.latitude, this.state.zoomlevel) - Math.floor(this.maxYTiles/2),
                topTile: this.lat2tile(this.latitude, this.state.zoomlevel) + Math.floor(this.maxYTiles/2)
            }
        }
        // this.windowTiles.leftTile = this.state.windowTiles.center.tileX - Math.floor(this.maxXTiles/2);
        // this.windowTiles.rightTile = this.state.windowTiles.center.tileX + Math.floor(this.maxXTiles/2);
        // this.windowTiles.bottomTile = this.state.windowTiles.center.tileY - Math.floor(this.maxYTiles/2);
        // this.windowTiles.topTile = this.state.windowTiles.center.tileY + Math.floor(this.maxYTiles/2);
        // this.windowTiles.XTiles = this.state.windowTiles.rightTile-this.state.windowTiles.leftTile+1;
        // this.windowTiles.YTiles = this.state.windowTiles.topTile-this.state.windowTiles.bottomTile+1;
        this.setState(this.state);
    }

    drawTripVisibleTiles() {
        return new Promise(async (resolve,reject)=>{
            this.popupWidget.width=(this.rightTile-this.leftTile)*256;
            this.popupWidget.height=(this.topTile-this.state.bottomTile)*256;
            this.mapWidget.width=(this.rightTile-this.leftTile)*256;
            this.mapWidget.height=(this.topTile-this.state.bottomTile)*256;
            this.tripWidget.width=(this.rightTile-this.leftTile)*256;
            this.tripWidget.height=(this.topTile-this.state.bottomTile)*256;

            this.mapCanvas.fillStyle = "black";
            this.mapCanvas.fillRect(0,0,window.innerWidth,window.innerHeight);
            let wasFirstRender=this.firstRender;
            
            if (this.firstRender) {
                this.firstRender=false;
                const elementRect = this.mapWidget.getBoundingClientRect();
                this.mapWidget.parentElement.scrollTo((elementRect.width/2)-512, (elementRect.height/2)-256);
            }

            for (let tileX = this.windowTiles.leftTile; tileX <= this.state.windowTiles.rightTile; tileX++) {
                for (let tileY = this.windowTiles.bottomTile; tileY <= this.state.windowTiles.topTile; tileY++) {
                    if (!this.props.cache.images[this.state.zoomlevel] ||
                        !this.props.cache.images[this.zoomlevel][tileX] || 
                        !this.props.cache.images[this.zoomlevel][tileX][tileY]){
                        await this.addTileToCache(tileX, tileY).catch(reject);
                    } else {
                        await this.getTileFromCache(tileX, tileY).catch(reject);
                    }        
                }
            }
            if (wasFirstRender) {
                new Promise((resolve,reject)=>{
                    for (let tileX = this.trip.leftTile; tileX <= this.trip.rightTile; tileX++) {
                        for (let tileY = this.trip.bottomTile; tileY <= this.trip.topTile; tileY++) {
                            if (!this.props.cache.images[this.state.zoomlevel] ||
                                !this.props.cache.images[this.zoomlevel][tileX] || 
                                !this.props.cache.images[this.zoomlevel][tileX][tileY]){
                                this.addTileToCache(tileX, tileY).catch(reject);
                            }        
                        }
                    }
                    resolve();
                });
            }
            resolve();
        });
    }

    drawPointPopup(){
        if (this.focused) {
            this.popupCanvas.clearRect(0,0,this.popupWidget.width,this.popupWidget.height);
            this.popupCanvas.fillStyle = "transparent";
            this.popupCanvas.fillRect(0,0,window.innerWidth,window.innerHeight);
    
            this.popupCanvas.font = "12px Helvetica";
            let txtSz = this.popupCanvas.measureText(new Date(`${this.focused.timestamp} UTC`).toLocaleString());
            let props =  Object.keys(this.focused)
                               .filter(prop => prop !== "timestamp" && !prop.match(/.*tile.*/i));

            let boxHeight=50 + (9*props.length);
            let boxWidth=txtSz.width+10;
            this.popupCanvas.strokeStyle = 'green';
            this.popupCanvas.shadowColor = '#00ffff';
            this.popupCanvas.fillStyle = "#000000";
            this.popupCanvas.lineWidth = 1;
            this.popupCanvas.shadowBlur = 2;
            this.popupCanvas.beginPath();
            this.popupCanvas.rect(this.getClientX(this.focused),this.getClientY(this.focused)-boxHeight,boxWidth,boxHeight);
            this.popupCanvas.fill();
            this.popupCanvas.stroke();

            this.popupCanvas.fillStyle = 'rgba(00, 0, 0, 1)';
            this.popupCanvas.strokeStyle = '#000000';

            this.popupCanvas.beginPath();
            let ypos = this.getClientY(this.focused)-boxHeight+txtSz.actualBoundingBoxAscent+3;
            let xpos = this.getClientX(this.focused)+3;

            this.popupCanvas.strokeStyle = '#97ea44';
            this.popupCanvas.shadowColor = '#ffffff';
            this.popupCanvas.fillStyle = "#97ea44";
            this.popupCanvas.lineWidth = 1;
            this.popupCanvas.shadowBlur = 2;
            this.popupCanvas.fillText(new Date(`${this.focused.timestamp} UTC`).toLocaleString(),xpos,ypos);
            ypos+=5;

            ypos+=txtSz.actualBoundingBoxAscent+3;
            props.forEach(prop => {
                let propVal = this.getPropValue(prop,this.focused[prop]);
                this.popupCanvas.strokeStyle = 'aquamarine';
                this.popupCanvas.shadowColor = '#ffffff';
                this.popupCanvas.fillStyle = "aquamarine";
                this.popupCanvas.fillText(`${prop}: `,xpos,ypos);
                txtSz = this.popupCanvas.measureText(propVal);
                this.popupCanvas.strokeStyle = '#97ea44';
                this.popupCanvas.shadowColor = '#ffffff';
                this.popupCanvas.fillStyle = "#97ea44";
                this.popupCanvas.fillText(propVal,(xpos+(boxWidth-txtSz.width)-5),ypos);
                ypos+=txtSz.actualBoundingBoxAscent+3;
            });

            this.popupCanvas.stroke();
        }
    }

    drawTrip() {
        return new Promise((resolve,reject)=>{
            let firstPoint = this.points[0];
            if (firstPoint){
                this.tripCanvas.fillStyle = "transparent";
                this.tripCanvas.fillRect(0,0,window.innerWidth,window.innerHeight);
    
                this.tripCanvas.strokeStyle = '#00ffff';
                this.tripCanvas.shadowColor = '#00ffff';
                this.tripCanvas.fillStyle = "#00ffff";
                this.tripCanvas.lineWidth = 2;
                this.tripCanvas.shadowBlur = 2;
        
                this.tripCanvas.beginPath();
                this.tripCanvas.moveTo(this.getClientX(firstPoint), this.getClientY(firstPoint));
                this.points.forEach(point => {
                    this.tripCanvas.lineTo(this.getClientX(point), this.getClientY(point));
                });
                this.tripCanvas.stroke();
                this.tripCanvas.strokeStyle = '#0000ff';
                this.tripCanvas.shadowColor = '#0000ff';
                this.tripCanvas.fillStyle = "#0000ff";
                this.points.forEach(point => {
                    this.tripCanvas.beginPath();
                    this.tripCanvas.arc(((point.tileX - this.leftTile) * 256) + (256 * point.posTileX), ((point.tileY - this.bottomTile) * 256) + (256 * point.posTileY), 5, 0, 2 * Math.PI);
                    this.tripCanvas.stroke();
                });
                resolve(this.state);
            } else {
                reject({error:"no points"});
            }
        });
    }

    mouseEvent(event) {
        let margin = 10;
        let focused = this.points.find(point => 
            (this.getClientX(point) >= (event.offsetX - margin)) &&
            (this.getClientX(point) <= (event.offsetX + margin)) &&
            (this.getClientY(point) >= (event.offsetY - margin)) &&
            (this.getClientY(point) <= (event.offsetY + margin)));
        if (focused !== this.focused){
            this.focused=focused;
            window.requestAnimationFrame(this.drawPointPopup.bind(this));
        }
    }

    getPropValue(name,val) {
        if (name === "speed") {
            return `${Math.floor(1.852*val/100.0)} km/h`;
        }
        if (name === "altitude") {
            return `${Math.floor(val/100)}m`;
        }
        if ((name === "longitude") || (name === "latitude")) {
            return val;
        }
        return Math.round(val);
    }

    addTileToCache(tileX,tileY) {
        return new Promise((resolve,reject) => {
            this.getTile(tileX, tileY, 0).then(imgData => {
                let tileImage = new Image();
                tileImage.posX = tileX - this.leftTile;
                tileImage.posY = tileY - this.bottomTile;
                tileImage.src = (window.URL || window.webkitURL).createObjectURL(imgData);
                if (!this.props.cache.images[this.state.zoomlevel]) {
                    this.props.cache.images[this.state.zoomlevel] = {};
                }
                if (!this.props.cache.images[this.zoomlevel][tileX]) {
                    this.props.cache.images[this.zoomlevel][tileX] = {};
                }
                this.props.cache.images[this.zoomlevel][tileX][tileY] = tileImage;

                tileImage.onload = (elem) => {
                    resolve(this.mapCanvas.drawImage(tileImage, tileImage.posX * tileImage.width, tileImage.posY * tileImage.height));
                };
            }).catch(reject);
        });
    }

    getTileFromCache(tileX,tileY) {
        return new Promise((resolve,reject) => {
            let tileImage = this.props.cache.images[this.zoomlevel][tileX][tileY];
            try {
                resolve(this.mapCanvas.drawImage(tileImage, tileImage.posX * tileImage.width, tileImage.posY * tileImage.height));
            } catch (err) {
                this.props.cache.images[this.zoomlevel][tileX][tileY] = null;
                reject(err);
            }
        });
    }

    downloadTile(tileX,tileY) {
        return new Promise((resolve,reject) => {
            let newImg;
            chipRequest(`https://tile.openstreetmap.de/${this.zoomlevel}/${tileX}/${tileY}.png`,{skipHttpPrefix:true})
                .then(resp => resp.blob())
                .then(imgData => chipRequest(`/sdcard/web/tiles/${this.state.zoomlevel}/${tileX}/${tileY}.png`,{
                    method: 'put',
                    body: (newImg=imgData)
                }).then(resolve(newImg)).catch(resolve(newImg)))
                  .catch(reject);
        })
    }

    drawWatermark(tileX,tileY) {
        return new Promise((resolve,reject) => {
            return this.mapWidget.toBlob(resolve);
        })
    }

    getTile(tileX,tileY, retryCount) {
        return new Promise((resolve,reject)=>{
            if (this.testing) {
                return resolve(this.drawWatermark(tileX,tileY));
            }
            chipRequest(`/sdcard/web/tiles/${this.state.zoomlevel}/${tileX}/${tileY}.png`)
                .then(resp => resolve(resp.status >= 300? this.downloadTile(tileX,tileY):resp.blob()))
                .catch(err => {
                    if (retryCount > 3) {
                        reject({error:`Error in downloading ${this.zoomlevel}/${tileX}/${tileY}`});
                    } else {
                        resolve(this.getTile(tileX,tileY,retryCount++));
                    }
                });
        });
    }

    getClientY(firstPoint) {
        return ((firstPoint.tileY - this.bottomTile) * 256) + (256 * firstPoint.posTileY);
    }

    getClientX(firstPoint) {
        return ((firstPoint.tileX - this.leftTile) * 256) + (256 * firstPoint.posTileX);
    }

    lon2tile(lon) { 
        return Math.floor(this.lon2x(lon)); 
    }
    
    lat2tile(lat)  { 
        return Math.floor(this.lat2y(lat));
    }

    lon2x(lon) { 
        return (lon+180)/360*Math.pow(2,this.state.zoomlevel); 
    }
    
    lat2y(lat)  { 
        return (1-Math.log(Math.tan(lat*Math.PI/180) + 1/Math.cos(lat*Math.PI/180))/Math.PI)/2 *Math.pow(2,this.state.zoomlevel); 
    }

    tile2long(x,z) {
        return (x/Math.pow(2,z)*360-180);
    }

    tile2lat(y,z) {
        let n=Math.PI-2*Math.PI*y/Math.pow(2,z);
        return (180/Math.PI*Math.atan(0.5*(Math.exp(n)-Math.exp(-n))));
    }
    
    pointToCartesian(point) {
        return {
            tileX: this.lon2tile(point.longitude, this.state.zoomlevel),
            tileY: this.lat2tile(point.latitude, this.state.zoomlevel),
            posTileX: this.lon2x(point.longitude,this.zoomlevel) - this.lon2tile(point.longitude, this.state.zoomlevel),
            posTileY: this.lat2y(point.latitude,this.zoomlevel) - this.lat2tile(point.latitude, this.state.zoomlevel),
            ...point
        };
    }

    closeIt(){
        this.setState({closed:true});
        if (this.props.onClose) {
            this.props.onClose();
        }
    }

    render(){
        return e("div",{key:"trip",className:`lightbox ${this.state?.closed?"closed":"opened"}`},[
            e("div",{key:"control"},[
                e("div",{key:"tripHeader",className:"tripHeader"},[
                    `Trip with ${this.props.points.length} points`,
                    e("br",{key:"daterange"}),
                    `From ${new Date(`${this.props.points[0].timestamp} UTC`).toLocaleString()} `,
                    `To ${new Date(`${this.props.points[this.props.points.length-1].timestamp} UTC`).toLocaleString()} Zoom:`,
                    e("select",{
                        key:"zoom",
                        onChange: elem=>this.setState({zoomlevel:elem.target.value}),
                        value: this.state.zoomlevel
                    },
                        Array.from(Array(18).keys()).map(zoom => e("option",{key:zoom},zoom+1))
                    )
                ]),
                e("span",{key:"close",className:"close",onClick:this.closeIt.bind(this)},"X")
            ]),
            e("div",{key:"map",className:"trip"},[
                e("canvas",{
                    key:"mapWidget",
                    className:"mapCanvas",
                    ref: (elem) => this.mapWidget = elem
                }),
                e("canvas",{
                    key:"tripWidget",
                    className:"mapCanvas",
                    ref: (elem) => this.tripWidget = elem
                }),
                e("canvas",{
                    key:"popupWidget",
                    className:"mapCanvas",
                    ref: (elem) => this.popupWidget = elem
                })
            ])
        ]);
    }
}
