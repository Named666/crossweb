import {
  finalizeEvent,
  getPublicKey as getNostrPublicKey,
  nip04,
  nip44,
} from 'nostr-tools';
import {
  keystoreDecrypt,
  keystoreEncrypt,
  keystoreGenerate,
  keystoreLoad,
  keystoreSave,
} from '../../keystore/js/keystore.js';

const HEX_32 = /^[0-9a-f]{64}$/i;

function bytesToHex(bytes) {
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, '0')).join('');
}

function hexToBytes(hex) {
  if (!HEX_32.test(hex || '')) {
    throw new Error('invalid nostr private key');
  }

  const out = new Uint8Array(32);
  for (let i = 0; i < out.length; i += 1) {
    out[i] = Number.parseInt(hex.slice(i * 2, i * 2 + 2), 16);
  }
  return out;
}

function assertOk(result, fallback) {
  if (!result || result.ok !== true) {
    throw new Error((result && (result.msg || result.error)) || fallback);
  }
  return result;
}

function normalizeUnsignedEvent(event) {
  if (!event || typeof event !== 'object') {
    throw new Error('event must be an object');
  }
  if (!Number.isInteger(event.created_at)) {
    throw new Error('event.created_at must be an integer unix timestamp');
  }
  if (!Number.isInteger(event.kind)) {
    throw new Error('event.kind must be an integer');
  }
  if (!Array.isArray(event.tags) || !event.tags.every((tag) => Array.isArray(tag))) {
    throw new Error('event.tags must be an array of arrays');
  }
  if (typeof event.content !== 'string') {
    throw new Error('event.content must be a string');
  }

  return {
    created_at: event.created_at,
    kind: event.kind,
    tags: event.tags.map((tag) => tag.map((value) => String(value))),
    content: event.content,
  };
}

async function loadOrCreatePrivateKeyHex() {
  const loaded = await keystoreLoad();

  if (loaded && loaded.ok === true && loaded.encrypted) {
    const decrypted = assertOk(
      await keystoreDecrypt(loaded.encrypted),
      'failed to decrypt nostr private key',
    );
    if (!HEX_32.test(decrypted.privateKey || '')) {
      throw new Error('stored nostr private key is invalid');
    }
    return decrypted.privateKey.toLowerCase();
  }

  const generated = assertOk(
    await keystoreGenerate(),
    'failed to generate nostr private key',
  );
  const privateKeyHex = (generated.privateKey || '').toLowerCase();
  hexToBytes(privateKeyHex);
  const encrypted = assertOk(
    await keystoreEncrypt(privateKeyHex),
    'failed to encrypt nostr private key',
  );
  assertOk(await keystoreSave(encrypted.encrypted), 'failed to store nostr private key');
  return privateKeyHex;
}

export function createNostrSigner() {
  async function getSecretKey() {
    return hexToBytes(await loadOrCreatePrivateKeyHex());
  }

  return {
    async getPublicKey() {
      return getNostrPublicKey(await getSecretKey());
    },

    async signEvent(event) {
      return finalizeEvent(normalizeUnsignedEvent(event), await getSecretKey());
    },

    nip04: {
      async encrypt(pubkey, plaintext) {
        return nip04.encrypt(await getSecretKey(), pubkey, plaintext);
      },

      async decrypt(pubkey, ciphertext) {
        return nip04.decrypt(await getSecretKey(), pubkey, ciphertext);
      },
    },

    nip44: {
      async encrypt(pubkey, plaintext) {
        const conversationKey = nip44.getConversationKey(await getSecretKey(), pubkey);
        return nip44.encrypt(plaintext, conversationKey);
      },

      async decrypt(pubkey, ciphertext) {
        const conversationKey = nip44.getConversationKey(await getSecretKey(), pubkey);
        return nip44.decrypt(ciphertext, conversationKey);
      },
    },
  };
}

export function installWindowNostr(target = globalThis) {
  const signer = createNostrSigner();

  if (target && typeof target === 'object') {
    Object.defineProperty(target, 'nostr', {
      configurable: true,
      enumerable: true,
      value: signer,
      writable: false,
    });
  }

  return signer;
}

export const nostr = installWindowNostr(
  typeof window !== 'undefined' ? window : globalThis,
);

export default nostr;
