var charsetDiscovery = require('../'),
    util = require('util'),
    fs = require('fs');

console.log( 
    util.inspect( 
        charsetDiscovery, 
        { colors: true, depth: 100, showHidden: true }
    )
);

