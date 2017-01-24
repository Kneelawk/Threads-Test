const fs = require('fs');
const express = require('express');
const body_parser = require('body-parser');
const Png = require('pngjs').PNG;
const threads_test_native = require('bindings')('threads_test_native');

if (!fs.existsSync('fractals')) {
  fs.mkdirSync('fractals');
}

let app = express();

app.use(body_parser.json());

let width, height;

threads_test_native.setDoneCallback((data) => {
  let buffer = new Buffer(data);
  let png = new Png({
    width,
    height,
    hasInputAlpha: true
  });
  png.data = buffer;
  png.pack().pipe(fs.createWriteStream('fractals/fractal.png'));
});

app.post('/api/generate', (req, res) => {
  threads_test_native.generateFractal(
    req.body.imageWidth || 300,
    req.body.imageHeight || 200,
    req.body.fractalWidth || 3,
    req.body.fractalHeight || 2,
    req.body.fractalX || 0,
    req.body.fractalY || 0,
    req.body.iterations || 100);

  width = req.body.imageWidth || 300;
  height = req.body.imageHeight || 200;

  return res.send(req.body);
});

app.get('/api/progress', (req, res) => {
  return res.send(threads_test_native.getProgress());
});

app.get('/api/result', (req, res) => {
  let progress = threads_test_native.getProgress();
  if (!progress.generating) {
    res.type('image/png');
    // node is single threaded, this will not happend while the file is being written
    return fs.createReadStream('fractals/fractal.png').pipe(res);
  } else {
    return res.send(progress);
  }
});

app.listen(8080);
