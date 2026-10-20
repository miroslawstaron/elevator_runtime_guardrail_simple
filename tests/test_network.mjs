/****************************************
*
* Elevator Lab -- Direct Network and Origin Verification
*
* SPDX-License-Identifier: MIT
*
******************************************/

import assert from 'node:assert/strict';
import {spawn, spawnSync} from 'node:child_process';
import {networkInterfaces} from 'node:os';
import net from 'node:net';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root = fileURLToPath(new URL('../', import.meta.url));
const binary = process.argv[2] || path.join(root, 'build', 'elevator_web.exe');
const address = Object.values(networkInterfaces()).flat().find(item => item.family === 'IPv4' && !item.internal)?.address;
assert.ok(address, 'a network IPv4 interface is required for this integration test');
const port = 18091;
/**
 * Wait briefly for server startup or process teardown without blocking the C clock
 */
const pause = ms => new Promise(resolve => setTimeout(resolve, ms));

/**
 * Exercise an explicit destination, Host and optional Origin using the Node HTTP client
 * Return the response status and body for network policy assertions
 */
async function request(destination, host, origin) {
  return new Promise((resolve, reject) => {
    const socket = net.connect(port, destination);
    let response = '';
    socket.setTimeout(3000, () => socket.destroy(new Error('request timeout')));
    socket.on('error', reject);
    socket.on('data', data => { response += data; });
    socket.on('end', () => resolve(response));
    socket.on('connect', () => {
      const body = 'action=request&floor=3';
      socket.write(origin === undefined
        ? `GET /api/state HTTP/1.1\r\nHost: ${host}\r\n\r\n`
        : `POST /api/control HTTP/1.1\r\nHost: ${host}\r\nOrigin: ${origin}\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: ${body.length}\r\n\r\n${body}`);
    });
  });
}

/**
 * Start one isolated bind/hostname configuration and verify its accepted origins
 * Always terminate that test process before trying another configuration
 */
async function check(listenAddress, allowedHosts = []) {
  const server = spawn(binary, ['--port', String(port), '--database', ':memory:',
    ...(listenAddress ? ['--listen-address', listenAddress] : []),
    ...allowedHosts.flatMap(host => ['--allowed-host', host])], {cwd: root, windowsHide: true});
  let log = '';
  server.stdout.on('data', data => { log += data; });
  server.stderr.on('data', data => { log += data; });
  const closed = new Promise(resolve => server.once('close', resolve));
  let spawnError;
  server.on('error', error => { spawnError = error; });
  try {
    for (let attempt = 0; attempt < 100 && !log.includes('Telemetry:'); attempt++) {
      if (spawnError) throw spawnError;
      if (server.exitCode !== null) throw new Error(log);
      await pause(50);
    }
    assert.match(log, /Telemetry:/, 'server must become ready');
    assert.match(await request('127.0.0.1', `127.0.0.1:${port}`), /200 OK/);
    // A remote-looking Host must not broaden the default loopback-only service.
    assert.match(await request('127.0.0.1', `${address}:${port}`), /403 Forbidden/);
    if (listenAddress) {
      assert.match(await request(address, `${address}:${port}`), /200 OK/);
      assert.match(await request(address, `${address}:${port}`, `http://${address}:${port}`), /200 OK/);
      assert.match(await request(address, `${address}:${port}`, 'https://outside.example'), /403 Forbidden/);
      assert.match(await request(address, `outside.example:${port}`), /403 Forbidden/);
      assert.match(await request(address, `${address}:${port + 1}`), /403 Forbidden/);
      assert.match(await request(address, `0.0.0.0:${port}`), /403 Forbidden/);
      for (const name of ['lab.example', 'second.example']) {
        const expected = allowedHosts.includes(name) ? /200 OK/ : /403 Forbidden/;
        assert.match(await request(address, `${name}:${port}`), expected);
        assert.match(await request(address, `${name}:${port}`, `http://${name}:${port}`), expected);
        if (allowedHosts.includes(name)) {
          assert.match(await request(address, `${name.toUpperCase()}:${port}`), /200 OK/);
          assert.match(await request(address, `${name}:${port}`, 'http://outside.example'), /403 Forbidden/);
          assert.match(await request(address, `${name}:${port + 1}`), /403 Forbidden/);
          assert.match(await request(address, `${name}.outside.example:${port}`), /403 Forbidden/);
        }
      }
    }
  } finally {
    server.kill();
    await closed;
  }
}

await check();
await check('0.0.0.0');
await check('0.0.0.0', ['lab.example', 'second.example']);
const invalid = spawnSync(binary, ['--listen-address', 'invalid'], {cwd: root, windowsHide: true, encoding: 'utf8'});
assert.equal(invalid.status, 1);
assert.match(invalid.stderr, /must be an IPv4 address/);
// CONFIGURATION REJECTION: Invalid hostname arguments must fail before opening a listener
for (const name of ['http://lab.example', 'lab.example:8099', '*.example', 'lab.example/path', '']) {
  const invalidHost = spawnSync(binary, ['--allowed-host', name], {cwd: root, windowsHide: true, encoding: 'utf8'});
  assert.equal(invalidHost.status, 1, `invalid hostname must be rejected: ${name}`);
}
console.log('PASS: loopback default, network GET/POST, explicit hostname aliases, host/port restrictions, cross-origin rejection, invalid configuration');
