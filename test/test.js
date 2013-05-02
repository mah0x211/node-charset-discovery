var charsetDiscovery = require('../'),
    util = require('util'),
    fs = require('fs');

console.log( 
    util.inspect( 
        charsetDiscovery, 
        { colors: true, depth: 100, showHidden: true }
    )
);

var cd = new charsetDiscovery();

console.log( cd.getName( new Buffer('abcあいうえお@efg','binary') ) );
var res = cd.getName(new Buffer('abc','binary'), function(){
    console.log( 'callback', arguments );
});
console.log( res );
