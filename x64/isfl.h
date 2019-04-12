/*
 * InsydeFlash Linux Driver
 * File: isfl.h
 *
 * Copyright (C) 2005 - 2015 Insyde Software Corp.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _DEBUG_H_
#define _DEBUG_H_

#define DEVICE_NAME             "isfl"                  //InsydeFlash buffer
#define DEVICE_NAME_PATH        "/dev/"DEVICE_NAME      // Open device node on user-space
#define DEVICE_MAJOR_NUM        100
#define ISFB_FLASH_SIZE         ( 128 * 1024 )          // 128KB
#define MEMORY_BLOCK_SIZE       ( 64 * 1024 )           // 64KB
#define MAX_MEMORY_BLOCK_COUNT  512

struct _isfl
{
    unsigned long   smi_io;             // SMI Port
    unsigned long   mbuf_size;          // memory buffer size
    unsigned long   mbuf_paddr;         // memory buffer physical address
    unsigned long   smi_status;         // SW SMI Status Return
    unsigned long   enable_kbc;
    unsigned long   disable_kbc;
};

struct _mblock
{
    unsigned long   mbuf_PhyAddr;         // memory buffer index
    unsigned long   mbuf_size;          // memory buffer size
    unsigned char   pbyBuffer[MEMORY_BLOCK_SIZE];
};

#define IOCTL_ALLOCATE_MEMORY               _IOWR(DEVICE_MAJOR_NUM, 0, struct _isfl *)
#define IOCTL_FREE_MEMORY                   _IOW(DEVICE_MAJOR_NUM, 1, struct _isfl *)
#define IOCTL_WRITE_MEMORY                  _IOWR(DEVICE_MAJOR_NUM, 2, struct _isfl *)
#define IOCTL_READ_MEMORY                   _IOWR(DEVICE_MAJOR_NUM, 3, struct _isfl *)
#define IOCTL_GENERATE_SMI                  _IOW(DEVICE_MAJOR_NUM, 4, struct _isfl *)
#define IOCTL_GET_SMI_STATUS                _IOR(DEVICE_MAJOR_NUM, 5, struct _isfl *)
#define IOCTL_DISABLE_KBC                   _IOR(DEVICE_MAJOR_NUM, 6, struct _isfl *)
#define IOCTL_ENABLE_KBC                    _IOR(DEVICE_MAJOR_NUM, 7, struct _isfl *)
#define IOCTL_ALLOCATE_MULTI_BLOCK_MEMORY   _IOWR(DEVICE_MAJOR_NUM, 8, struct _isfl *)
#define IOCTL_FREE_MULTI_BLOCK_MEMORY       _IOW(DEVICE_MAJOR_NUM, 9, struct _isfl *)
#define IOCTL_WRITE_TO_PHYSICAL             _IOWR(DEVICE_MAJOR_NUM, 10, struct _mblock *)
#define IOCTL_READ_FROME_PHYSICAL           _IOWR(DEVICE_MAJOR_NUM, 11, struct _mblock *)

#endif 
