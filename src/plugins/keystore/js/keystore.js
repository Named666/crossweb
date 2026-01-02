// Keystore guest module prepared for npm packaging.
import { isNative, invokeNative } from '../../ipc/ipc.js';

const toHex = (bytes) => Array.from(bytes).map(b => b.toString(16).padStart(2, '0')).join('');
const hexToBytes = (hex) => {
  if (!hex) return new Uint8Array(0);
  const out = new Uint8Array(Math.floor(hex.length / 2));
  for (let i = 0; i < out.length; i++) out[i] = parseInt(hex.substr(i * 2, 2), 16);
  return out;
};
const base64FromArrayBuffer = (buf) => {
  const bytes = new Uint8Array(buf);
  let s = '';
  for (let i = 0; i < bytes.length; i++) s += String.fromCharCode(bytes[i]);
  return btoa(s);
};
const arrayBufferFromBase64 = (b64) => {
  const bin = atob(b64 || '');
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
  return out;
};

export async function keystoreEncrypt(hexPayload) {
  if (isNative()) return invokeNative('keystore.encrypt', hexPayload);
  try {
    const bytes = hexToBytes(hexPayload || '');
    return { ok: true, encrypted: base64FromArrayBuffer(bytes.buffer) };
  } catch (err) {
    return Promise.reject(err);
  }
}

export async function keystoreDecrypt(encryptedPayload) {
  if (isNative()) return invokeNative('keystore.decrypt', encryptedPayload);
  try {
    const bytes = arrayBufferFromBase64(encryptedPayload || '');
    return { ok: true, privateKey: toHex(bytes) };
  } catch (err) {
    return Promise.reject(err);
  }
}

export async function keystoreSave(encryptedPayload) {
  if (isNative()) return invokeNative('keystore.save', encryptedPayload);
  try {
    localStorage.setItem('crossweb.keystore.saved', encryptedPayload || '');
    return { ok: true };
  } catch (e) {
    return { ok: false, msg: e && e.message ? e.message : String(e) };
  }
}

export async function keystoreLoad() {
  if (isNative()) return invokeNative('keystore.load', '');
  try {
    const v = localStorage.getItem('crossweb.keystore.saved');
    if (v !== null) return { ok: true, encrypted: v };
    return { ok: false, msg: 'no saved value' };
  } catch (e) {
    return { ok: false, msg: e && e.message ? e.message : String(e) };
  }
}

export default { keystoreEncrypt, keystoreDecrypt, keystoreSave, keystoreLoad };
