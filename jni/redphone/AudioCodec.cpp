#include "AudioCodec.h"

#include <speex/speex.h>
//#include <speex/speex_preprocess.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <android/log.h>

#define TAG "AudioCodec"

#define ECHO_TAIL_MILLIS 75

AudioCodec::AudioCodec() : enc(NULL), dec(NULL), aecm(NULL)
{ }

int AudioCodec::init() {
  enc = speex_encoder_init( speex_lib_get_mode( SPEEX_MODEID_NB ) );
  dec = speex_decoder_init( speex_lib_get_mode( SPEEX_MODEID_NB ) );

  WebRtcAecm_Create(&aecm);
  WebRtcAecm_Init(aecm, SPEEX_SAMPLE_RATE);

  if (enc == NULL) {
    __android_log_print(ANDROID_LOG_WARN, TAG, "Encoder failed to initialize!");
    return -1;
  }

  if (dec == NULL) {
    __android_log_print(ANDROID_LOG_WARN, TAG, "Decoder failed to initialize!");
    return -1;
  }

  if (aecm == NULL) {
    __android_log_print(ANDROID_LOG_WARN, TAG, "AECM failed to initialize!");
    return -1;
  }

  spx_int32_t config = 1;
  speex_decoder_ctl(dec, SPEEX_SET_ENH, &config);
  config = 0;
  speex_encoder_ctl(enc, SPEEX_SET_VBR, &config);
  config = 3;
  speex_encoder_ctl(enc, SPEEX_SET_QUALITY, &config);
  config = 1;
  speex_encoder_ctl(enc, SPEEX_SET_COMPLEXITY, &config);

  speex_encoder_ctl(enc, SPEEX_GET_FRAME_SIZE, &enc_frame_size );
  speex_decoder_ctl(dec, SPEEX_GET_FRAME_SIZE, &dec_frame_size );

  __android_log_print(ANDROID_LOG_WARN, TAG, "Encoding frame size: %d", enc_frame_size);
  __android_log_print(ANDROID_LOG_WARN, TAG, "Decoding frame size: %d", dec_frame_size);

  speex_bits_init(&enc_bits);
  speex_bits_init(&dec_bits);

  return 0;
}

AudioCodec::~AudioCodec() {
  speex_bits_destroy( &enc_bits );
  speex_bits_destroy( &dec_bits );

  if (enc != NULL) speex_encoder_destroy( enc );
  if (dec != NULL) speex_decoder_destroy( dec );
}

int AudioCodec::encode(short *rawData, char* encodedData, int maxEncodedDataLen) {
  short cleanData[SPEEX_FRAME_SIZE];

  WebRtcAecm_Process(aecm, rawData, NULL, cleanData, SPEEX_FRAME_SIZE, ECHO_TAIL_MILLIS);

  speex_bits_reset(&enc_bits);
  speex_encode_int(enc, (spx_int16_t *)cleanData, &enc_bits);

  return speex_bits_write(&enc_bits, encodedData, maxEncodedDataLen);
}

int AudioCodec::decode(char* encodedData, int encodedDataLen, short *rawData) {
  int rawDataOffset = 0;

  speex_bits_read_from(&dec_bits, encodedData, encodedDataLen);

  while (speex_decode_int(dec, &dec_bits, rawData + rawDataOffset) == 0) { // TODO bounds?
    WebRtcAecm_BufferFarend(aecm, rawData + rawDataOffset, dec_frame_size);
    rawDataOffset += dec_frame_size;
  }

  return rawDataOffset;
}