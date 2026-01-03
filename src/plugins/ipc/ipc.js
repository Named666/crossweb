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
      const id = window.external.invoke(cmd, payload);
      pending[id] = { resolve, reject };
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
