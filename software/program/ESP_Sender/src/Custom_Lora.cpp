#include "Custom_Lora.h"

/*======================== LORA STATE ========================*/

// save transmission states between loops
extern int transmissionState;
/*  Declare variable */
extern SX1278 radio;
extern IMUSample nodeData[MAX_NODES];
extern SemaphoreHandle_t gLoraMutex;

/* General Function */
uint32_t calcCRC32(const void *data, size_t length) {
    const uint8_t *bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;   // initial value

    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;  // polynomial
            else
                crc >>= 1;
        }
    }

    return ~crc;  // final XOR
}

void radio_config_beacon() {
  // Beacon: kênh/sync word/preamble riêng, SF/BW có thể khác uplink
  radio.setFrequency(F_BCN);       // RadioLib nhận float MHz/kHz? (Hàm của bạn nhận MHz)
  radio.setSyncWord(SW_BCN);
  radio.setSpreadingFactor(SF_BCN);
  radio.setBandwidth(BW_BCN);
  radio.setPreambleLength(PREAMBLE_BCN);
}
void radio_config_uplink() {
  radio.setFrequency(F_UL);
  radio.setSyncWord(SW_UL);
  radio.setSpreadingFactor(SF_UL);
  radio.setBandwidth(BW_UL);
}
void InitLora(void)
{
    // initialize SX1278 with default settings
    
    int state = radio.begin();
    if (state == RADIOLIB_ERR_NONE) {
      
    } else {
      
      
      while (true) { delay(10); }
    }
    radio_config_beacon();
}

/* Send Data */
void lora_send_imusample(const IMUSample& s) {
  uint8_t buffer[128];  // đủ lớn cho IMUSample + CRC

  // 1) serialize IMUSample
  int payload_len = serializeIMUSample(s, buffer);

  // 2) calc CRC trên phần bytes của IMUSample
  uint32_t crc = calcCRC32(buffer, payload_len);

  // 3) append CRC32 (little-endian theo memcpy)
  memcpy(&buffer[payload_len], &crc, sizeof(crc));
  int total_len = payload_len + sizeof(crc);

  /* for (int i=0; i<sizeof(IMUSample); i++) {
      
  } */
  // 4) Gửi qua LoRa
  xSemaphoreTake(gLoraMutex, portMAX_DELAY);
  // 
  int state;
  void radio_config_uplink();
  state = radio.transmit((byte*)buffer, total_len);
  radio_config_beacon();
  /* Time delay for switch TX -> RX */
  radio.standby();
  delayMicroseconds(3000);   // 2–5 ms
  radio.startReceive();

  if (state == RADIOLIB_ERR_NONE) {
    // the packet was successfully transmitted
    

  /* } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
    // the supplied packet was longer than 256 bytes
    

  } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
    // timeout occurred while transmitting packet
     */

  } else {
    // some other error occurred
    
    // 

  }
  xSemaphoreGive(gLoraMutex);
}

/* Send Encrypted Data */
void lora_send_imusample_secure(const IMUSample& s) {
    uint8_t cipher[SECURE_DATA_TOTAL_LEN];

    // Encrypt the IMU sample
    if (!secure_data_encrypt(s, cipher)) {
        
        return;
    }

    xSemaphoreTake(gLoraMutex, portMAX_DELAY);
    radio_config_uplink();

    
    
    

    int state = radio.transmit((byte*)cipher, SECURE_DATA_TOTAL_LEN);

    /* Move to recieve beacon moed */
    radio_config_beacon();
    /* Time delay for switch TX -> RX */
    radio.standby();
    delayMicroseconds(3000);   // 2–5 ms
    radio.startReceive();

    if (state == RADIOLIB_ERR_NONE) {
        
    } else {
        
        
    }

    xSemaphoreGive(gLoraMutex);
}
int serializeIMUSample(const IMUSample& s, uint8_t* out) {
  int idx = 0;

  memcpy(&out[idx], &s.id, sizeof(s.id));      idx += sizeof(s.id);
  memcpy(&out[idx], &s.ax, sizeof(s.ax));      idx += sizeof(s.ax);
  memcpy(&out[idx], &s.ay, sizeof(s.ay));      idx += sizeof(s.ay);
  memcpy(&out[idx], &s.az, sizeof(s.az));      idx += sizeof(s.az);
  memcpy(&out[idx], &s.gx, sizeof(s.gx));      idx += sizeof(s.gx);
  memcpy(&out[idx], &s.gy, sizeof(s.gy));      idx += sizeof(s.gy);
  memcpy(&out[idx], &s.gz, sizeof(s.gz));      idx += sizeof(s.gz);
  memcpy(&out[idx], &s.dt, sizeof(s.dt));      idx += sizeof(s.dt);
  memcpy(&out[idx], &s.t_s, sizeof(s.t_s));    idx += sizeof(s.t_s);

  return idx; // độ dài phần dữ liệu IMUSample (chưa có CRC)
}


/* ========================Start Recieve Data======================== */
void lora_recieve_imusample(IMUSample &s)
{

}

int deserializeIMUSample(IMUSample& s, const uint8_t *buffer) {
  int idx = 0;

  memcpy(&s.id, &buffer[idx], sizeof(s.id));        idx += sizeof(s.id);
  memcpy(&s.ax, &buffer[idx], sizeof(s.ax));    idx += sizeof(s.ax);
  memcpy(&s.ay, &buffer[idx], sizeof(s.ay));    idx += sizeof(s.ay);
  memcpy(&s.az, &buffer[idx], sizeof(s.az));    idx += sizeof(s.az);
  memcpy(&s.gx, &buffer[idx], sizeof(s.gx));  idx += sizeof(s.gx);
  memcpy(&s.gy, &buffer[idx], sizeof(s.gy));  idx += sizeof(s.gy);
  memcpy(&s.gz, &buffer[idx], sizeof(s.gz));  idx += sizeof(s.gz);
  memcpy(&s.dt, &buffer[idx], sizeof(s.dt));      idx += sizeof(s.dt);
  memcpy(&s.t_s, &buffer[idx], sizeof(s.t_s));    idx += sizeof(s.t_s);
  memcpy(&s.crc, &buffer[idx], sizeof(s.crc));      idx += sizeof(s.crc);

  /* for (int i=0; i<sizeof(IMUSample); i++) {
      
  } */
  return idx;
}

/* ========================End Recieve Data======================== */
