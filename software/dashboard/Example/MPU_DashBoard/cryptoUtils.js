// cryptoUtils.js
const crypto = require("crypto");
const aesCmac = require("node-aes-cmac").aesCmac;

// ===== KEYS =====

const DATA_AES_KEY = Buffer.from([
 0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,
 0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,0x40
]);

const DATA_MIC_KEY = Buffer.from([
 0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,
 0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50
]);

const DATA_IV = Buffer.from([
 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
 0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F
]);

const SECURE_DATA_PAYLOAD_LEN = 33;
const SECURE_DATA_MIC_LEN = 4;
const SECURE_DATA_TOTAL_LEN = 48;

// ===== Utils =====

function hexToBytes(hex){
 return Buffer.from(hex.replace(/\s/g,''),"hex");
}

function bytesToHex(buf){
 return buf.toString("hex").toUpperCase();
}

// ===== AES CBC decrypt =====

function aesCbcDecrypt(ciphertext){

 const decipher = crypto.createDecipheriv(
  "aes-128-cbc",
  DATA_AES_KEY,
  DATA_IV
 );

 decipher.setAutoPadding(false);

 return Buffer.concat([
  decipher.update(ciphertext),
  decipher.final()
 ]);
}

// ===== Parse IMU =====

function parseImuSample(buf){

 return {

  id : buf.readUInt8(0),

  ax : buf.readFloatLE(1),
  ay : buf.readFloatLE(5),
  az : buf.readFloatLE(9),

  gx : buf.readFloatLE(13),
  gy : buf.readFloatLE(17),
  gz : buf.readFloatLE(21),

  dt : buf.readFloatLE(25),

  t_s : buf.readUInt32LE(29)

 };
}

// ===== MAIN =====

function decryptCipherData(cipherHex){

 try{

  const ciphertext = hexToBytes(cipherHex);

  if(ciphertext.length !== SECURE_DATA_TOTAL_LEN){
   return { success:false, error:"Invalid cipher length" };
  }

  // AES decrypt
  const plaintext = aesCbcDecrypt(ciphertext);

  // MIC received
  const micReceived = plaintext.slice(
   SECURE_DATA_PAYLOAD_LEN,
   SECURE_DATA_PAYLOAD_LEN + SECURE_DATA_MIC_LEN
  );

  // Data for CMAC
  const dataForCmac = plaintext.slice(0,SECURE_DATA_PAYLOAD_LEN);

  // Calculate CMAC
  const cmacFull = Buffer.from(
   aesCmac(DATA_MIC_KEY,dataForCmac,{returnAsBuffer:true})
  );

  const micCalculated = cmacFull.slice(0,4);

  const micValid = micReceived.equals(micCalculated);

  const imu = parseImuSample(plaintext);

  return {
   success: micValid,
   micValid,
   imu
  };

 }catch(err){

  return {
   success:false,
   error:err.message
  };

 }
}

module.exports = {
 decryptCipherData,
 bytesToHex
};