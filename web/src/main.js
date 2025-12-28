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
      <button id="keystore" type="button">Test Keystore</button>
      <!-- <button id="fsread" type="button">Test FS Read</button> -->
    </div>
    <p class="read-the-docs">
      Click on the Vite logo to learn more
    </p>
  </div>
`

setupCounter(document.querySelector('#counter'))

// Set up native bridge
window.__native__.onMessage = function(msg) {
  console.log('Received from native:', msg);
  alert('Native response: ' + msg);
};

document.querySelector('#keystore').addEventListener('click', () => {
  const id = window.__native__.invoke('keystore.get', 'testkey');
  console.log('Invoked keystore.get with id:', id);
});

// document.querySelector('#fsread').addEventListener('click', () => {
//   const id = window.__native__.invoke('fs.read', JSON.stringify({ path: 'test.txt' }));
//   console.log('Invoked fs.read with id:', id);
// });
