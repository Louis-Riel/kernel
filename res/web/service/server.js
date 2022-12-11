const Finder = require('./finder');
const EventEmitter = require('node:events');

const events = new EventEmitter();
const finder = new Finder.Finder(events,process.env.REACT_APP_NETMASK);

require("./rest").serve(events, finder, process.env.REACT_APP_PORT);
require("./wss").serve(events, process.env.REACT_APP_WSPORT);
