/*
 * Copyright © 2016-2017 The TokTok team.
 * Copyright © 2013-2015 Tox project.
 *
 * This file is part of Tox, the free peer to peer instant messenger.
 *
 * Tox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Tox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tox.  If not, see <http://www.gnu.org/licenses/>.
 */


#define DISABLE_H264_DECODER_FEATURE    0

// H264 settings -----------
#define VIDEO_BITRATE_INITIAL_VALUE_H264 1500
#define VIDEO_BITRATE_MIN_AUTO_VALUE_H264 95
#define VIDEO_BITRATE_SCALAR_AUTO_VALUE_H264 1400
#define VIDEO_BITRATE_SCALAR_INC_BY_AUTO_VALUE_H264 200
#define VIDEO_BITRATE_SCALAR2_AUTO_VALUE_H264 5000
#define VIDEO_BITRATE_SCALAR2_INC_BY_AUTO_VALUE_H264 15
#define VIDEO_BITRATE_SCALAR3_AUTO_VALUE_H264 7000
#define VIDEO_BITRATE_MAX_AUTO_VALUE_H264 8000

#ifdef HW_CODEC_CONFIG_RPI3_TBW_BIDI
    // lower max video bitrate on ToxPhone
    #undef VIDEO_BITRATE_MAX_AUTO_VALUE_H264
    #define VIDEO_BITRATE_MAX_AUTO_VALUE_H264 3100
#endif

// -- these control how agressive the bandwidth control is --
#define VIDEO_BITRATE_AUTO_INC_THRESHOLD 1.7 // threshold loss % to increase bitrate (in %/100)
#define VIDEO_BITRATE_AUTO_DEC_THRESHOLD 5.1 // threshold loss % to lower the bitrate (in %/100)
#define VIDEO_BITRATE_AUTO_INC_TO 1.05 // increase video bitrate by n% (in %/100)
#define VIDEO_BITRATE_AUTO_DEC_FACTOR 0.93 // (in %/100)
// -- these control how agressive the bandwidth control is --

#define VIDEO_MAX_KF_H264 200
#define VIDEO_BUF_FACTOR_H264 1
#define VIDEO_F_RATE_TOLERANCE_H264 1.3
#define VIDEO_BITRATE_FACTOR_H264 0.7
// H264 settings -----------

#define TOXAV_ENCODER_CODEC_HW_ACCEL_NONE 0
#define TOXAV_ENCODER_CODEC_HW_ACCEL_OMX_PI 1

#define VIDEO_BITRATE_MAX_AUTO_VALUE_VP8 6000
#define VIDEO_BITRATE_MIN_AUTO_VALUE_VP8 200
#define VIDEO_BITRATE_CORRECTION_FACTOR_VP8 (float)1


typedef struct ToxAVCall_s {
    ToxAV *av;

    pthread_mutex_t mutex_audio[1];
    RTPSession *audio_rtp;
    ACSession *audio;

    pthread_mutex_t mutex_video[1];
    RTPSession *video_rtp;
    VCSession *video;

    BWController *bwc;

    uint8_t skip_video_flag;

    bool active;
    MSICall *msi_call;
    uint32_t friend_number;

    uint32_t audio_bit_rate; /* Sending audio bit rate */
    uint32_t video_bit_rate; /* Sending video bit rate */
    uint32_t video_bit_rate_last_last_changed; // only for callback info
    uint32_t video_bit_rate_last_last_changed_cb_ts;

    uint64_t last_incoming_video_frame_rtimestamp;
    uint64_t last_incoming_video_frame_ltimestamp;

    uint64_t last_incoming_audio_frame_rtimestamp;
    uint64_t last_incoming_audio_frame_ltimestamp;

    int64_t call_timestamp_difference_to_sender;
    int64_t call_timestamp_difference_adjustment;
    uint32_t call_rountrip_time_ms;

    uint64_t reference_rtimestamp;
    uint64_t reference_ltimestamp;
    int64_t reference_diff_timestamp;
    uint8_t reference_diff_timestamp_set;

    /** Required for monitoring changes in states */
    uint8_t previous_self_capabilities;

    pthread_mutex_t mutex[1];

    struct ToxAVCall_s *prev;
    struct ToxAVCall_s *next;
} ToxAVCall;


struct ToxAV {
    Tox *tox;
    Messenger *m;
    MSISession *msi;

    bool toxav_audio_iterate_seperation_active;

    /* Two-way storage: first is array of calls and second is list of calls with head and tail */
    ToxAVCall **calls;
    uint32_t calls_tail;
    uint32_t calls_head;
    pthread_mutex_t mutex[1];

    /* Call callback */
    toxav_call_cb *ccb;
    void *ccb_user_data;
    /* Call_comm callback */
    toxav_call_comm_cb *call_comm_cb;
    void *call_comm_cb_user_data;
    /* Call state callback */
    toxav_call_state_cb *scb;
    void *scb_user_data;
    /* Audio frame receive callback */
    toxav_audio_receive_frame_cb *acb;
    void *acb_user_data;
    /* Video frame receive callback */
    toxav_video_receive_frame_cb *vcb;
    void *vcb_user_data;
    /* Bit rate control callback */
    toxav_bit_rate_status_cb *bcb;
    void *bcb_user_data;
    /* Bit rate control callback */
    toxav_audio_bit_rate_cb *abcb;
    void *abcb_user_data;
    /* Bit rate control callback */
    toxav_video_bit_rate_cb *vbcb;
    void *vbcb_user_data;

    /** Decode time measures */
    int32_t dmssc; /** Measure count */
    int32_t dmsst; /** Last cycle total */
    int32_t dmssa; /** Average decoding time in ms */

    uint32_t interval; /** Calculated interval */
};

