/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd-circular-buffer.h"

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("githuberj"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");

    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    struct aesd_dev *aesd_device = filp->private_data;

    if (mutex_lock_interruptible(&aesd_device->mutex))
    {
        return -ERESTARTSYS;
    }

    struct aesd_buffer_entry *entry;
    size_t off;
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device->cbuf, *f_pos, &off);

    if (entry)
    {
        ssize_t sz = entry->size - off;
        if (sz > count)
        {
            sz = count;
        }

        if (copy_to_user(buf, entry->buffptr + off, sz) != 0)
        {
            retval = -EFAULT;
            goto exit;
        }
        *f_pos += sz;
        retval = sz;
    }
exit:
    mutex_unlock(&aesd_device->mutex);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    struct aesd_dev *aesd_device = filp->private_data;

    if (mutex_lock_interruptible(&aesd_device->mutex))
    {
        return -ERESTARTSYS;
    }

    char * new_buf = kmalloc(aesd_device->working_entry.size + count, GFP_KERNEL);
    if (!new_buf)
    {
        retval = -ENOMEM;
        goto exit;
    }

    if (aesd_device->working_entry.size)
    {
        memcpy(new_buf, aesd_device->working_entry.buffptr, aesd_device->working_entry.size);
    }

    if (copy_from_user(new_buf + aesd_device->working_entry.size, buf, count))
    {
        retval = -EFAULT;
        goto exit;
    }

    kfree(aesd_device->working_entry.buffptr);
    aesd_device->working_entry.buffptr = new_buf;
    aesd_device->working_entry.size += count;
    retval = count;

    while (aesd_device->working_entry.size > 0)
    {
        char *p = memchr(aesd_device->working_entry.buffptr, '\n', aesd_device->working_entry.size);
        struct aesd_buffer_entry entry;
        size_t sz;

        if (!p)
        {
            break;
        }

        sz = p + 1 - aesd_device->working_entry.buffptr;
        entry.buffptr = aesd_device->working_entry.buffptr;
        entry.size = sz;
        kfree(aesd_circular_buffer_add_entry(&aesd_device->cbuf, &entry));

        if (aesd_device->working_entry.size == sz)
        {
            aesd_device->working_entry.buffptr = NULL;
            aesd_device->working_entry.size = 0;
            break;
        }
        else
        {
            new_buf = kmemdup(aesd_device->working_entry.buffptr + sz, aesd_device->working_entry.size - sz, GFP_KERNEL);

            if (!new_buf)
            {
                retval = -ENOMEM;
                goto exit;
            }

            aesd_device->working_entry.buffptr = new_buf;
            aesd_device->working_entry.size -= sz;
        }
    }

exit:
    mutex_unlock(&aesd_device->mutex);

    return retval;
}
struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
                                 "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    mutex_init(&aesd_device.mutex);
    aesd_circular_buffer_init(&aesd_device.cbuf);

    result = aesd_setup_cdev(&aesd_device);

    if (result)
    {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    uint8_t idx;
    struct aesd_buffer_entry *entry;
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.cbuf, idx)
    {
        kfree(entry->buffptr);
    }

    kfree(aesd_device.working_entry.buffptr);

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
