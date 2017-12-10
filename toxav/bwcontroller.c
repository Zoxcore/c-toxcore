/*
 * Copyright © 2016-2018 The TokTok team.
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "bwcontroller.h"

#include "ring_buffer.h"

#include "../toxcore/logger.h"
#include "../toxcore/util.h"

#include <assert.h>
#include <errno.h>

#define BWC_PACKET_ID 196
#define BWC_SEND_INTERVAL_MS (1000)     /* 1s  */
#define BWC_REFRESH_INTERVAL_MS (2000) /* 2s */
#define BWC_AVG_PKT_COUNT 20

/**
 *
 */

typedef struct BWCCycle {
    uint32_t last_recv_timestamp; /* Last recv update time stamp */
    uint32_t last_sent_timestamp; /* Last sent update time stamp */
    uint32_t last_refresh_timestamp; /* Last refresh time stamp */

    uint32_t lost;
    uint32_t recv;
} BWCCycle;

typedef struct BWCRcvPkt {
    uint32_t packet_length_array[BWC_AVG_PKT_COUNT];
    RingBuffer *rb;
} BWCRcvPkt;

struct BWController_s {
    m_cb *mcb;
    void *mcb_user_data;

    Messenger *m;
    uint32_t friend_number;

    struct {
        uint32_t last_recv_timestamp; /* Last recv update time stamp */
        uint32_t last_sent_timestamp; /* Last sent update time stamp */
        uint32_t last_refresh_timestamp; /* Last refresh time stamp */

        uint32_t lost;
        uint32_t recv;
    } cycle;

    struct {
        uint32_t packet_length_array[BWC_AVG_PKT_COUNT];
        RingBuffer *rb;
    } rcvpkt; /* To calculate average received packet (this means split parts, not the full message!) */
};

struct BWCMessage {
    uint32_t lost;
    uint32_t recv;
};


int bwc_handle_data(Messenger *m, uint32_t friendnumber, const uint8_t *data, uint16_t length, void *object);
void send_update(BWController *bwc);

BWController *bwc_new(Messenger *m, uint32_t friendnumber, m_cb *mcb, void *mcb_user_data)
{
    BWController *retu = (BWController *)calloc(sizeof(struct BWController_s), 1);

    LOGGER_WARNING(m->log, "BWC: new");

    retu->mcb = mcb;
    retu->mcb_user_data = mcb_user_data;
    retu->m = m;
    retu->friend_number = friendnumber;
    retu->cycle.last_sent_timestamp = retu->cycle.last_refresh_timestamp = current_time_monotonic();
    retu->rcvpkt.rb = rb_new(BWC_AVG_PKT_COUNT);

    retu->cycle.lost = 0;
    retu->cycle.recv = 0;

    /* Fill with zeros */
    int i = 0;

    for (i = 0; i < BWC_AVG_PKT_COUNT; i++) {
        uint32_t *j = (retu->rcvpkt.packet_length_array + i);
        *j = 0;
        rb_write(retu->rcvpkt.rb, j, 0);
    }

    m_callback_rtp_packet(m, friendnumber, BWC_PACKET_ID, bwc_handle_data, retu);

    return retu;
}

void bwc_kill(BWController *bwc)
{
    if (!bwc) {
        return;
    }

    m_callback_rtp_packet(bwc->m, bwc->friend_number, BWC_PACKET_ID, NULL, NULL);

    rb_kill(bwc->rcvpkt.rb);
    free(bwc);
}

void bwc_feed_avg(BWController *bwc, uint32_t bytes)
{
    // DISABLE
    return;

    uint32_t *packet_length;
    uint8_t dummy;

    rb_read(bwc->rcvpkt.rb, (void **) &packet_length, &dummy);
    *packet_length = bytes;
    rb_write(bwc->rcvpkt.rb, packet_length, 0);
}

/*
 * this function name is confusing
 * it should be called for every packet that has lost bytes, and called with how many bytes are ok
 */
void bwc_add_lost(BWController *bwc, uint32_t bytes_received_ok)
{
    if (!bwc) {
        return;
    }

    // DISABLE
    return;

    if (!bytes_received_ok) {
        LOGGER_WARNING(bwc->m->log, "BWC lost(1): %d", (int)bytes_received_ok);

        uint32_t *avg_packet_length_array[BWC_AVG_PKT_COUNT];
        uint32_t count = 1;

        rb_data(bwc->rcvpkt.rb, (void **)avg_packet_length_array);

        int i = 0;

        for (i = 0; i < BWC_AVG_PKT_COUNT; i ++) {
            bytes_received_ok = bytes_received_ok + *(avg_packet_length_array[i]);

            if (*(avg_packet_length_array[i])) {
                count++;
            }
        }

        LOGGER_WARNING(bwc->m->log, "BWC lost(2): %d count: %d", (int)bytes_received_ok, (int)count);

        bytes_received_ok = bytes_received_ok / count;

        LOGGER_WARNING(bwc->m->log, "BWC lost(3): %d", (int)bytes_received_ok);
    }

    bwc->cycle.lost = bwc->cycle.lost + bytes_received_ok;
    send_update(bwc);
}

void bwc_add_lost_v3(BWController *bwc, uint32_t bytes_lost)
{
    if (!bwc) {
        return;
    }

    if (bytes_lost > 0) {
        LOGGER_DEBUG(bwc->m->log, "BWC lost(1): %d", (int)bytes_lost);

        bwc->cycle.lost = bwc->cycle.lost + bytes_lost;
        send_update(bwc);
    }
}


void bwc_add_recv(BWController *bwc, uint32_t recv_bytes)
{
    if (!bwc || !recv_bytes) {
        return;
    }

    // LOGGER_WARNING(bwc->m->log, "BWC recv: %d", (int)recv_bytes);

    bwc->cycle.recv = bwc->cycle.recv + recv_bytes;
    send_update(bwc);
}


void send_update(BWController *bwc)
{
    if (current_time_monotonic() - bwc->cycle.last_refresh_timestamp > BWC_REFRESH_INTERVAL_MS) {

        bwc->cycle.lost /= 10;
        bwc->cycle.recv /= 10;
        bwc->cycle.last_refresh_timestamp = current_time_monotonic();

    } else if (current_time_monotonic() - bwc->cycle.last_sent_timestamp > BWC_SEND_INTERVAL_MS) {

        if (bwc->cycle.lost) {
            LOGGER_INFO(bwc->m->log, "%p Sent update rcv: %u lost: %u percent: %f %%",
                        bwc, bwc->cycle.recv, bwc->cycle.lost,
                        (float)(((float) bwc->cycle.lost / (bwc->cycle.recv + bwc->cycle.lost)) * 100.0f));

            uint8_t bwc_packet[sizeof(struct BWCMessage) + 1];
            struct BWCMessage *msg = (struct BWCMessage *)(bwc_packet + 1);

            bwc_packet[0] = BWC_PACKET_ID; // set packet ID
            msg->lost = net_htonl(bwc->cycle.lost);
            msg->recv = net_htonl(bwc->cycle.recv);

            if (-1 == m_send_custom_lossy_packet(bwc->m, bwc->friend_number, bwc_packet, sizeof(bwc_packet))) {
                LOGGER_WARNING(bwc->m->log, "BWC send failed (len: %d)! std error: %s", sizeof(bwc_packet), strerror(errno));
            }
        }

        bwc->cycle.last_sent_timestamp = current_time_monotonic();
    }
}

static int on_update(BWController *bwc, const struct BWCMessage *msg)
{
    LOGGER_DEBUG(bwc->m->log, "%p Got update from peer", bwc);

    /* Peers sent update too soon */
    if ((bwc->cycle.last_recv_timestamp + BWC_SEND_INTERVAL_MS) > current_time_monotonic()) {
        LOGGER_INFO(bwc->m->log, "%p Rejecting extra update", bwc);
        return -1;
    }

    bwc->cycle.last_recv_timestamp = current_time_monotonic();

    uint32_t recv = net_ntohl(msg->recv);
    uint32_t lost = net_ntohl(msg->lost);

    // LOGGER_INFO(bwc->m->log, "recved: %u lost: %u", recv, lost);

    if (lost && bwc->mcb) {

        LOGGER_INFO(bwc->m->log, "recved: %u lost: %u percentage: %f %%", recv, lost,
                    (float)(((float) lost / (recv + lost)) * 100.0f));

        bwc->mcb(bwc, bwc->friend_number,
                 ((float) lost / (recv + lost)),
                 bwc->mcb_user_data);
    }

    return 0;
}

int bwc_handle_data(Messenger *m, uint32_t friendnumber, const uint8_t *data, uint16_t length, void *object)
{
    if (sizeof(struct BWCMessage) != (length - 1)) {
        return -1;
    }

    return on_update((BWController *)object, (const struct BWCMessage *)(data + 1));
}
