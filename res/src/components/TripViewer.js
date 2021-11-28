class TripViewer extends React.Component {
    constructor(props) {
        super(props);
        this.zoomlevel=15;
        this.state={images:{}};
    }
    componentDidMount() {
        this.mounted=true;
        this.drawMap();
    }
    

    drawMap() {
        return new Promise((resolve,reject) => {
            if (!this.mounted || !this.widget || !this.props.points || !this.props.points.length){
                reject({});
            } else {
                this.canvas = this.widget.getContext("2d");                    
        
                this.getTripTiles().then(this.drawTripTiles.bind(this))
                                   .then(this.drawTrip.bind(this))
                                   .catch(reject);
            }
        });
    }

    getTripTiles() {
        return new Promise((resolve,reject) => this.props.points?.length?resolve(this.props.points.reduce((ret, point) => {
            ret.trip.leftTile = this.lon2tile(ret.left = Math.min(ret.left, point.longitude));
            ret.trip.rightTile = this.lon2tile(ret.right = Math.max(ret.right, point.longitude));
            ret.trip.topTile = this.lat2tile(ret.top = Math.min(ret.top, point.latitude));
            ret.trip.bottomTile = this.lat2tile(ret.bottom = Math.max(ret.bottom, point.latitude));
            ret.trip.XTiles = (ret.trip.rightTile - ret.trip.leftTile) + 1;
            ret.trip.YTiles = (ret.trip.topTile - ret.trip.bottomTile) + 1;
            ret.margin.bottom = Math.floor((ret.maxYTiles-ret.trip.YTiles)/2);
            ret.margin.top = ret.maxYTiles-ret.trip.YTiles-ret.margin.bottom;
            ret.margin.left = Math.floor((ret.maxXTiles-ret.trip.XTiles)/2);
            ret.margin.right = ret.maxXTiles-ret.trip.XTiles-ret.margin.left;
            ret.leftTile=ret.trip.leftTile-Math.abs(ret.margin.left);
            ret.rightTile=Math.max(ret.maxXTiles+ret.trip.leftTile, ret.trip.rightTile+Math.abs(ret.margin.right));
            ret.bottomTile=ret.trip.bottomTile-Math.abs(ret.margin.bottom);
            ret.topTile=Math.max(ret.maxYTiles+ret.trip.bottomTile, ret.trip.bottomTile+Math.abs(ret.margin.top));
            ret.points.push(this.pointToCartesian(point));
            return ret;
        }, {
            left: this.props.points[0].longitude,
            right: this.props.points[0].longitude,
            top: this.props.points[0].latitude,
            bottom: this.props.points[0].latitude,
            trip:{},
            margin:{},
            points: [],
            margin: [],
            maxXTiles:Math.ceil(window.innerWidth/256),
            maxYTiles: Math.ceil(window.innerHeight/256)
        })):reject({"error":"no points"}));
    }

    setupListeners(canvas,tiles) {
        this.widget.addEventListener("mousemove", (event) => {
            var pt = tiles.points.find(point => (this.getClientX(point,tiles) >= (event.offsetX - 15)) &&
                (this.getClientX(point,tiles) <= (event.offsetX + 15)) &&
                (this.getClientY(point,tiles) >= (event.offsetY - 15)) &&
                (this.getClientY(point,tiles) <= (event.offsetY + 15)));
            var needsRefresh = false;
            tiles.points.filter(point => point != pt).forEach(point => {
                if (point.focused) {
                    point.focused = false;
                    needsRefresh = true;
                }
            });
            if (needsRefresh) {
                this.drawMap().then(res => {
                    if (pt && !pt.focused) {
                        pt.focused = true;
                        this.drawPointPopup(canvas, pt, tiles);
                    }
                });
            } else if (pt && !pt.focused) {
                pt.focused = true;
                this.drawPointPopup(canvas, pt, tiles);
            }
        });
    }

    drawPointPopup(canvas,pt,tiles){
        canvas.font = "10px Helvetica";
        var txtSz = canvas.measureText(new Date(`${pt.timestamp} UTC`).toLocaleString());
        var boxHeight=60;
        var boxWidth=txtSz.width+10;
        canvas.strokeStyle = '#00ffff';
        canvas.shadowColor = '#00ffff';
        canvas.fillStyle = "#ffffff";
        canvas.lineWidth = 1;
        canvas.shadowBlur = 2;
        canvas.beginPath();
        canvas.rect(this.getClientX(pt,tiles),this.getClientY(pt,tiles)-boxHeight,boxWidth,boxHeight);
        canvas.fill();
        canvas.stroke();

        canvas.fillStyle = 'rgba(00, 0, 0, 1)';
        canvas.strokeStyle = '#000000';

        canvas.beginPath();
        var ypos = this.getClientY(pt,tiles)-boxHeight+txtSz.actualBoundingBoxAscent+3;
        var xpos = this.getClientX(pt,tiles)+3;
        canvas.fillText(new Date(`${pt.timestamp} UTC`).toLocaleString(),xpos,ypos);
        ypos+=txtSz.actualBoundingBoxAscent+3;
        canvas.fillText(`Bat: ${Math.floor(pt.Battery)}`,xpos,ypos);
        ypos+=txtSz.actualBoundingBoxAscent+3;
        canvas.fillText(`Sats: ${pt.Satellites}`,xpos,ypos);
        ypos+=txtSz.actualBoundingBoxAscent+3;
        canvas.fillText(`Speed:${Math.floor(1.852*pt.speed/100.0)} km/h`,xpos,ypos);
        ypos+=txtSz.actualBoundingBoxAscent+3;
        canvas.fillText(`Alt:${Math.floor(pt.altitude/100)} m`,xpos,ypos);

        canvas.stroke();
    }

    drawTrip(tiles) {
        var firstPoint = tiles.points[0];
        this.canvas.strokeStyle = '#00ffff';
        this.canvas.shadowColor = '#00ffff';
        this.canvas.fillStyle = "#00ffff";
        this.canvas.lineWidth = 2;
        this.canvas.shadowBlur = 2;

        this.canvas.beginPath();
        this.canvas.moveTo(this.getClientX(firstPoint,tiles), this.getClientY(firstPoint,tiles));
        tiles.points.forEach(point => {
            this.canvas.lineTo(this.getClientX(point,tiles), this.getClientY(point,tiles));
        });
        this.canvas.stroke();
        this.canvas.strokeStyle = '#0000ff';
        this.canvas.shadowColor = '#0000ff';
        this.canvas.fillStyle = "#0000ff";
        tiles.points.forEach(point => {
            this.canvas.beginPath();
            this.canvas.arc(((point.tileX - tiles.leftTile) * 256) + (256 * point.posTileX), ((point.tileY - tiles.bottomTile) * 256) + (256 * point.posTileY), 5, 0, 2 * Math.PI);
            this.canvas.stroke();
        });
    }

    getClientY(firstPoint,tiles) {
        return ((firstPoint.tileY - tiles.bottomTile) * 256) + (256 * firstPoint.posTileY);
    }

    getClientX(firstPoint,tiles) {
        return ((firstPoint.tileX - tiles.leftTile) * 256) + (256 * firstPoint.posTileX);
    }

    drawTripTiles(tiles) {
        this.widget.width=(tiles.trip.XTiles+Math.abs(tiles.margin.left+tiles.margin.right))*256;
        this.widget.height=(tiles.trip.YTiles+Math.abs(tiles.margin.bottom+tiles.margin.top))*256;
        this.canvas.fillStyle = "black";
        this.canvas.fillRect(0,0,window.innerWidth,window.innerHeight);
        this.setupListeners(this.canvas,tiles);
        var promises = [];
        for (var tileX = tiles.leftTile; tileX <= tiles.rightTile; tileX++) {
            for (var tileY = tiles.bottomTile; tileY <= tiles.topTile; tileY++) {
                promises.push(this.drawTile(tileX, tileY, 0, tiles));
            }
        }
        return new Promise((resolve,reject)=>Promise.all(promises).then(res=>resolve(tiles)).catch(err=>resolve(tiles)));
    }

    drawTile(tileX, tileY, numTries, tiles) {
        return new Promise((resolve,reject) => {
            if (!this.state.images[tileX] || !this.state.images[tileX][tileY]){
                var tile = new Image();
                tile.posX = tileX - tiles.leftTile;
                tile.posY = tileY - tiles.bottomTile;
                tile.onload = (elem) => {
                    if (!this.state.images[tileX]) {
                        this.state.images[tileX]={};
                    }
                    this.state.images[tileX][tileY]=elem.currentTarget;
                    resolve(this.canvas.drawImage(elem.currentTarget, elem.currentTarget.posX * elem.currentTarget.width, elem.currentTarget.posY * elem.currentTarget.height));
                };
                tile.onerror = (err) => {
                    if (numTries > 3) {
                        reject(`Failed to fetch tile ${tileX},${tileY}`);
                    } else {
                        this.drawTile(tileX, tileY, numTries+1,tiles).then(resolve).catch(reject);
                    }
                };
                tile.src = `${httpPrefix}/sdcard/web/tiles/${tileX}/${tileY}.png`;
            } else {
                resolve(this.canvas.drawImage(this.state.images[tileX][tileY], 
                                         this.state.images[tileX][tileY].posX * this.state.images[tileX][tileY].width, 
                                         this.state.images[tileX][tileY].posY * this.state.images[tileX][tileY].height));
            }
        });
    }

    lon2tile(lon) { 
        return Math.floor(this.lon2x(lon)); 
    }
    
    lat2tile(lat)  { 
        return Math.floor(this.lat2y(lat));
    }

    lon2x(lon) { 
        return (lon+180)/360*Math.pow(2,this.zoomlevel); 
    }
    
    lat2y(lat)  { 
        return (1-Math.log(Math.tan(lat*Math.PI/180) + 1/Math.cos(lat*Math.PI/180))/Math.PI)/2 *Math.pow(2,this.zoomlevel); 
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
            tileX: this.lon2tile(point.longitude, this.zoomlevel),
            tileY: this.lat2tile(point.latitude, this.zoomlevel),
            posTileX: this.lon2x(point.longitude,this.zoomlevel) - this.lon2tile(point.longitude, this.zoomlevel),
            posTileY: this.lat2y(point.latitude,this.zoomlevel) - this.lat2tile(point.latitude, this.zoomlevel),
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
                    e("br"),
                    `From ${new Date(`${this.props.points[0].timestamp} UTC`).toLocaleString()} `,
                    `To ${new Date(`${this.props.points[this.props.points.length-1].timestamp} UTC`).toLocaleString()}`,
                ]),
                e("span",{key:genUUID(),className:"close",onClick:this.closeIt.bind(this)},"X")
            ]),
            e("div",{key:genUUID(),className:"trip"},e("canvas",{
                onresize:this.drawMap(),
                key:genUUID(),
                ref: (elem) => this.widget = elem
            }))
        ]);
    }
}
