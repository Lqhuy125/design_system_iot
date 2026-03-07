/**
 * crypto.js — Decrypt IMU data from ESP32
 * Matches: ESP_Sender/src/secure_data.cpp
 *
 * Encryption: AES-128-CBC + CMAC-128 MIC
 */

// ===== KEYS (Must match ESP32 secure_data.cpp) =====

// DATA_AES_KEY: 3132333435363738393A3B3C3D3E3F40
const DATA_AES_KEY = new Uint8Array([
  0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40
]);

// DATA_MIC_KEY: 4142434445464748494A4B4C4D4E4F50
const DATA_MIC_KEY = new Uint8Array([
  0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
  0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50
]);

// DATA_IV: 000102030405060708090A0B0C0D0E0F
const DATA_IV = new Uint8Array([
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
]);

// CMAC constant
const CMAC_RB = new Uint8Array([
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
]);

// ===== Constants (match secure_data.h) =====
const SECURE_DATA_PAYLOAD_LEN = 32;  // IMU data length for MIC placement
const SECURE_DATA_MIC_LEN = 4;       // 4 bytes MIC
const SECURE_DATA_TOTAL_LEN = 48;    // 3 AES blocks
const IMU_SERIALIZED_LEN = 33;       // id(1) + ax,ay,az,gx,gy,gz,dt,t_s (8*4=32) = 33

// ===== Utility Functions =====

function hexToBytes(hex) {
  const clean = hex.replace(/\s/g, '');
  const bytes = new Uint8Array(clean.length / 2);
  for (let i = 0; i < clean.length; i += 2) {
    bytes[i / 2] = parseInt(clean.substr(i, 2), 16);
  }
  return bytes;
}

function bytesToHex(bytes) {
  return Array.from(bytes).map(b => b.toString(16).padStart(2, '0').toUpperCase()).join(' ');
}

// ===== AES-CBC Decrypt (Web Crypto API) =====

async function aesCbcDecrypt(key, iv, ciphertext) {
  const cryptoKey = await crypto.subtle.importKey(
    'raw', key, { name: 'AES-CBC' }, false, ['decrypt']
  );

  // Web Crypto expects PKCS7 padding, but ESP32 uses zero padding
  // We need to handle this manually
  try {
    const decrypted = await crypto.subtle.decrypt(
      { name: 'AES-CBC', iv: iv },
      cryptoKey,
      ciphertext
    );
    return new Uint8Array(decrypted);
  } catch (e) {
    // If PKCS7 padding fails, decrypt block by block
    return await aesCbcDecryptNoPadding(key, iv, ciphertext);
  }
}

async function aesCbcDecryptNoPadding(key, iv, ciphertext) {
  const cryptoKey = await crypto.subtle.importKey(
    'raw', key, { name: 'AES-CBC' }, false, ['decrypt']
  );

  // Add fake PKCS7 padding block to make Web Crypto happy
  const padded = new Uint8Array(ciphertext.length + 16);
  padded.set(ciphertext);
  // Fill last block with 0x10 (16) for valid PKCS7
  for (let i = ciphertext.length; i < padded.length; i++) {
    padded[i] = 0x10;
  }

  // Encrypt the padding block to get correct ciphertext
  const encKey = await crypto.subtle.importKey(
    'raw', key, { name: 'AES-CBC' }, false, ['encrypt']
  );

  // Get last ciphertext block as IV for padding encryption
  const lastBlock = ciphertext.slice(ciphertext.length - 16);
  const paddingBlock = new Uint8Array(16).fill(0x10);
  const encryptedPadding = await crypto.subtle.encrypt(
    { name: 'AES-CBC', iv: lastBlock },
    encKey,
    paddingBlock
  );

  padded.set(new Uint8Array(encryptedPadding).slice(0, 16), ciphertext.length);

  const decrypted = await crypto.subtle.decrypt(
    { name: 'AES-CBC', iv: iv },
    cryptoKey,
    padded
  );

  return new Uint8Array(decrypted).slice(0, ciphertext.length);
}

// ===== AES-ECB for CMAC (via CBC with zero IV) =====

async function aesEcbEncryptBlock(key, block) {
  const cryptoKey = await crypto.subtle.importKey(
    'raw', key, { name: 'AES-CBC' }, false, ['encrypt']
  );
  const iv = new Uint8Array(16);
  const padded = new Uint8Array(32);
  padded.set(block);

  const encrypted = await crypto.subtle.encrypt(
    { name: 'AES-CBC', iv }, cryptoKey, padded
  );
  return new Uint8Array(encrypted).slice(0, 16);
}

// ===== CMAC-128 (matches ESP32 aes_cmac_128) =====

function leftShift128(input) {
  const out = new Uint8Array(16);
  let carry = 0;
  for (let i = 15; i >= 0; i--) {
    const c = (input[i] & 0x80) ? 1 : 0;
    out[i] = ((input[i] << 1) | carry) & 0xff;
    carry = c;
  }
  return out;
}

function xorBlocks(a, b) {
  const result = new Uint8Array(16);
  for (let i = 0; i < 16; i++) {
    result[i] = a[i] ^ b[i];
  }
  return result;
}

async function aesCmac128(key, msg, len) {
  // 1. L = AES(key, 0^128)
  const L = await aesEcbEncryptBlock(key, new Uint8Array(16));

  // 2. Generate K1
  let K1 = leftShift128(L);
  if (L[0] & 0x80) {
    K1 = xorBlocks(K1, CMAC_RB);
  }

  // 3. Generate K2
  let K2 = leftShift128(K1);
  if (K1[0] & 0x80) {
    K2 = xorBlocks(K2, CMAC_RB);
  }

  // 4. Process blocks
  const n = Math.max(1, Math.ceil(len / 16));
  const lastComplete = (len !== 0 && len % 16 === 0);

  let X = new Uint8Array(16);

  // CBC-MAC blocks 0..n-2
  for (let i = 0; i < n - 1; i++) {
    const blk = msg.slice(i * 16, (i + 1) * 16);
    X = xorBlocks(X, blk);
    X = await aesEcbEncryptBlock(key, X);
  }

  // Last block
  const lastBlock = new Uint8Array(16);
  const off = (n - 1) * 16;
  const rem = len > off ? len - off : 0;

  if (rem > 0) {
    lastBlock.set(msg.slice(off, off + rem));
  }

  let finalBlock;
  if (lastComplete) {
    finalBlock = xorBlocks(lastBlock, K1);
  } else {
    lastBlock[rem] = 0x80; // padding
    finalBlock = xorBlocks(lastBlock, K2);
  }

  X = xorBlocks(X, finalBlock);
  return await aesEcbEncryptBlock(key, X);
}

// ===== Parse IMU Sample =====

/**
 * IMUSample layout (matches ESP32 serializeIMUSample):
 * - id:   uint8_t  (1 byte)  offset 0
 * - ax:   float    (4 bytes) offset 1
 * - ay:   float    (4 bytes) offset 5
 * - az:   float    (4 bytes) offset 9
 * - gx:   float    (4 bytes) offset 13
 * - gy:   float    (4 bytes) offset 17
 * - gz:   float    (4 bytes) offset 21
 * - dt:   float    (4 bytes) offset 25
 * - t_s:  uint32_t (4 bytes) offset 29
 * Total: 33 bytes
 */
function parseImuSample(data) {
  const view = new DataView(data.buffer, data.byteOffset, data.byteLength);

  const id  = data[0];
  const ax  = view.getFloat32(1, true);   // little-endian
  const ay  = view.getFloat32(5, true);
  const az  = view.getFloat32(9, true);
  const gx  = view.getFloat32(13, true);
  const gy  = view.getFloat32(17, true);
  const gz  = view.getFloat32(21, true);
  const dt  = view.getFloat32(25, true);
  const t_s = view.getUint32(29, true);

  return { id, ax, ay, az, gx, gy, gz, dt, t_s };
}

// ===== Main Decrypt & Verify Function =====

/**
 * Decrypt cipher data from ESP32
 * Matches: ESP_Sender/src/secure_data.cpp secure_data_encrypt()
 *
 * @param {string} cipherHex - Hex string (96 chars = 48 bytes)
 * @returns {Object} { success, imu, error }
 */
async function decryptCipherData(cipherHex) {
  try {
    const ciphertext = hexToBytes(cipherHex);

    console.log("🔐 Cipher length:", ciphertext.length, "bytes");
    console.log("🔐 Cipher:", bytesToHex(ciphertext.slice(0, 16)), "...");

    if (ciphertext.length !== SECURE_DATA_TOTAL_LEN) {
      return {
        success: false,
        error: `Invalid length: ${ciphertext.length}, expected ${SECURE_DATA_TOTAL_LEN}`
      };
    }

    // 1. Decrypt AES-128-CBC
    const plaintext = await aesCbcDecrypt(DATA_AES_KEY, DATA_IV, ciphertext);

    console.log("🔓 Plaintext:", bytesToHex(plaintext));

    // 2. Extract MIC (4 bytes at offset SECURE_DATA_PAYLOAD_LEN = 32)
    const micReceived = plaintext.slice(SECURE_DATA_PAYLOAD_LEN, SECURE_DATA_PAYLOAD_LEN + SECURE_DATA_MIC_LEN);

    // 3. Reconstruct original data for CMAC verification
    // ESP32 calculates CMAC on serialized IMU data (33 bytes) BEFORE placing MIC
    // We need the original 33 bytes, but byte 32 was overwritten with MIC[0]
    // This is the actual data length used for CMAC
    const dataForCmac = new Uint8Array(IMU_SERIALIZED_LEN);
    dataForCmac.set(plaintext.slice(0, IMU_SERIALIZED_LEN));

    // 4. Calculate CMAC
    const cmacFull = await aesCmac128(DATA_MIC_KEY, dataForCmac, IMU_SERIALIZED_LEN);
    const micCalculated = cmacFull.slice(0, SECURE_DATA_MIC_LEN);

    console.log("📋 MIC received:  ", bytesToHex(micReceived));
    console.log("📋 MIC calculated:", bytesToHex(micCalculated));

    // 5. Verify MIC
    let micValid = true;
    for (let i = 0; i < SECURE_DATA_MIC_LEN; i++) {
      if (micReceived[i] !== micCalculated[i]) {
        micValid = false;
        break;
      }
    }

    if (!micValid) {
      console.warn("⚠️ MIC verification FAILED");
      // Still return data for debugging
      const imu = parseImuSample(plaintext);
      return { success: false, error: 'MIC verification failed', imu };
    }

    console.log("✅ MIC verification PASSED");

    // 6. Parse IMU sample
    const imu = parseImuSample(plaintext);

    console.log("📊 IMU:", imu);

    return { success: true, imu };

  } catch (err) {
    console.error("❌ Decrypt error:", err);
    return { success: false, error: err.message };
  }
}

// ===== Export to window =====
window.CryptoUtils = {
  decryptCipherData,
  hexToBytes,
  bytesToHex,
  parseImuSample,
  aesCmac128,
};
