const http = require('http');
const net   = require('net');

const ESP_HOST = process.env.CAM_HOST || 'esp32cam.local';
const ESP_PORT = 80;
const PORT     = 8080;

const clients = [];

const PAGE = `<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32-CAM</title>
  <style>
    *{margin:0;padding:0;background:#111}
    img{display:block;width:100vw;height:100vh;object-fit:contain}
  </style>
</head>
<body>
  <img src="/stream">
</body>
</html>`;

function connectESP() {
  const esp = net.connect(ESP_PORT, ESP_HOST, () => {
    console.log('Connected to camera');
    esp.setKeepAlive(true, 5000);
    esp.write(
      'GET /stream HTTP/1.1\r\n' +
      'Host: ' + ESP_HOST + '\r\n' +
      'x-multiclient-stream: 1\r\n' +
      '\r\n'
    );
  });

  let headerDone = false;
  let headerBuf  = Buffer.alloc(0);

  esp.setTimeout(10000);
  esp.on('timeout', () => {
    console.log('Camera timeout — no data for 10s, destroying...');
    esp.destroy();
  });

  esp.on('data', (data) => {
    if (!headerDone) {
      headerBuf = Buffer.concat([headerBuf, data]);
      const s = headerBuf.toString('latin1');
      const i = s.indexOf('\r\n\r\n');
      if (i >= 0) {
        console.log('Camera response:', s.substring(0, i).split('\r\n')[0]);
        const rest = headerBuf.slice(i + 4);
        headerDone = true;
        if (rest.length) forward(rest);
      }
      return;
    }
    forward(data);
  });

  esp.on('close', () => {
    console.log('Camera disconnected, reconnecting in 2s...');
    headerDone = false;
    headerBuf  = Buffer.alloc(0);
    setTimeout(connectESP, 2000);
  });

  esp.on('error', (e) => console.log('Camera error:', e.message));
}

function forward(data) {
  for (const res of clients) {
    try { res.write(data); } catch (_) {}
  }
}

http.createServer((req, res) => {
  if (req.url === '/stream') {
    res.writeHead(200, {
      'Content-Type': 'multipart/x-mixed-replace; boundary=frame',
      'Access-Control-Allow-Origin': '*',
    });
    clients.push(res);
    console.log('+client (', clients.length, 'total)');
    req.on('close', () => {
      clients.splice(clients.indexOf(res), 1);
      console.log('-client (', clients.length, 'total)');
    });
  } else {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
    res.end(PAGE);
  }
}).listen(PORT, () => {
  console.log('Relay listening on :' + PORT);
});

connectESP();
