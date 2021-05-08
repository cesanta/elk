'use strict';
import {h, html, render, Router, useEffect, useRef, useState, useCallback} from './preact-bundle.min.js';

const DEFAULT_CODE = `// To start, flash this ESP32 Arduino firmware:
// https://github.com/cesanta/elk/blob/master/examples/Esp32JS/Esp32JS.ino

let led = { pin: 2, on: false };  // LED state
gpio.mode(led.pin, 2);            // Set LED pin to output mode

let ledTimer = timer.create(1000, function() {  // Blink LED every second
  led.on = !led.on;                             // Toggle LED state
  gpio.write(led.pin, led.on);                  // Set LED voltage
}, null);

let broker = 'mqtt://broker.hivemq.com:1883';   // Our MQTT server
let mqttConn = mqtt.connect(broker);            // Create MQTT connection
mqtt.subscribe(mqttConn, 'elk/rx');             // Subscribe to 'elk/rx' topic

// Handle incoming MQTT messages. Send a response with some stats.
mqtt.setfn(function(topic, message) {
  log('MQTT: ' + topic + ' -> ' + message);
  let response = {ram: ram(), usage: usage(), received: message};
  mqtt.publish(mqttConn, 'elk/tx', str(response));
}, null);

// Called when JS instance is destroyed
let cleanup = function() {
  log('Cleaning up C state...');
  timer.delete(ledTimer);
  mqtt.disconnect(mqttConn);
};
`;

const InfoPanel = function(props) {
  const href = 'https://github.com/cesanta/elk'
  const a = (href, title) => html`<a target="_blank" href=${href}>${title}</a>`;
  return html`
    <${Panel} type="width" value="33%" class="bg-light ${props.class}">
      <div class="m-2 text-muted">
        <p>
          This editor lets you develop ESP32 firmware
          in JavaScript. All components are open source: 
        </p>

        <ul>
          <li>${a(href, 'elk')} - a JS engine</li>
          <li>${a(href, 'mongoose')} - a networking library</li>
          <li>${a(href, 'mjson')} - a JSON parser</li>
          <li>${a(href, 'Eps32JS')} - ESP32 firmware and this editor</li>
        </ul>
        <h5>C API imported to JS:</h5>
        <ul>
          <li>gpio.mode(pin, mode) - set pin mode</li>
          <li>gpio.write(pin, value) - set pin state</li>
          <li>gpio.read(pin) - read pin state</li>
          <li>timer.create(milli, func, null) - create timer</li>
          <li>timer.delete(timerID) - delete timer</li>
          <li>mqtt.connect(url) - MQTT connect</li>
          <li>mqtt.disconnect(url) - MQTT disconnect</li>
          <li>mqtt.publish(conn, topic, msg) - publish</li>
          <li>mqtt.subscribe(conn, topic) - subscribe</li>
          <li>mqtt.setfn(conn, func) - set MQTT handler</li>
          <li>str(val) - stringify JS value</li>
          <li>log(stringval) - log a message</li>
          <li>usage() - JS mem usage in %</li>
          <li>ram() - current free RAM in bytes</li>
        </ul>
      </div>
    </${Panel}>
  `;
};

const getCookie = function(name) {
  var v = document.cookie.match('(^|;) ?' + name + '=([^;]*)(;|$)');
  return v ? v[2].replace(/^[\s;]/, '') : '';
};

// Simple publish/subscribe
var pubsub = function() {
  var events = {}, id = 0;
  return {
    sub: function(name, fn) {
      if (!events[name]) events[name] = {};
      events[name][id] = fn;
      return id++;
    },
    del: function(name, key) {
      delete (events[name] || {})[key];
    },
    pub: function(name, data) {
      var m = events[name] || {};
      for (var k in m) m[k](data);
    },
  };
};

// JSON-RPC over Websocket, using pubsub
const rpc = (url, ps) => new Promise(function(resolve, reject) {
  let ws = new WebSocket(url), rpcid = 0;
  const instance = {
    disconnect: () => new Promise(r => r(ws.close())),
    call: (method, params, timeout) => new Promise(function(res, rej) {
      const rid = rpcid++;
      console.log('call', method, params);
      const pid = ps.sub(`rpc-${rid}`, data => {
        clearTimeout(tid);
        ps.del(pid);
        res(data);
      });
      const tid = setTimeout(() => {
        ps.del(pid);
        rej();
        console.log('timeout', pid);
      }, timeout || 3000);
      ws.send(JSON.stringify({id: rid, method, params}));
    }),
  };
  ws.onopen = () => {
    ps.pub('online');
    resolve(instance);
  };
  ws.onclose = ws.onerror = () => {
    ps.pub('offline');
    reject();
  };
  ws.onmessage = function(ev) {
    try {
      const msg = JSON.parse(ev.data)
      ps.pub('ws', msg);
      if (msg.id) ps.pub(`rpc-${msg.id}`, msg);
    } catch (e) {
      console.log('Invalid ws frame:', e, ev.data);
    }
  };
});

const Editor = function(props) {
  const textarea = useRef(null);
  useEffect(() => {
    const cm = window.CodeMirror.fromTextArea(textarea.current, {
      lineNumbers: true,
      lineWrapping: true,
      mode: 'javascript',
    });
    cm.on('keydown', (cm, ev) => {
      if (ev.keyCode === 13 && (ev.metaKey || ev.ctrlKey))
        props.ps.pub('wantexec');
    });
    cm.on('change', function(cm, change) {
      if (props.onChange && change.origin !== 'setValue')
        props.onChange(cm.getValue(), change);
    });
    cm.setValue(props.defaultValue || '');
    return () => cm.toTextArea();
  }, []);
  return html`<div class=${props.class}>
    <textarea class="d-none" ref=${textarea} /></div>`;
};

const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

const Button = function(props) {
  const [spin, setSpin] = useState(false);
  const cb = ev => {
    if (!props.onClick) return;
    setSpin(true);
    const res = props.onClick();
    if (!res || !res.catch) {
      setSpin(false);
    } else {
      res.catch(() => false).then(() => setSpin(false));
    }
  };
  const color = props.disabled ? 'btn-secondary' : 'btn-primary';
  const iclass = `fa fa-fw fa-${spin ? 'refresh fa-spin' : props.icon}`;
  return html`
    <button class="btn btn-sm ${color} ${props.class}" tooltip=${props.tooltip}
      onclick=${cb} disabled=${props.disabled || spin} >
        <i class="${props.text ? 'mr-2' : ''} ${iclass}"></i>
      ${props.text}
    </button>`;
};

const Panel = function(props) {
  const [attr, setAttr] = useState(props.value);
  const toggleAttr = ev => setAttr(attr ? 0 : props.value);
  const w = props.type == 'width';
  const style = `transform: translate(${w ? '-100%,0' : '0,-100%'});`;
  const border = `border-${w ? 'left' : 'top'}: 1px solid #ccc`;
  const handles = [['angle-right', 'angle-left'], ['angle-down', 'angle-up']];
  const handle = handles[w ? 0 : 1][attr ? 0 : 1];
  return html`
    <div class="${props.class}"
      style="${props.type}: ${attr}; ${border};
      transition: ${props.type} 0.3s ease-in-out;">
      <div class="position-absolute bg-warning" 
        style="${style} z-index: 99; cursor: pointer;">
        <i class="fa fa-fw fa-bold fa-${handle}" onClick=${toggleAttr} />
      </div>
      ${props.children}
    </div>
  `;
};

const autoscroll = (el) => {
  const sh = el.scrollHeight, ch = el.clientHeight, max = sh - ch;
  el.scrollTop = max > 0 ? max : 0;
};

const LogPanel = function(props) {
  const pre = useRef(null);
  const [enableAutoscroll, setEnableAutoscroll] = useState(true);
  const [text, setText] = useState('');
  useEffect(() => {
    const id = props.ps.sub('ws', msg => {
      if (msg.name != 'log') return;
      setText(t => t + atob(msg.data));
      if (enableAutoscroll && pre.current) autoscroll(pre.current.parentNode);
    });
    return () => props.ps.del(id);
  }, []);
  const onclear = ev => {
    ev.preventDefault();
    setText('');
  };
  const onenable = ev => setEnableAutoscroll(!enableAutoscroll);
  return html`
    <${Panel} type="height" value="300px" class=${props.class}>
      <div class="position-absolute text-muted d-flex" style="right: 0;">
        <div class="custom-control custom-checkbox mr-2 mt-2 d-none">
          <input type="checkbox" class="custom-control-input" id="cb1"
            onchange=${onenable} checked=${enableAutoscroll} />
          <label class="custom-control-label" for="cb1">autoscroll</label>
        </div>
        <a href="#" class="py-0 m-2"
          onclick=${onclear} > clear logs </a>
      </div>
      <pre class="m-2 small" ref=${pre}>${text}</pre>
    </${Panel}>
  `;
};

const Toolbar = function(props) {
  const [ip, setIP] = useState(getCookie('elk-ip'));
  const color = props.online ? 'text-success' : 'text-danger';
  const label = props.online ? 'connected' : 'disconnected';
  const oninput = ev => setIP(ev.target.value);
  const text = props.online ? 'disconnect' : 'connect';
  const onconnect = ev => {
    if (props.online) {
      props.setOnline(false);
      props.setRPC(null);
      return props.rpc.disconnect();
    }
    return rpc(`ws://${ip}/ws`, props.ps)
        .then(rpc => {
          props.setRPC(rpc);
          props.setOnline(true);
          document.cookie = `elk-ip=${ip}`;
        })
        .catch(() => props.setOnline(false));
  };
  const onexecute = ev => props.rpc.call('exec', {code: props.code});
  useEffect(() => {
    const id = props.ps.sub('wantexec', onexecute);
    return () => props.ps.del(id);
  }, []);
  return html`
    <nav class=${props.class}>
        <${Button} text="execute (Ctrl+Enter)"
          icon="play" class="ml-3 mr-5"
          disabled=${!props.online} onClick=${onexecute} />
        <div class="nav-item text-light">
          <i class="fa fa-fw fa-circle ${color} mr-2" />
          ${label}
        </div>
        <div class="input-group input-group-sm ml-3 mr-2">
          <input class="form-control form-control-sm" style="width: 12em;"
            disabled=${props.online} onInput=${oninput} value=${ip}
            placeholder="IP address, x.x.x.x" type="text" />
          <div class="input-group-append">
            <${Button} text=${text} icon="link" onClick=${onconnect} />
          </div>
        </div>
        <div class="flex-grow-1" />
        <div class="text-light">
          <img src="elk.svg" height="32" class="round mr-2" />
          Elk JS, 
          \u00a9 2021 Cesanta. <a 
            href="https://cesanta.com/contact.html">Contact us</a>
        </div>
    </nav>
  `;
};

const Main = function(props) {
  return html`
    <div class=${props.class}>
      <${Editor} class="flex-grow-1" ps=${props.ps}
        defaultValue=${props.code} onChange=${x => props.setCode(x)}/>
      <${InfoPanel} class="overflow-auto h-100 vh-100" />
    </div>
  `;
};

const App = function(props) {
  const [ps, setPS] = useState(pubsub());
  const [code, setCode] = useState(DEFAULT_CODE);
  const [online, setOnline] = useState(false);
  const [rpc, setRPC] = useState(null);
  useEffect(() => {
    const id = ps.sub('offline', () => setOnline(false));
    return () => ps.del(id);
  }, []);
  return html`
    <div class="d-flex flex-column position-absolute vh-100 h-100 w-100">
      <${Toolbar} online=${online} setOnline=${setOnline}
        setRPC=${setRPC} rpc=${rpc} ps=${ps} code=${code}
        class="navbar bg-dark form-inline" />
      <${Main} code=${code} setCode=${setCode} ps=${ps}
        class="d-flex flex-grow-1 overflow-auto" />
      <${LogPanel} class="overflow-auto" ps=${ps} />
    </div>
  `;
};

window.onload = () => render(h(App), document.body);
