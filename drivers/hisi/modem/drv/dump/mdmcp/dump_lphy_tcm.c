/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2012-2015. All rights reserved.
 * foss@huawei.com
 *
 * If distributed as part of the Linux kernel, the following license terms
 * apply:
 *
 * * This program is free software; you can redistribute it and/or modify
 * * it under the terms of the GNU General Public License version 2 and
 * * only version 2 as published by the Free Software Foundation.
 * *
 * * This program is distributed in the hope that it will be useful,
 * * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * * GNU General Public License for more details.
 * *
 * * You should have received a copy of the GNU General Public License
 * * along with this program; if not, write to the Free Software
 * * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Otherwise, the following license terms apply:
 *
 * * Redistribution and use in source and binary forms, with or without
 * * modification, are permitted provided that the following conditions
 * * are met:
 * * 1) Redistributions of source code must retain the above copyright
 * *    notice, this list of conditions and the following disclaimer.
 * * 2) Redistributions in binary form must reproduce the above copyright
 * *    notice, this list of conditions and the following disclaimer in the
 * *    documentation and/or other materials provided with the distribution.
 * * 3) Neither the name of Huawei nor the names of its contributors may
 * *    be used to endorse or promote products derived from this software
 * *    without specific prior written permission.
 *
 * * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/timer.h>
#include <linux/thread_info.h>
#include <linux/syslog.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <asm/string.h>
#include "securec.h"
#include "osl_types.h"
#include "osl_bio.h"
#include "osl_io.h"
#include "bsp_rfile.h"
#include "bsp_dspload.h"
#include "bsp_dsp.h"
#include "bsp_dump.h"
#include "bsp_ddr.h"
#include "dump_file.h"
#include "dump_print.h"
#include "dump_config.h"
#include <bsp_slice.h>
#include <bsp_nvim.h>
#include <dump_lphy_tcm.h>

struct dsp_dump
{
    u8  sect_check[NXP_MAX_SECTNUM];
    u32 file_size;
    u32 mini_log_flag;
};

char * lphy_image_ddr_addr = NULL;
char * lphy1_image_ddr_addr = NULL;
char * lphy2_image_ddr_addr = NULL;

struct dsp_dump g_dsp_dump_ctrl;

/* ���Ӷε�CRCУ��2017/08/29 */
u8 dump_sect_check_crc8(u8 *data, u32 len)
{
    u8 crc8 = 0;
    u32 i;

    for(i=0; i<len; i++)
    {
        crc8 += *data;
        data ++;
    }

    return crc8;
}
/*****************************************************************************
* �� �� ��  : dump_image_head_edit
* ��������  : �ı�������ͷ����2017/09/19
*
* �������  :file_name:�޸ĵ��ļ���;offset:����ļ�ͷ��ƫ��;ptr: д�������
* �������  :��

* �� �� ֵ  :��
*****************************************************************************/
void dump_image_head_edit(char * file_name, u32 offset, void *ptr)
{
    int fd;
    s32 ret;
    fd = bsp_open((s8*)file_name, RFILE_RDWR, 0755);
    if(fd < 0)
    {
        dump_fetal("[dump]: bsp_open %s failed! \n", file_name);
    }
    ret = bsp_lseek(fd, offset, SEEK_SET);
    if(ret != offset)
    {
        dump_fetal("[dump]: bsp_lseek failed! ret = 0x%x, offset = 0x%x \n", ret, offset);
    }
    ret = bsp_write(fd, (s8 *)ptr, sizeof(u32));
    if(ret != (sizeof(u32)))
    {
        dump_fetal("[dump]: bsp_write failed! ret = 0x%x \n", ret);
    }
    ret = bsp_close(fd);
    if(ret)
    {
        dump_fetal("[dump]: bsp_close failed! ret =  0x%x \n", ret);
    }
}
/*****************************************************************************
* �� �� ��  : dump_nxp_mini_log
*
* ��������  : ��Сָ��NXDSP�ı������ݣ�ͨ��ƴ�Ӷ�����ʵ��
*
* �� �� ֵ  : 0���ɹ�����0��ʧ��
*
* ����˵��  : ����C���쳣���ݡ�����ӿڳ���
*
*****************************************************************************/
int dump_nxp_mini_log(char * file_name, void * addr, char* dst_path)
{
    u32 i, tmp_time, psest_offset, tmp = 0;
    struct dsp_sect_desc_stru *psect = NULL;
    struct dsp_bin_header_stru *pheader = NULL;
    u32 ulFileSize_offset, ulFileOffset_offset, ulSectSize_offset;

    pheader = (struct dsp_bin_header_stru *)addr;
    if(NULL == pheader)
    {
        return BSP_ERROR;
    }
    /* ����㾵��ͷ���������Խ�����˳� 2017/09/27 */
    if((pheader->ulSectNum > NXP_MAX_SECTNUM) || (pheader->ulFileSize > DDR_TLPHY_IMAGE_SIZE))
    {
        dump_fetal("pheader->ulSectNum = 0x%x ,pheader->ulFileSize = 0x%x ,it's out of range\n", pheader->ulSectNum, pheader->ulFileSize);
        return BSP_ERROR;
    }
    psect = &pheader->astSect[0];
    /* ���ȼ���ƫ���� */
    ulFileSize_offset = (u32)((uintptr_t)(&(pheader->ulFileSize)) - (uintptr_t)pheader);
    ulFileOffset_offset = (u32)((uintptr_t)(&(psect->ulFileOffset)) - (uintptr_t)psect);
    ulSectSize_offset = (u32)((uintptr_t)(&(psect->ulSectSize)) - (uintptr_t)psect);

    g_dsp_dump_ctrl.file_size = sizeof(struct dsp_bin_header_stru) + sizeof(struct dsp_sect_desc_stru)*(pheader->ulSectNum);

    /* ���澵����ddr���� */
    dump_append_file(dst_path, file_name, addr, g_dsp_dump_ctrl.file_size, pheader->ulFileSize);
    for(i = 0; i < pheader->ulSectNum; i++)
    {
        psect = &pheader->astSect[i];

        /* �����п�����sizeΪ0�Ķ� */
        if(0 == psect->ulSectSize)
        {
            continue;
        }
        if((psect->ulFileOffset + psect->ulSectSize) > DDR_TLPHY_IMAGE_SIZE)
        {
            dump_fetal("ulFileOffset: 0x%x, ulSectSize: 0x%x, psectmax: 0x%x, out of range\n",
                psect->ulFileOffset, psect->ulSectSize, DDR_TLPHY_IMAGE_SIZE);
        }
        psest_offset = sizeof(struct dsp_bin_header_stru) + sizeof(struct dsp_sect_desc_stru)*i;
        /* ����λ��־�жϣ�1Ϊ����λ������λ�Ķβ���Ҫ���� 2017/08/29 */
        if(1 == psect->ucLoadStoreType.ucSectOnSite)
        {
            /* ����ҪУ�飬����������ݣ���SIZE = 0 */
            tmp = 0;
            dump_image_head_edit(file_name, (psest_offset + ulFileOffset_offset), &g_dsp_dump_ctrl.file_size);
            dump_image_head_edit(file_name, (psest_offset + ulSectSize_offset), &tmp);
            continue;
        }
        else
        {
            /* ����㾵��ͷ���������Խ�����˳� 2017/09/27 */
            if(psect->ucTcmType > 3)
            {
                dump_fetal("psect->ucTcmType = 0x%x ,it's out of range\n", psect->ucTcmType);
                return BSP_ERROR;
            }
            /* ɾ��log�е�δ�仯text��rodata�Σ��������޸�logͷ���� 2017/08/29 */
            if((0 == psect->ucTcmType) || (1 == psect->ucTcmType))
            {
                g_dsp_dump_ctrl.sect_check[i] = dump_sect_check_crc8((u8 *)((uintptr_t)(pheader) + (uintptr_t)(psect->ulFileOffset)), psect->ulSectSize);
                if(psect->ucCrc8 == g_dsp_dump_ctrl.sect_check[i])
                {
                    /* ���У��ɹ�������������ݣ���SIZE = 0 */
                    tmp = 0;
                    dump_image_head_edit(file_name, (psest_offset + ulFileOffset_offset), &g_dsp_dump_ctrl.file_size);
                    dump_image_head_edit(file_name, (psest_offset + ulSectSize_offset), &tmp);
                    continue;
                }
                else if(dump_get_edition_type() != EDITION_INTERNAL_BETA)
                {
                    /* ���У��ʧ�ܣ�����������ݽ�����ʶ */
                    tmp = 0xffffffff;
                    dump_image_head_edit(file_name, (psest_offset + ulFileOffset_offset), &g_dsp_dump_ctrl.file_size);
                    dump_image_head_edit(file_name, (psest_offset + ulSectSize_offset), &tmp);
                    dump_fetal("sect_num: 0x%x   sect_check: 0x%x != psect->ucCrc8: 0x%x, not same!\n", i, g_dsp_dump_ctrl.sect_check[i], psect->ucCrc8);
                    continue;
                }
            }
        }
        /* ���澵����ddr���� */
        dump_append_file(dst_path, file_name, (void*)((uintptr_t)pheader + (uintptr_t)(psect->ulFileOffset)), psect->ulSectSize, pheader->ulFileSize);
        dump_image_head_edit(file_name, (psest_offset + ulFileOffset_offset), &g_dsp_dump_ctrl.file_size);
        g_dsp_dump_ctrl.file_size += psect->ulSectSize;
    }
    /* �ı��ļ����ܴ�С */
    dump_image_head_edit(file_name, ulFileSize_offset, &g_dsp_dump_ctrl.file_size);
    tmp_time = bsp_get_slice_value();
    writel(tmp_time, &g_dsp_dump_ctrl.mini_log_flag);
    return BSP_OK;
}
/*****************************************************************************
* �� �� ��  : dump_save_lphy_log
* ��������  : ����lphyȫ����log�ļ�
*
* �������  :
* �������  :

* �� �� ֵ  :
*****************************************************************************/
void dump_save_lphy_log(char* data,char* dst_path)
{
    int ret = 0;
    char file_name[128] = {0};
    /* coverity[HUAWEI DEFECT] */
    memset_s(file_name, sizeof(file_name), 0, sizeof(file_name));
    /* coverity[HUAWEI DEFECT] */
    snprintf_s(file_name, sizeof(file_name), (sizeof(file_name)-1), "%slphy_dump.bin", dst_path);
    file_name[127]='\0';
    ret = dump_nxp_mini_log(file_name, data, dst_path);
    if(ret)
    {
        dump_fetal("dump_nxp_mini_log failed, ret = %x \n", ret);
    }
    dump_fetal("[dump]: save %s finished\n", file_name);
}
/*****************************************************************************
* �� �� ��  : dump_save_all_tcm
* ��������  : ����ȫ����tcm�ļ�
*
* �������  :
* �������  :

* �� �� ֵ  :

*
* �޸ļ�¼  : 2016��1��4��17:05:33   lixiaofan  creat
*
*****************************************************************************/
void dump_save_all_tcm(char* data,char* dst_path)
{
}

/*****************************************************************************
* �� �� ��  : dump_save_some_tcm
* ��������  : ����ȫ����dtcm��itcm�ļ�
*
* �������  :
* �������  :

* �� �� ֵ  :

*
* �޸ļ�¼  : 2016��1��4��17:05:33   lixiaofan  creat
*
*****************************************************************************/
void dump_save_some_tcm(char* data,char* dst_path)
{
}
/*****************************************************************************
* �� �� ��  : dump_save_lphy_tcm
* ��������  : ����tldsp�ľ���
*
* �������  :
* �������  :

* �� �� ֵ  :

*
* �޸ļ�¼  : 2016��1��4��17:05:33   lixiaofan  creat
*
*****************************************************************************/
void dump_save_lphy_tcm(char * dst_path)
{
    DUMP_FILE_CFG_STRU* cfg = dump_get_file_cfg();

    if(DUMP_PHONE == dump_get_product_type()
        && DUMP_ACCESS_MDD_DDR_NON_SEC != dump_get_access_mdmddr_type())
    {
        return;
    }

    if(cfg->file_list.file_bits.lphy_tcm == 0)
    {
        return;
    }
    lphy_image_ddr_addr = (char *)ioremap_wc(NXDSP_MDDR_FAMA(DDR_TLPHY_IMAGE_ADDR), DDR_TLPHY_IMAGE_SIZE);
    if(NULL == lphy_image_ddr_addr)
    {
        dump_fetal("ioremap DDR_TLPHY_IMAGE_ADDR fail\n");
        return;
    }
    dump_save_lphy_log(lphy_image_ddr_addr,dst_path);
    iounmap(lphy_image_ddr_addr);
    return;
}

