var tty = require('tty');
var pos = require('bindings')('pos');
var code = '\x1b[6n';

function setRawMode(mode) {
  if (process.stdin.setRawMode) {
    process.stdin.setRawMode(mode)
  } else {
    tty.setRawMode(mode)
  }
}

pos.async = function (callback, context) {
  if (process.platform === 'win32') {
    process.nextTick(function () {
      var position = pos.sync();

      if (position) {
        callback && callback.call(context, {
          row: position.row,
          col: position.col
        });
      }
    });

    return;
  }

  // start listening
  process.stdin.resume();
  setRawMode(true);

  process.stdin.once('data', function (b) {
    var match = /\[(\d+)\;(\d+)R$/.exec(b.toString());
    if (match) {
      var position = match.slice(1, 3).reverse().map(Number);

      callback && callback.call(context, {
        row: position[1],
        col: position[0]
      });
    }

    // cleanup and close stdin
    setRawMode(false);
    process.stdin.pause();
  });


  process.stdout.write(code);
  process.stdout.emit('data', code);
};

module.exports = pos;

