/*
 * InsydeFlash Linux Driver
 * File: isfl_drv.c
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/io.h>

typedef struct _PageAlloc {
	unsigned long addr;
	unsigned long PhyAddr;
	unsigned int  order;
}PageAlloc_t;

#include "isfl.h"

#define MAX_8042_LOOPS      100000

#define ISFL_VERSION        "0.0.05"
#define DRIVER_AUTHOR       "Howard Ho <howard.ho@insydesw.com.tw>"
#define DRIVER_DESC         "InsydeFlash tool for Linux"

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 36 )
static dev_t first;
static struct cdev c_dev;
static struct class *isfl_class;
#endif

static int              Device_Open = 0;
static unsigned char    *isfl_mem_buff;
static unsigned char    *g_ppbyMemoryBuffer[MAX_MEMORY_BLOCK_COUNT] = {0};
static PageAlloc_t      g_ppbyMemoryBuffer2[MAX_MEMORY_BLOCK_COUNT] = {{0,0}};
static int isfl_open(struct inode *, struct file *);
static int isfl_release(struct inode *, struct file *);
static ssize_t isfl_read(struct file *, char *, size_t, loff_t *);
static ssize_t isfl_write(struct file *file, const char *buff, size_t length, loff_t *offset);
//Flash will close kbc except show dialog
static void enable_a20_kbc(void);
static void disable_a20_kbc(void);
//static int empty_8042(void);
static inline void io_delay(void);

//Use this function to get the message from AP
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 36 )
    static long isfl_ioctl(struct file *file, unsigned int num, unsigned long arg)
#else
    static int isfl_ioctl(struct inode *inode, struct file *file, unsigned int num, unsigned long arg)
#endif
{
    unsigned long   isfl_phys_addr;           // Assgin to SW SMI
    struct _isfl    *isfl;
    int             isfl_size = sizeof( struct _isfl );
    unsigned char   buf[isfl_size];
    struct _mblock  *pBlock;
    int             iBlockStructureSize = sizeof( struct _mblock );
    unsigned char   *pbyBlockBuffer;
    unsigned char   *ch;
    int             i;
    unsigned long   mbuf_index;
    unsigned char   *vir_addr = 0;

    switch ( num )
    {
        case IOCTL_ALLOCATE_MULTI_BLOCK_MEMORY:
            ch = (unsigned char *)arg;
            for ( i = 0; i < isfl_size; i++ )
            {
                get_user( buf[i], ( ch + i ) );
            }
            isfl = (struct _isfl *)buf;

            mbuf_index = isfl->mbuf_paddr;  // use mbuf_paddr to store mbuf_index to backward compatible

            // Allocate memory buffer
            g_ppbyMemoryBuffer2[mbuf_index].order = get_order( isfl->mbuf_size );
            g_ppbyMemoryBuffer2[mbuf_index].addr = __get_free_pages( GFP_DMA32, g_ppbyMemoryBuffer2[mbuf_index].order );
            g_ppbyMemoryBuffer[mbuf_index] = (unsigned char *)g_ppbyMemoryBuffer2[mbuf_index].addr;
            if ( g_ppbyMemoryBuffer[mbuf_index] == NULL )
            {
              //printk( KERN_INFO "isfl ioctl IOCTL_ALLOCATE_MULTI_BLOCK_MEMORY allocate physical memory failed\n" );
                return -1;
            }
            memset( g_ppbyMemoryBuffer[mbuf_index], 0, isfl->mbuf_size );

            g_ppbyMemoryBuffer2[mbuf_index].PhyAddr = virt_to_phys( g_ppbyMemoryBuffer[mbuf_index] );
            isfl->mbuf_paddr = g_ppbyMemoryBuffer2[mbuf_index].PhyAddr;
            //printk( KERN_INFO "isfl alloc id=%d, PhyAddr=%x\n", mbuf_index, g_ppbyMemoryBuffer2[mbuf_index].PhyAddr );

            ch = (unsigned char *)arg;
            for ( i = 0; i < isfl_size; i++ )
            {
                put_user( buf[i], ( ch + i ) );
            }
            break;

        case IOCTL_FREE_MULTI_BLOCK_MEMORY:
            ch = (unsigned char *)arg;
            for ( i = 0; i < isfl_size; i++ )
            {
                get_user( buf[i], ( ch + i ) );
            }
            isfl = (struct _isfl *)buf;

            mbuf_index = -1;
            isfl_phys_addr = isfl->mbuf_paddr;
            for (i = 0; i < MAX_MEMORY_BLOCK_COUNT; i++)
              if (isfl_phys_addr == g_ppbyMemoryBuffer2[i].PhyAddr)
                mbuf_index = i;
            if (-1 == mbuf_index)
              return -1;

            // Free Memory
            if ( g_ppbyMemoryBuffer[mbuf_index] != NULL )
            {
                free_pages( g_ppbyMemoryBuffer2[mbuf_index].addr, g_ppbyMemoryBuffer2[mbuf_index].order );
                g_ppbyMemoryBuffer2[mbuf_index].addr = 0;
                g_ppbyMemoryBuffer2[mbuf_index].order = 0;
                g_ppbyMemoryBuffer[mbuf_index] = NULL;
            }
            break;

        case IOCTL_WRITE_TO_PHYSICAL:
            ch = (unsigned char *)arg;
            pbyBlockBuffer = kmalloc( iBlockStructureSize, GFP_DMA );
            if ( pbyBlockBuffer == NULL )
            {
              //printk( KERN_INFO "isfl ioctl IOCTL_WRITE_TO_PHYSICAL with NULL parameter\n" );
              return -1;
            }
            for ( i = 0; i < iBlockStructureSize; i++ )
            {
                get_user( pbyBlockBuffer[i], ( ch + i ) );
            }
            pBlock = (struct _mblock *)pbyBlockBuffer;
            isfl_phys_addr = pBlock->mbuf_PhyAddr;
            //printk( KERN_INFO "isfl write PhyAddr=%x\n", pBlock->mbuf_PhyAddr);
            //printk( KERN_INFO "isfl write len=%x\n", pBlock->mbuf_size);
            //printk( KERN_INFO "isfl write PhyAddr=%x\n", isfl_phys_addr );
            
            mbuf_index = -1;
            for (i = 0; i < MAX_MEMORY_BLOCK_COUNT; i++)
              if (isfl_phys_addr == g_ppbyMemoryBuffer2[i].PhyAddr)
                mbuf_index = i;
            if (-1 == mbuf_index)
            {
              //printk( KERN_INFO "isfl ioctl IOCTL_WRITE_TO_PHYSICAL with wrong physcial address\n" );
              return -1;
            }

            if ( g_ppbyMemoryBuffer[mbuf_index] == NULL )
            {
              //printk( KERN_INFO "isfl ioctl IOCTL_WRITE_TO_PHYSICAL with physcial address didn't allocate\n" );
              return -1;
            }

            for ( i = 0; i < pBlock->mbuf_size && i < ISFB_FLASH_SIZE; i++ )
            {
                g_ppbyMemoryBuffer[mbuf_index][i] = pBlock->pbyBuffer[i];
            }
            
            kfree( pbyBlockBuffer );
            break;

        case IOCTL_READ_FROME_PHYSICAL:
            ch = (unsigned char *)arg;
            pbyBlockBuffer = kmalloc( iBlockStructureSize, GFP_DMA );
            if ( pbyBlockBuffer == NULL )
            {
                return -1;
            }
            for ( i = 0; i < iBlockStructureSize; i++ )
            {
                get_user( pbyBlockBuffer[i], ( ch + i ) );
            }
            pBlock = (struct _mblock *)pbyBlockBuffer;
            isfl_phys_addr = pBlock->mbuf_PhyAddr;
            //printk( KERN_INFO "isfl read PhyAddr=%x\n", pBlock->mbuf_PhyAddr);
            //printk( KERN_INFO "isfl read len=%x\n", pBlock->mbuf_size);
            //printk( KERN_INFO "isfl read PhyAddr=%x\n", isfl_phys_addr );

            mbuf_index = -1;
            for (i = 0; i < MAX_MEMORY_BLOCK_COUNT; i++)
              if (isfl_phys_addr == g_ppbyMemoryBuffer2[i].PhyAddr)
                mbuf_index = i;
            if (-1 == mbuf_index)
              return -1;

            if ( g_ppbyMemoryBuffer[mbuf_index] == NULL )
            {
                return -1;
            }

            for ( i = 0; i < pBlock->mbuf_size && i < ISFB_FLASH_SIZE; i++ )
            {
                pBlock->pbyBuffer[i] = g_ppbyMemoryBuffer[mbuf_index][i];
            }
            
            for ( i = 0; i < iBlockStructureSize; i++ )
            {
                put_user( pbyBlockBuffer[i], ( ch + i ) );
            }
            kfree( pbyBlockBuffer );
            break;

        case IOCTL_ALLOCATE_MEMORY:
            //printk( KERN_INFO "IOCTL_ALLOCATE_MEMORY\n" );
            ch = (unsigned char *)arg;
            for ( i = 0; i < isfl_size; i++ )
            {
                get_user( buf[i], ( ch + i ) );
                //printk( KERN_INFO "%02X ", buf[i] );
            }
            //printk( KERN_INFO "\n" );
            isfl = (struct _isfl *)buf;

            /* Allocate a memory buffer */
            //printk( KERN_INFO "Allocate Memory size: %ld\n", isfl->mbuf_size );

            isfl_mem_buff = kmalloc( isfl->mbuf_size, GFP_DMA );        //20090605 - Keep OS allocate under 16M
            if ( isfl_mem_buff == NULL )
            {
                return -1;
            }
            memset( isfl_mem_buff, 0, isfl->mbuf_size );

            isfl_phys_addr = virt_to_phys( isfl_mem_buff );
            //printk( KERN_INFO "isfl: virt: 0x%p     phys: 0x%08lx\n", isfl_mem_buff, isfl_phys_addr );
            isfl->mbuf_paddr = isfl_phys_addr;
            //printk( KERN_INFO "Memory physical addr: 0x%08lx\n", isfl->mbuf_paddr );

            ch = (unsigned char *)arg;
            for ( i = 0; i < isfl_size; i++ )
            {
                //printk( KERN_INFO "%02X ", buf[i] );
                put_user( buf[i], ( ch + i ) );
            }
            //printk( KERN_INFO "\n" );
            break;

        case IOCTL_FREE_MEMORY:
            //printk( KERN_INFO "IOCTL_FREE_MEMORY\n" );
            //kfree( isfl_mem_buff );
            ch = (unsigned char *)arg;
            for ( i = 0; i < isfl_size; i++ )
            {
                get_user( buf[i], ( ch + i ) );
                //printk( KERN_INFO "%02X ", buf[i] );
            }
            
            isfl = (struct _isfl *)buf;
            isfl_phys_addr = isfl->mbuf_paddr;
            
            vir_addr = (unsigned char*)phys_to_virt (isfl_phys_addr);
            if ( vir_addr == NULL )
            {
              //printk( KERN_INFO "isfl ioctl IOCTL_READ_FROME_PHYSICAL with failure from phys_to_virt\n" );
              return -1;
            }

            kfree (vir_addr);
            break;

        case IOCTL_GENERATE_SMI:
            //printk( "IOCTL_GENERATE_SMI\n" );

            //printk( "size of (struct _isfl): %d\n", isfl_size );

            /* Get SMI initial data */
            ch = (unsigned char *)arg;
            for ( i = 0; i < isfl_size; i++ )
            {
                get_user( buf[i], ( ch + i ) );
                //printk( "%02X ", buf[i] );
            }
            //printk( "\n" );
            isfl = (struct _isfl *)buf;
            //printk( "SMI IO port: 0x%lX\n", isfl->smi_io );
            //printk( "Bufer Size: %ld\n", isfl->mbuf_size );
            break;

        case IOCTL_GET_SMI_STATUS:
            //printk( "IOCTL_GET_SMI_STATUS\n" );

            ch = (unsigned char *)arg;
            for ( i = 0; i < isfl_size; i++ )
            {
                //printk( "%02X ", buf[i] );
                put_user( buf[i], ( ch + i ) );
            }
            //printk( "\n" );
            break;

        case IOCTL_ENABLE_KBC:
            enable_a20_kbc();
            break;

        case IOCTL_DISABLE_KBC:
            disable_a20_kbc();
            break;
    }

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 36 )
    static DEFINE_MUTEX( isflash_mutex );
    static long isfl_ioctl_unlock(struct file *fp, unsigned int cmd, unsigned long arg)
    {
        long            ret;
        mutex_lock( &isflash_mutex );
        ret = isfl_ioctl( fp, cmd, arg );
        mutex_unlock( &isflash_mutex );
        return ret;
    }
#endif

static struct file_operations fops = {
    .read = isfl_read,
    .write = isfl_write,
    .open = isfl_open,
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 36 )
    .unlocked_ioctl = isfl_ioctl_unlock,
    .compat_ioctl = isfl_ioctl_unlock,
#else
    .ioctl = isfl_ioctl,
#endif
    .release = isfl_release
};

//Driver initialization
static int __init init_isfl(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 36 )
  first = MKDEV(DEVICE_MAJOR_NUM, 0);
  if (register_chrdev_region(first, 1, DEVICE_NAME) < 0)
  {
    return -1;
  }
  if ((isfl_class = class_create(THIS_MODULE, DEVICE_NAME)) == NULL)
  {
    unregister_chrdev_region(first, 1);
    return -1;
  }
  if (device_create(isfl_class, NULL, first, NULL, "isfl") == NULL)
  {
    class_destroy(isfl_class);
    unregister_chrdev_region(first, 1);
    return -1;
  }
  cdev_init(&c_dev, &fops);
  if (cdev_add(&c_dev, first, 1) == -1)
  {
    device_destroy(isfl_class, first);
    class_destroy(isfl_class);
    unregister_chrdev_region(first, 1);
    return -1;
  }
#else
  int ret = register_chrdev(DEVICE_MAJOR_NUM, DEVICE_NAME, &fops);
  if ( ret < 0 )
  {
    //printk( KERN_ALERT "Registering char device %s failed with %d\n", DEVICE_NAME, ret );
    return ret;
  }
#endif
  //printk ("<0> IOCTL_ALLOCATE_MEMORY               = %lx\n", IOCTL_ALLOCATE_MEMORY            ); 
  //printk ("<0> IOCTL_FREE_MEMORY                   = %lx\n", IOCTL_FREE_MEMORY                ); 
  //printk ("<0> IOCTL_WRITE_MEMORY                  = %lx\n", IOCTL_WRITE_MEMORY               ); 
  //printk ("<0> IOCTL_READ_MEMORY                   = %lx\n", IOCTL_READ_MEMORY                ); 
  //printk ("<0> IOCTL_GENERATE_SMI                  = %lx\n", IOCTL_GENERATE_SMI               ); 
  //printk ("<0> IOCTL_GET_SMI_STATUS                = %lx\n", IOCTL_GET_SMI_STATUS             ); 
  //printk ("<0> IOCTL_DISABLE_KBC                   = %lx\n", IOCTL_DISABLE_KBC                ); 
  //printk ("<0> IOCTL_ENABLE_KBC                    = %lx\n", IOCTL_ENABLE_KBC                 ); 
  //printk ("<0> IOCTL_ALLOCATE_MULTI_BLOCK_MEMORY   = %lx\n", IOCTL_ALLOCATE_MULTI_BLOCK_MEMORY); 
  //printk ("<0> IOCTL_FREE_MULTI_BLOCK_MEMORY       = %lx\n", IOCTL_FREE_MULTI_BLOCK_MEMORY    ); 
  //printk ("<0> IOCTL_COPY_BUFFER_TO_MEMORY         = %lx\n", IOCTL_COPY_BUFFER_TO_MEMORY      ); 
  //printk ("<0>  = %d\n", MAJOR(first));
  return 0;
}

static void __exit cleanup_isfl(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 36 )
  cdev_del(&c_dev);
  device_destroy(isfl_class, first);
  class_destroy(isfl_class);
  unregister_chrdev_region(first, 1);
#else
  unregister_chrdev( DEVICE_MAJOR_NUM, DEVICE_NAME );
#endif
}

static int isfl_open(struct inode *inode, struct file *file)
{
    if ( Device_Open )
    {
        return -EBUSY;
    }

    Device_Open++;
    try_module_get( THIS_MODULE );

    return 0;
}

static int isfl_release(struct inode *inode, struct file *file)
{
    Device_Open--;

    module_put( THIS_MODULE );

    return 0;
}

static ssize_t isfl_read(struct file *filp, char *buff, size_t len, loff_t *offset)
{
    int             bytes_read = 0;

    //printk( KERN_INFO "isfl_read\n" );

    if ( isfl_mem_buff == 0 )
    {
        //printk( KERN_ERR "Memory buffer in kernel space that maybe not be allocated!\n" );
        return -EFAULT;
    }

    while ( len )
    {
        put_user( isfl_mem_buff[bytes_read++], buff++ );
        len--;
    }

    //printk( KERN_INFO "isfl_read: read %d bytes, left %d\n", bytes_read, len );
    return bytes_read;
}

static ssize_t isfl_write(struct file *file, const char *buff, size_t len, loff_t *offset)
{
    int             i;

    //printk( KERN_INFO "isfl_write\n" );

    if ( isfl_mem_buff == 0 )
    {
        //printk( KERN_ERR "Memory buffer in kernel space that maybe not be allocated!\n" );
        return -EFAULT;
    }

    //printk( KERN_INFO "isfl_write: len: %d\n", len );

    for ( i = 0; i < len && i < ISFB_FLASH_SIZE; i++ )
    {
        get_user( *( isfl_mem_buff + i ), buff + i );
    }
#if 0
    for ( i = 0; i < len; i++ )
    {
        if ( ( i + 1 ) % 8 )
        {
            //printk( "%02x ", isfl_mem_buff[i] );
        }
        else if ( ( i + 1 ) %16 )
        {
            //printk( "%02x   ", isfl_mem_buff[i] );
        }
        else
        {
            //printk( "%02x\n", isfl_mem_buff[i] );
        }
    }
    //printk( "\n" );
#endif 
    return i;
}

static int empty_8042(void)
{
    u8              status;
    int             loops = MAX_8042_LOOPS;

    while ( loops-- )
    {
        io_delay();

        status = inb( 0x64 );
        if ( status & 1 )
        {
            /* Read and discard input data */
            io_delay();
            (void)inb( 0x60 );
        }
        else if ( !( status & 2 ) )
        {
            /* Buffers empty, finished! */
            return 0;
        }
    }

    return -1;
}

static void enable_a20_kbc(void)
{
    empty_8042();

    outb( 0x60, 0x64 );   // Command write
    empty_8042();

    outb( 0x47, 0x60 );   // A20 on
    empty_8042();
}

static void disable_a20_kbc(void)
{
    empty_8042();

    outb( 0x60, 0x64 ); // Command write
    empty_8042();

    outb( 0x74, 0x60 );   // A20 on
    empty_8042();
}

static inline void io_delay(void)
{
    const u16   DELAY_PORT = 0x80;
    asm volatile( "outb %%al,%0" : : "dN" (DELAY_PORT) );
}

module_init( init_isfl );
module_exit( cleanup_isfl );

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_VERSION( ISFL_VERSION );
MODULE_SUPPORTED_DEVICE( "Insyde EFI BIOS Flash" );
MODULE_LICENSE( "GPL" );
