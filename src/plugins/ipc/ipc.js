// IPC helper for plugins.
// Provides `isNative()` and `invokeNative(cmd, payload)` to call the host IPC.

const pending = {};
let nativeListenerInstalled = false;

export function isNative() {
  console.log('isNative check:', typeof window !== 'undefined', window && window.external, window && window.external && typeof window.external.invoke === 'function');
  return typeof window !== 'undefined' && window.external && typeof window.external.invoke === 'function';
}

function installListener() {
  if (nativeListenerInstalled || !isNative()) return;
  const prev = window.external.onMessage;
  window.external.onMessage = (id, result) => {
    if (pending[id]) {
      try { pending[id].resolve(result); } catch (e) { /* ignore */ }
      delete pending[id];
    }
    if (typeof prev === 'function') {
      try { prev(id, result); } catch (e) { /* ignore */ }
    }
  };
  nativeListenerInstalled = true;
}

export function invokeNative(cmd, payload) {
  return new Promise((resolve, reject) => {
    if (!isNative()) return reject(new Error('native bridge not available'));
    try {
      installListener();
      console.log(`Invoking native command: ${cmd} with payload:`, payload);
      // If the host injected the bridge it will set __bridgeInstalled.
      // Otherwise, some webviews expose a raw `external.invoke` that
      // expects a single string payload. Compose the message to match
      // the native format: id<SEP>cmd<SEP>base64(payload)
      const SEP = String.fromCharCode(30);
      function encodePayload(value) {
        if (value === undefined || value === null) return '';
        const text = (typeof value === 'string') ? value : JSON.stringify(value);
        if (typeof TextEncoder !== 'undefined') {
          const bytes = new TextEncoder().encode(text);
          let bin = '';
          for (let i = 0; i < bytes.length; i++) bin += String.fromCharCode(bytes[i]);
          return btoa(bin);
        }
        return btoa(unescape(encodeURIComponent(text)));
      }

      const id = Math.random().toString(36).slice(2) + Date.now().toString(36);
      pending[id] = { resolve, reject };

      // If bridge installed, call native invoke as (cmd, payload) and expect it to return id.
      if (window.external.__bridgeInstalled) {
        try {
          const ret = window.external.invoke(cmd, payload);
          // Some bridges return the id directly
          if (ret) {
            // Use returned id if non-empty and pending not yet set for it
            if (typeof ret === 'string' && ret.length && !pending[ret]) {
              pending[ret] = pending[id];
              delete pending[id];
            }
          }
        } catch (e) {
          // ignore and fall back to composed call below
        }
      } else {
        // Compose single-string payload expected by native host
        const normalized = (payload === undefined || payload === null) ? '' : (typeof payload === 'string' ? payload : JSON.stringify(payload));
        const encoded = encodePayload(normalized);
        try {
          window.external.invoke(id + SEP + String(cmd || '') + SEP + encoded);
        } catch (e) {
          // if invoke throws, clean up pending and rethrow
          delete pending[id];
          throw e;
        }
      }

      // timeout to avoid forever-hanging promises
      setTimeout(() => {
        if (pending[id]) {
          try { pending[id].reject(new Error('native call timeout')); } catch (e) { /* ignore */ }
          delete pending[id];
        }
      }, 30000);
    } catch (err) {
      reject(err);
    }
  });
}

export default { isNative, invokeNative };
