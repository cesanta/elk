'use strict';
import {h, html, render, Router, useEffect, useRef, useState, useCallback} from './preact-bundle.min.js';

const DEFAULT_CODE =
    `// To start, flash this ESP32 Arduino firmware, enter IP address and click connect:
// https://github.com/cesanta/elk/blob/master/examples/Esp32JS/Esp32JS.ino

let led = { pin: 26, on: 0 };  // LED state
gpio.mode(led.pin, 2);         // Set LED pin to output mode

let timer_fn = function() {
  led.on = led.on ? 0 : 1; 
  gpio.write(led.pin, led.on);
};

// Start a timer that toggles LED every 1000 milliseconds
// This timer is a C resource. It is cleaned automatically on JS code refresh
let timer_id = timer.create(1000, 'timer_fn');
`;

const clamp = (x, min, max) => Math.max(Math.min(x, max), min).toFixed(2);
export const Split = ({vertical, children, startSize = 50, minSize = 10}) => {
  if (children.length !== 2) throw 'Split expects exactly 2 child panes';
  const layout = useRef(null);
  const divider = useRef(null);
  const resizing = useRef(null);
  const [size, setSize] = useState(startSize);
  const onMouseDown = (e) => (resizing.current = e.target);
  const handleMouseUp = (e) => (resizing.current = null);
  const handleMouseMove = (e) => {
    if (!resizing.current || !divider.current || !layout.current) return;
    const layoutRect = layout.current.getBoundingClientRect();
    const splitterRect = divider.current.getBoundingClientRect();
    let totalSize = vertical ? layoutRect.height : layoutRect.width;
    let splitterSize = vertical ? splitterRect.height : splitterRect.width;
    let offset =
        (vertical ? e.clientY - layoutRect.top : e.clientX - layoutRect.left) -
        2;
    let secondaryPaneSize = clamp(
        ((totalSize - splitterSize - offset) * 100) / totalSize, minSize,
        100 - minSize);
    setSize(100 - secondaryPaneSize);
    // console.log(secondaryPaneSize);
  };
  useEffect(() => {
    document.addEventListener('mouseup', handleMouseUp);
    document.addEventListener('mousemove', handleMouseMove);
    return () => {
      document.removeEventListener('mouseup', handleMouseUp);
      document.removeEventListener('mousemove', handleMouseMove);
    };
  }, []);

  return html`
      <div class="split split-${vertical ? 'vertical' : 'horizontal'}"
        ref=${layout}>
        <div style="flex: 0 0 ${size}%">${children[0]}</div>
        <div class="split-divider" onMouseDown=${onMouseDown} ref=${divider} />
        <div style="flex: 1 1 ${100 - size}%"> ${children[1]}</div>
      </div>`;
};

const InfoPanel = function(props) {
  const href = 'https://github.com/cesanta/elk'
  const a = (href, title) => html`<a target="_blank" href=${href}>${title}</a>`;
  return html`
      <div class="m-2 text-muted overflow-auto mh-100">
        <p>
          This editor lets you customise ESP32 firmware
          with JavaScript. All components are open source: 
        </p>

        <ul>
          <li>${a(href, 'elk')} - a JS engine</li>
          <li>${a(href, 'mongoose')} - a networking library</li>
          <li>${a(href, 'Eps32JS')} - ESP32 firmware and this editor</li>
        </ul>
        <h5>C API imported to JS:</h5>
        <ul>
          <li>gpio.mode(pin, mode) - set pin mode</li>
          <li>gpio.write(pin, value) - set pin state</li>
          <li>gpio.read(pin) - read pin state</li>
          <li>timer.create(milli, func, null) - create timer</li>
          <li>timer.delete(timerID) - delete timer</li>
          <li>log(strval) - log a message</li>
        </ul>

        <p>
          Refer to <a href="https://github.com/cesanta/elk/blob/master/examples/Esp32JS/JS.h">JS.h</a> for implementation details.
        </p>
      </div>`;
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
      if ('id' in msg) ps.pub(`rpc-${msg.id}`, msg);
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
    <div class="mh-100 w-100">
      <div class="position-absolute text-muted d-flex" style="right: 0;">
        <div class="custom-control custom-checkbox mr-2 mt-2">
          <input type="checkbox" class="custom-control-input" id="cb1"
            onchange=${onenable} checked=${enableAutoscroll} />
          <label class="custom-control-label" for="cb1">autoscroll</label>
        </div>
        <button class="btn btn-sm btn-primary py-0 m-2"
          onclick=${onclear}> clear logs </button>
      </div>
      <pre class="m-2 small h-100 overflow-auto" ref=${pre}>${text}</pre>
    </div>
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
          Elk JS, \u00a9 2021-2022 Cesanta. <a 
            href="https://cesanta.com/contact.html">Contact us</a>
        </div>
    </nav>
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
    <div class="d-flex flex-column h-100 w-100">
      <${Toolbar} online=${online} setOnline=${setOnline}
        setRPC=${setRPC} rpc=${rpc} ps=${ps} code=${code}
        class="navbar bg-dark form-inline" />
      <div class="flex-grow-1 flex-fill">
        <${Split} startSize="70" vertical="true">
          <${Split} startSize="65">
            <${Editor} ps=${ps} defaultValue=${code} class="w-100 h-100"
              onChange=${x => setCode(x)}/>
            <${InfoPanel} class="overflow-auto h-100 vh-100" />
          <//>
          <${LogPanel} ps=${ps} />
        <//>
      </div>
    </div>`;
};

window.onload = () => render(h(App), document.body);
