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

console.log( cd.getName('abcあいうえお@efg') );
var res = cd.getName('abc', function(){
    console.log( 'callback', arguments );
});
console.log( res );
