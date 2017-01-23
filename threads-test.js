const express = require('express');
const body_parser = require('body-parser');
const Png = require('pngjs').PNG;
const threads_test_native = require('bindings')('threads_test_native');

let app = express();

app.use(body_parser.json());

let width, height;

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
  let result = threads_test_native.getResult();
  if (result) {
    let png = new Png({
      width,
      height,
      hasInputAlpha: true
    });
    png.data = result;
    res.type('image/png');
    return png.pack().pipe(res);
  } else {
    return res.send(threads_test_native.getProgress());
  }
});

app.listen(8080);
