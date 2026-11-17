// IPC helper for plugins.
// Provides `isNative()` and `invokeNative(cmd, payload)` to call the host IPC.

const pending = {};
let nativeListenerInstalled = false;

export function isNative() {
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

function generateId() {
  return Math.random().toString(36).slice(2) + Date.now().toString(36);
}

export function invokeNative(cmd, payload) {
  return new Promise((resolve, reject) => {
    if (!isNative()) return reject(new Error('native bridge not available'));
    
    try {
      installListener();
      
      const id = generateId();
      pending[id] = { resolve, reject };

      // Send JSON message: {id, cmd, payload}
      const message = JSON.stringify({
        id,
        cmd: cmd || '',
        payload: payload !== undefined ? payload : null
      });

      window.external.invoke(message);

      // Timeout to avoid forever-hanging promises
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
