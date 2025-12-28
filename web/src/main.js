import './style.css'
import javascriptLogo from './javascript.svg'
import viteLogo from '/vite.svg'
import { setupCounter } from './counter.js'

document.querySelector('#app').innerHTML = `
  <div>
    <a href="https://vite.dev" target="_blank">
      <img src="${viteLogo}" class="logo" alt="Vite logo" />
    </a>
    <a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript" target="_blank">
      <img src="${javascriptLogo}" class="logo vanilla" alt="JavaScript logo" />
    </a>
    <h1>Hello Vite!</h1>
    <div class="card">
      <button id="counter" type="button"></button>
      <button id="keystore-encrypt" type="button">Keystore Encrypt</button>
      <button id="keystore-decrypt" type="button">Keystore Decrypt</button>
      <button id="keystore-save" type="button">Keystore Save</button>
      <button id="keystore-load" type="button">Keystore Load</button>
      <!-- <button id="fsread" type="button">Test FS Read</button> -->
    </div>
    <div id="results" style="margin-top: 20px; padding: 10px; border: 1px solid #ccc; background: #f9f9f9;"></div>
    <p class="read-the-docs">
      Click on the Vite logo to learn more
    </p>
  </div>
`

setupCounter(document.querySelector('#counter'))

// Set up native bridge
const resultsDiv = document.querySelector('#results');
let pendingInvokes = {};
let encryptedData = null;

window.__native__.onMessage = function(id, result) {
  console.log('Received from native:', id, result);
  const cmd = pendingInvokes[id];
  if (cmd) {
    delete pendingInvokes[id];
    resultsDiv.innerHTML += `<p><strong>${cmd}:</strong> ${JSON.stringify(result)}</p>`;
    if (cmd === 'keystore.encrypt' && result.ok) {
      encryptedData = result.encrypted;
    }
  }
};

function invokeCmd(cmd, payload) {
  const id = window.__native__.invoke(cmd, payload);
  pendingInvokes[id] = cmd;
  console.log(`Invoked ${cmd} with id: ${id}`);
  return id;
}

document.querySelector('#keystore-encrypt').addEventListener('click', () => {
  invokeCmd('keystore.encrypt', 'deadbeef');
});

document.querySelector('#keystore-decrypt').addEventListener('click', () => {
  if (encryptedData) {
    invokeCmd('keystore.decrypt', encryptedData);
  } else {
    resultsDiv.innerHTML += `<p>No encrypted data available for decrypt</p>`;
  }
});

document.querySelector('#keystore-save').addEventListener('click', () => {
  if (encryptedData) {
    invokeCmd('keystore.save', encryptedData);
  } else {
    resultsDiv.innerHTML += `<p>No encrypted data available for save</p>`;
  }
});

document.querySelector('#keystore-load').addEventListener('click', () => {
  invokeCmd('keystore.load', '');
});

// document.querySelector('#fsread').addEventListener('click', () => {
//   const id = window.__native__.invoke('fs.read', JSON.stringify({ path: 'test.txt' }));
//   console.log('Invoked fs.read with id:', id);
// });
