/******************************************************************************
 *
 *  Copyright (C) 2006-2013 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#include <string.h>

#include "gki.h"
#include "avrc_api.h"
#include "avrc_defs.h"
#include "avrc_int.h"
#include <cutils/log.h>

/*****************************************************************************
**  Global data
*****************************************************************************/

#if (AVRC_METADATA_INCLUDED == TRUE)

/*******************************************************************************
**
** Function         avrc_pars_vendor_rsp
**
** Description      This function parses the vendor specific commands defined by
**                  Bluetooth SIG
**
** Returns          AVRC_STS_NO_ERROR, if the message in p_data is parsed successfully.
**                  Otherwise, the error code defined by AVRCP 1.4
**
*******************************************************************************/
static tAVRC_STS avrc_pars_vendor_rsp(tAVRC_MSG_VENDOR *p_msg, tAVRC_RESPONSE *p_result, UINT8 *p_buf, UINT16 buf_len)
{
    if (p_msg->vendor_len < 4)
    {
        android_errorWriteLog(0x534e4554, "111450531");
        AVRC_TRACE_WARNING2("%s: message length %d too short: must be at least 4",
                            __func__, p_msg->vendor_len);
        return AVRC_STS_INTERNAL_ERR;
    }
    tAVRC_STS  status = AVRC_STS_NO_ERROR;
    UINT8   *p = p_msg->p_vendor_data;
    UINT16  len;
    UINT8   xx, yy;
    tAVRC_NOTIF_RSP_PARAM   *p_param;
    tAVRC_APP_SETTING       *p_app_set;
    tAVRC_APP_SETTING_TEXT  *p_app_txt;
    tAVRC_ATTR_ENTRY        *p_entry;
    UINT32  *p_u32;
    UINT8   *p_u8;
    UINT16  size_needed;
    UINT8 eventid=0;

    BE_STREAM_TO_UINT8 (p_result->pdu, p);
    p++; /* skip the reserved/packe_type byte */
    BE_STREAM_TO_UINT16 (len, p);
    AVRC_TRACE_DEBUG6("%s ctype:0x%x pdu:0x%x, len:%d/0x%x vendor_len=0x%x",
                      __func__, p_msg->hdr.ctype, p_result->pdu, len, len,
                      p_msg->vendor_len);
    if (p_msg->vendor_len < len + 4)
    {
        android_errorWriteLog(0x534e4554, "111450531");
        AVRC_TRACE_WARNING3("%s: message length %d too short: must be at least %d",
                            __func__, p_msg->vendor_len, len + 4);
        return AVRC_STS_INTERNAL_ERR;
    }

    if (p_msg->hdr.ctype == AVRC_RSP_REJ)
    {
        if (len < 1)
        {
            android_errorWriteLog(0x534e4554, "111450531");
            AVRC_TRACE_WARNING2("%s: invalid parameter length %d: must be at least 1",
                                __func__, len);
            return AVRC_STS_INTERNAL_ERR;
        }
        p_result->rsp.status = *p;
        return p_result->rsp.status;
    }

    switch (p_result->pdu)
    {
    /* case AVRC_PDU_REQUEST_CONTINUATION_RSP: 0x40 */
    /* case AVRC_PDU_ABORT_CONTINUATION_RSP:   0x41 */

#if (AVRC_ADV_CTRL_INCLUDED == TRUE)
    case AVRC_PDU_SET_ABSOLUTE_VOLUME:      /* 0x50 */
        if (len != 1)
            status = AVRC_STS_INTERNAL_ERR;
        else
        {
            BE_STREAM_TO_UINT8 (p_result->volume.volume, p);
        }
        break;
#endif /* (AVRC_ADV_CTRL_INCLUDED == TRUE) */

    case AVRC_PDU_REGISTER_NOTIFICATION:    /* 0x31 */
#if (AVRC_ADV_CTRL_INCLUDED == TRUE)
        if (len < 1) 
        {
            android_errorWriteLog(0x534e4554, "111450531");
            AVRC_TRACE_WARNING2("%s: invalid parameter length %d: must be at least 1",
                                __func__, len);
            return AVRC_STS_INTERNAL_ERR;
        }
        BE_STREAM_TO_UINT8 (eventid, p);
        if(AVRC_EVT_VOLUME_CHANGE==eventid
            && (AVRC_RSP_CHANGED==p_msg->hdr.ctype || AVRC_RSP_INTERIM==p_msg->hdr.ctype
            || AVRC_RSP_REJ==p_msg->hdr.ctype || AVRC_RSP_NOT_IMPL==p_msg->hdr.ctype))
        {
            if (len < 2) 
            {
                android_errorWriteLog(0x534e4554, "111450531");
                AVRC_TRACE_WARNING2("%s: invalid parameter length %d: must be at least 2",
                                    __func__, len);
                return AVRC_STS_INTERNAL_ERR;
            }
            p_result->reg_notif.status=p_msg->hdr.ctype;
            p_result->reg_notif.event_id=eventid;
            BE_STREAM_TO_UINT8 (p_result->reg_notif.param.volume, p);
        }
        AVRC_TRACE_DEBUG2("avrc_pars_vendor_rsp PDU reg notif response:event %x, volume %x",eventid,
            p_result->reg_notif.param.volume);
#endif /* (AVRC_ADV_CTRL_INCLUDED == TRUE) */
        break;
    default:
        status = AVRC_STS_BAD_CMD;
        break;
    }

    return status;
}

/*******************************************************************************
**
** Function         AVRC_ParsResponse
**
** Description      This function is a superset of AVRC_ParsMetadata to parse the response.
**
** Returns          AVRC_STS_NO_ERROR, if the message in p_data is parsed successfully.
**                  Otherwise, the error code defined by AVRCP 1.4
**
*******************************************************************************/
tAVRC_STS AVRC_ParsResponse (tAVRC_MSG *p_msg, tAVRC_RESPONSE *p_result, UINT8 *p_buf, UINT16 buf_len)
{
    tAVRC_STS  status = AVRC_STS_INTERNAL_ERR;
    UINT16  id;

    if (p_msg && p_result)
    {
        switch (p_msg->hdr.opcode)
        {
        case AVRC_OP_VENDOR:     /*  0x00    Vendor-dependent commands */
            status = avrc_pars_vendor_rsp(&p_msg->vendor, p_result, p_buf, buf_len);
            break;

        case AVRC_OP_PASS_THRU:  /*  0x7C    panel subunit opcode */
            status = avrc_pars_pass_thru(&p_msg->pass, &id);
            if (status == AVRC_STS_NO_ERROR)
            {
                p_result->pdu = (UINT8)id;
            }
            break;

        default:
            AVRC_TRACE_ERROR1("AVRC_ParsResponse() unknown opcode:0x%x", p_msg->hdr.opcode);
            break;
        }
        p_result->rsp.opcode = p_msg->hdr.opcode;
    p_result->rsp.status = status;
    }
    return status;
}


#endif /* (AVRC_METADATA_INCLUDED == TRUE) */
