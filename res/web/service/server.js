const Finder = require('./finder');
const EventEmitter = require('node:events');

const events = new EventEmitter();
const finder = new Finder.Finder(events,'192.168.1.*');

require("./rest").serve(finder, process.env.PORT || 8008);
require("./wss").serve(events, process.env.WSPORT || 8009);
