class TripViewer extends React.Component {
    constructor(props) {
        super(props);
        this.state={
            cache:this.props.cache,
            zoomlevel:15
        };
    }
    componentDidMount() {
        this.mounted=true;
        this.needsRefresh=true;
        this.canvas = this.widget.getContext("2d");                    
        this.widget.addEventListener("mousemove", this.mouseEvent.bind(this));
        window.requestAnimationFrame(this.drawMap.bind(this));
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        this.canvas = this.widget.getContext("2d");                    
        this.widget.addEventListener("mousemove", this.mouseEvent.bind(this));
        this.needsRefresh=true;
    }

    drawMap() {
        if (this.needsRefresh){
            this.needsRefresh=false;
            if (!this.mounted || !this.widget || !this.props.points || !this.props.points.length){
                return;
            } else {
                this.getTripTiles()
                this.drawTripTiles()
                    .then(this.drawTrip.bind(this))
                    .then(this.drawPointPopup.bind(this))
                    .then(ret=>window.requestAnimationFrame(this.drawMap.bind(this)))
                    .catch(err => {
                        console.error(err);
                        window.requestAnimationFrame(this.drawMap.bind(this));
                    });
            }
        } else {
            window.requestAnimationFrame(this.drawMap.bind(this));
        }
    }
   
    getTripTiles() {
        var points = this.props.points.map(this.pointToCartesian.bind(this));
        this.tiles= {
            points:points,
            trip:{
                leftTile: points.reduce((a,b)=>a<b.tileX?a:b.tileX,99999),
                rightTile: points.reduce((a,b)=>a>b.tileX?a:b.tileX,0),
                bottomTile: points.reduce((a,b)=>a<b.tileY?a:b.tileY,99999),
                topTile: points.reduce((a,b)=>a>b.tileY?a:b.tileY,0)
            },
            maxXTiles: Math.ceil(window.innerWidth/256),
            maxYTiles: Math.ceil(window.innerHeight/256)
        }
        this.tiles.trip.XTiles = this.tiles.trip.rightTile-this.tiles.trip.leftTile+1;
        this.tiles.trip.YTiles = this.tiles.trip.bottomTile-this.tiles.trip.bottomTile+1;
        this.tiles.margin = {
            left: this.tiles.maxXTiles>this.tiles.trip.XTiles?Math.floor((this.tiles.maxXTiles-this.tiles.trip.XTiles)/2):0,
            bottom: this.tiles.maxYTiles>this.tiles.trip.YTiles?Math.floor((this.tiles.maxYTiles-this.tiles.trip.YTiles)/2):0
        };
        this.tiles.margin.right=this.tiles.maxXTiles>this.tiles.trip.XTiles?this.tiles.maxXTiles-this.tiles.trip.XTiles-this.tiles.margin.left:0;
        this.tiles.margin.top= this.tiles.maxYTiles>this.tiles.trip.YTiles?this.tiles.maxYTiles-this.tiles.trip.YTiles-this.tiles.margin.bottom:0;
        this.tiles.leftTile=this.tiles.trip.leftTile-this.tiles.margin.left;
        this.tiles.rightTile=this.tiles.trip.rightTile+this.tiles.margin.right;
        this.tiles.bottomTile=this.tiles.trip.bottomTile-this.tiles.margin.bottom;
        this.tiles.topTile=this.tiles.trip.topTile + this.tiles.margin.top;
    }

    drawTripTiles() {
        this.widget.width=(this.tiles.rightTile-this.tiles.leftTile)*256;
        this.widget.height=(this.tiles.topTile-this.tiles.bottomTile)*256;
        this.canvas.fillStyle = "black";
        this.canvas.fillRect(0,0,window.innerWidth,window.innerHeight);
        var tiles=[];
        for (var tileX = this.tiles.leftTile; tileX <= this.tiles.rightTile; tileX++) {
            for (var tileY = this.tiles.bottomTile; tileY <= this.tiles.topTile; tileY++) {
                tiles.unshift({tileX:tileX, tileY:tileY});
            }
        }
        return Promise.all(Array.from(Array(Math.min(3,tiles.length)).keys()).map(unused => this.drawTile(tiles)));
    }

    drawPointPopup(){
        if (this.focused) {
            this.canvas.font = "12px Helvetica";
            var txtSz = this.canvas.measureText(new Date(`${this.focused.timestamp} UTC`).toLocaleString());
            var props =  Object.keys(this.focused)
                               .filter(prop => prop != "timestamp" && !prop.match(/.*tile.*/i));

            var boxHeight=50 + (9*props.length);
            var boxWidth=txtSz.width+10;
            this.canvas.strokeStyle = 'green';
            this.canvas.shadowColor = '#00ffff';
            this.canvas.fillStyle = "#000000";
            this.canvas.lineWidth = 1;
            this.canvas.shadowBlur = 2;
            this.canvas.beginPath();
            this.canvas.rect(this.getClientX(this.focused),this.getClientY(this.focused)-boxHeight,boxWidth,boxHeight);
            this.canvas.fill();
            this.canvas.stroke();

            this.canvas.fillStyle = 'rgba(00, 0, 0, 1)';
            this.canvas.strokeStyle = '#000000';

            this.canvas.beginPath();
            var ypos = this.getClientY(this.focused)-boxHeight+txtSz.actualBoundingBoxAscent+3;
            var xpos = this.getClientX(this.focused)+3;

            this.canvas.strokeStyle = '#97ea44';
            this.canvas.shadowColor = '#ffffff';
            this.canvas.fillStyle = "#97ea44";
            this.canvas.lineWidth = 1;
            this.canvas.shadowBlur = 2;
            this.canvas.fillText(new Date(`${this.focused.timestamp} UTC`).toLocaleString(),xpos,ypos);
            ypos+=5;

            ypos+=txtSz.actualBoundingBoxAscent+3;
            props.forEach(prop => {
                var propVal = this.getPropValue(prop,this.focused[prop]);
                this.canvas.strokeStyle = 'aquamarine';
                this.canvas.shadowColor = '#ffffff';
                this.canvas.fillStyle = "aquamarine";
                this.canvas.fillText(`${prop}: `,xpos,ypos);
                txtSz = this.canvas.measureText(propVal);
                this.canvas.strokeStyle = '#97ea44';
                this.canvas.shadowColor = '#ffffff';
                this.canvas.fillStyle = "#97ea44";
                this.canvas.fillText(propVal,(xpos+(boxWidth-txtSz.width)-5),ypos);
                ypos+=txtSz.actualBoundingBoxAscent+3;
            });

            this.canvas.stroke();
        }
    }

    drawTrip() {
        return new Promise((resolve,reject)=>{
            var firstPoint = this.tiles.points[0];
            if (firstPoint){
                this.canvas.strokeStyle = '#00ffff';
                this.canvas.shadowColor = '#00ffff';
                this.canvas.fillStyle = "#00ffff";
                this.canvas.lineWidth = 2;
                this.canvas.shadowBlur = 2;
        
                this.canvas.beginPath();
                this.canvas.moveTo(this.getClientX(firstPoint), this.getClientY(firstPoint));
                this.tiles.points.forEach(point => {
                    this.canvas.lineTo(this.getClientX(point), this.getClientY(point));
                });
                this.canvas.stroke();
                this.canvas.strokeStyle = '#0000ff';
                this.canvas.shadowColor = '#0000ff';
                this.canvas.fillStyle = "#0000ff";
                this.tiles.points.forEach(point => {
                    this.canvas.beginPath();
                    this.canvas.arc(((point.tileX - this.tiles.leftTile) * 256) + (256 * point.posTileX), ((point.tileY - this.tiles.bottomTile) * 256) + (256 * point.posTileY), 5, 0, 2 * Math.PI);
                    this.canvas.stroke();
                });
                resolve(this.tiles);
            } else {
                reject({error:"no points"});
            }
        });
    }

    mouseEvent(event) {
        var margin = 10;
        var focused = this.tiles.points.find(point => 
            (this.getClientX(point) >= (event.offsetX - margin)) &&
            (this.getClientX(point) <= (event.offsetX + margin)) &&
            (this.getClientY(point) >= (event.offsetY - margin)) &&
            (this.getClientY(point) <= (event.offsetY + margin)));
        this.needsRefresh = focused != this.focused;
        this.focused=focused;
    }

    getPropValue(name,val) {
        if (name == "speed") {
            return `${Math.floor(1.852*val/100.0)} km/h`;
        }
        if (name == "altitude") {
            return `${Math.floor(val/100)}m`;
        }
        if ((name == "longitude") || (name == "latitude")) {
            return val;
        }
        return Math.round(val);
    }

    drawTile(tiles) {
        return new Promise((resolve,reject) => {
            var curTile = tiles.pop();
            if (curTile){
                if (!this.state.cache.images[this.state.zoomlevel] ||
                    !this.state.cache.images[this.state.zoomlevel][curTile.tileX] || 
                    !this.state.cache.images[this.state.zoomlevel][curTile.tileX][curTile.tileY]){
                    this.getTile(curTile.tileX,curTile.tileY,0)
                        .then(imgData => {
                            var tileImage = new Image();
                            tileImage.posX = curTile.tileX - this.tiles.leftTile;
                            tileImage.posY = curTile.tileY - this.tiles.bottomTile;
                            tileImage.src = (window.URL || window.webkitURL).createObjectURL(imgData);
                            if (!this.state.cache.images[this.state.zoomlevel]) {
                                this.state.cache.images[this.state.zoomlevel]={};
                            } 
                            if (!this.state.cache.images[this.state.zoomlevel][curTile.tileX]) {
                                this.state.cache.images[this.state.zoomlevel][curTile.tileX]={};
                            } 
                            this.state.cache.images[this.state.zoomlevel][curTile.tileX][curTile.tileY]=tileImage;

                            tileImage.onload = (elem) => {
                                this.canvas.drawImage(tileImage, tileImage.posX * tileImage.width, tileImage.posY * tileImage.height);
                                if (tiles.length) {
                                    resolve(this.drawTile(tiles));
                                } else {
                                    resolve({});
                                }
                            };
                        }).catch(reject);
                } else {
                    var tileImage = this.state.cache.images[this.state.zoomlevel][curTile.tileX][curTile.tileY];
                    this.canvas.drawImage(tileImage, tileImage.posX * tileImage.width, tileImage.posY * tileImage.height);
                    if (tiles.length) {
                        resolve(this.drawTile(tiles));
                    } else {
                        resolve({});
                    }
                }
            } else {
                resolve({});
            }
        });
    }

    downloadTile(tileX,tileY) {
        return new Promise((resolve,reject) => {
            var newImg;
            wfetch(`https://tile.openstreetmap.de/${this.state.zoomlevel}/${tileX}/${tileY}.png`)
                .then(resp => resp.blob())
                .then(imgData => wfetch(`${httpPrefix}/sdcard/web/tiles/${this.state.zoomlevel}/${tileX}/${tileY}.png`,{
                    method: 'put',
                    body: (newImg=imgData)
                }).then(resolve(newImg)))
                  .catch(reject);
        })
    }

    getTile(tileX,tileY, retryCount) {
        return new Promise((resolve,reject)=>{
            wfetch(`${httpPrefix}/sdcard/web/tiles/${this.state.zoomlevel}/${tileX}/${tileY}.png`)
                .then(resp => resolve(resp.status >= 300? this.downloadTile(tileX,tileY):resp.blob()))
                .catch(err => {
                    if (retryCount > 3) {
                        reject({error:`Error in downloading ${this.state.zoomlevel}/${tileX}/${tileY}`});
                    } else {
                        console.log(`retrying ${this.state.zoomlevel}/${tileX}/${tileY}`);
                        resolve(this.getTile(tileX,tileY,retryCount++));
                    }
                });
        });
    }

    getClientY(firstPoint) {
        return ((firstPoint.tileY - this.tiles.bottomTile) * 256) + (256 * firstPoint.posTileY);
    }

    getClientX(firstPoint) {
        return ((firstPoint.tileX - this.tiles.leftTile) * 256) + (256 * firstPoint.posTileX);
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
        var n=Math.PI-2*Math.PI*y/Math.pow(2,z);
        return (180/Math.PI*Math.atan(0.5*(Math.exp(n)-Math.exp(-n))));
    }
    
    pointToCartesian(point) {
        return {
            tileX: this.lon2tile(point.longitude, this.state.zoomlevel),
            tileY: this.lat2tile(point.latitude, this.state.zoomlevel),
            posTileX: this.lon2x(point.longitude,this.state.zoomlevel) - this.lon2tile(point.longitude, this.state.zoomlevel),
            posTileY: this.lat2y(point.latitude,this.state.zoomlevel) - this.lat2tile(point.latitude, this.state.zoomlevel),
            ...point
        };
    }

    closeIt(){
        this.setState({closed:true});
    }

    render(){
        return e("div",{key:genUUID(),className:`lightbox ${this.state?.closed?"closed":"opened"}`},[
            e("div",{key:genUUID()},[
                e("div",{key:genUUID(),className:"tripHeader"},[
                    `Trip with ${this.props.points.length} points`,
                    e("br",{key:genUUID()}),
                    `From ${new Date(`${this.props.points[0].timestamp} UTC`).toLocaleString()} `,
                    `To ${new Date(`${this.props.points[this.props.points.length-1].timestamp} UTC`).toLocaleString()} Zoom:`,
                    e("select",{
                        key:genUUID(),
                        onChange: elem=>this.setState({zoomlevel:elem.target.value}),
                        value: this.state.zoomlevel
                    },
                        Array.from(Array(18).keys()).map(zoom => e("option",{key:genUUID()},zoom+1))
                    )
                ]),
                e("span",{key:genUUID(),className:"close",onClick:this.closeIt.bind(this)},"X")
            ]),
            e("div",{key:genUUID(),className:"trip"},e("canvas",{
                key:genUUID(),
                ref: (elem) => this.widget = elem
            }))
        ]);
    }
}
