/*
 * a simple char device driver: globalfifo without mutex
 *
 * copyright (c) 2017 Nick Yan
 *
 * Licensed under GPLv2 or later
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/device.h>

#define GLOBALFIFO_SIZE			0x1000  /*全局内存大小，用于模拟读写操作的内存区域*/
#define MEM_CLEAR				0x1     /*ioctl操作命令                      */
#define GLOBALFIFO_MAJOR		230     /*主设备号                           */
#define DEVICE_NUM              10      /*设备数目                           */

static int globalfifo_major = GLOBALFIFO_MAJOR;
module_param(globalfifo_major, int, S_IRUGO);    /*声明insmod时的参数*/

struct globalfifo_dev {
	struct cdev cdev;                   /*字符设备结构体*/
    unsigned int current_len;           /*记录当然FIFO中的数据长度*/
	unsigned char mem[GLOBALFIFO_SIZE]; /*用于模拟读写操作的内存空间*/
    struct mutex mutex;                 /*用于多用户(进程)访问时的控制，不能用自旋锁，因为读写操作中有调用可能导致阻塞的copy_to_user及copy_from_user; 只能使用互斥体*/
    wait_queue_head_t r_wait;           /*定义读取等待队列头部*/
    wait_queue_head_t w_wait;           /*定义写入等待队列头部*/
};

static struct globalfifo_dev *globalfifo_devp;

/*
 *文件打开函数，对应于用户空间的open函数，用户空间调用open函数时，系统内部经过各种处理后，最终调用本函数
 */
static int globalfifo_open(struct inode *inode, struct file *filep)
{
    /*获取包含cdev结构体的globalfifo_dev结构体指针*/
    struct globalfifo_dev *dev = container_of(inode->i_cdev, struct globalfifo_dev, cdev);
    filep->private_data = dev;
    return 0;
}

/*
 *文件释放函数
 *release()完成与open()相反的工作，释放open()向内核申请的所有资源，实际使用过程中，需要注意两点： 
 *1.close()并不完全是release() 
 *  内核对每个file结构维护其被使用的计数，无论是fork还是dup，都不会创建新的数据结构，如fork和dup，
 *  它们只是增加已有(由open()生成)结构中的计数，只有在file结构的计数为0时，close才会实质地调用release,
 *  因此并不是每个close()系统调用都会引起对release()方法的调用
 *2.内核自动关闭 
 *  内核在进程退出时，会在内部使用close()系统调用自动关闭所有相关文件 
 */
static int globalfifo_release(struct inode *inode, struct file *filep)
{
	return 0;
}

/*
 *ioctl设备控制函数
 */
static long golbalfifo_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct globalfifo_dev *dev = filep->private_data;

	switch(cmd) {
    case MEM_CLEAR:
        mutex_lock(&dev->mutex);

		memset(dev->mem, 0, GLOBALFIFO_SIZE);
		printk(KERN_INFO "globalfifo is set to zero\n");

        mutex_unlock(&dev->mutex);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
 *读取设备函数
 */
static ssize_t globalfifo_read(struct file *filp, char __user * buf, size_t count, loff_t *ppos)
{
    int ret = 0;
    struct globalfifo_dev *dev = filp->private_data;    /*获取设备结构体指针*/

    DECLARE_WAITQUEUE(wait, current);

    mutex_lock(&dev->mutex);
    add_wait_queue(&dev->r_wait, &wait);

    while (0 == dev->current_len) {
        if (filp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto out;
        }
        __set_current_state(TASK_INTERRUPTIBLE);
        mutex_unlock(&dev->mutex);

        schedule();
        if (signal_pending(current)) {
            ret = -ERESTARTSYS;
            goto out2;
        }

        mutex_lock(&dev->mutex);
    }

    if (count > dev->current_len) {
        count = dev->current_len;
    }
    
    /*buf为用户空间指针，不能直接使用memcpy()等方法，内核空间不能直接访问用户空间*/
    /*copy_to_user：完成数据从内核空间向用户空间的复制，可能引起阻塞*/
    if (copy_to_user(buf, dev->mem, count)) {
        ret = -EFAULT;
        goto out;
    } else {
        memcpy(dev->mem, dev->mem + count, dev->current_len - count);
        dev->current_len -= count;

        printk(KERN_INFO "read %ld bytes, current_len: %d\n", count, dev->current_len);

        wake_up_interruptible(&dev->w_wait);    /*读取数据后，FIFO中会空闲部分空间，唤醒写等待的进程，允许写入*/

        ret = count;
    }
out:
    mutex_unlock(&dev->mutex);
out2:
    remove_wait_queue(&dev->r_wait, &wait);
    set_current_state(TASK_RUNNING);
    return ret;
}

/*
 *写入设备函数
 */
static ssize_t globalfifo_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;
    struct globalfifo_dev *dev = filp->private_data;

    DECLARE_WAITQUEUE(wait, current);

    mutex_lock(&dev->mutex);
    add_wait_queue(&dev->w_wait, &wait);

    while (GLOBALFIFO_SIZE == dev->current_len) {
        if (filp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto out;
        }
        __set_current_state(TASK_INTERRUPTIBLE);

        mutex_unlock(&dev->mutex);
        schedule();
        if (signal_pending(current)) {
            ret = -ERESTARTSYS;
            goto out2;
        }
        mutex_lock(&dev->mutex);
    }

    if (count > GLOBALFIFO_SIZE - dev->current_len) {
        count = GLOBALFIFO_SIZE - dev->current_len;
    }
    
    /*将数据从用户空间拷贝的内核空间*/
    if (copy_from_user(dev->mem + dev->current_len, buf, count)) {
        ret = -EFAULT;
        goto out;
    } else {
        dev->current_len += count;
        printk(KERN_INFO "written %ld bytes, current_len: %d\n", count, dev->current_len);

        wake_up_interruptible(&dev->r_wait);

        ret = count;
    }
out:
    mutex_unlock(&dev->mutex);
out2:
    remove_wait_queue(&dev->w_wait, &wait);
    set_current_state(TASK_RUNNING);
    return ret;
}

/*
 *文件定位函数
 */
static loff_t globalfifo_llseek(struct file *filep, loff_t offset, int orig)
{
    loff_t ret = 0;
    switch (orig) {
    case 0:
        if (offset < 0) {
            ret = -EINVAL;
            break;
        }
        if ((unsigned int)offset > GLOBALFIFO_SIZE) {
            ret = -EINVAL;
            break;
        }
        filep->f_pos = (unsigned int)offset;
        ret = filep->f_pos;
        break;
    case 1:
        if ((filep->f_pos + offset) > GLOBALFIFO_SIZE) {
            ret = -EINVAL;
            break;
        }
        if ((filep->f_pos + offset) < 0) {
            ret = -EINVAL;
            break;
        }
        filep->f_pos += offset;
        ret = filep->f_pos;
        break;
    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

/*
 *文件操作的结构体
 */
static const struct file_operations globalfifo_fops = {
    .owner          = THIS_MODULE,
    .llseek         = globalfifo_llseek,
    .read           = globalfifo_read,
    .write          = globalfifo_write,
    .unlocked_ioctl = golbalfifo_ioctl,
    .open           = globalfifo_open,
    .release        = globalfifo_release,
};

/*
 *初始化字符设备结构体，并注册设备
 */
static void globalfifo_setup_cdev(struct globalfifo_dev *dev, int index)
{
    int err, devno = MKDEV(globalfifo_major, index);

    cdev_init(&dev->cdev, &globalfifo_fops);
    dev->cdev.owner = THIS_MODULE;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_INFO "Error %d adding globalfifo_%d", err, index);
    }
    mutex_init(&dev->mutex);
    init_waitqueue_head(&dev->r_wait);
    init_waitqueue_head(&dev->w_wait);
}

/*
 *设备驱动模块加载函数
 */
static struct class *globalfifo_class;
static int __init globalfifo_init(void)
{
    int ret, i;
    struct device *globalfifo_device = NULL;
    dev_t devno = MKDEV(globalfifo_major, 0);

    if (globalfifo_major) {  /*如果设备号为非0,则注册设备号*/
        ret = register_chrdev_region(devno, DEVICE_NUM, "globalfifo");
    } else {    /*设备号为0,动态申请设备号*/
        ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "globalfifo");
        globalfifo_major = MAJOR(devno);
    }
    if (ret < 0) {
        return ret;
    }
    /*注册设备类，使可以自动生成设备文件*/
    globalfifo_class = class_create(THIS_MODULE, "globalfifo_class");
    if (IS_ERR(globalfifo_class)) {
        ret = PTR_ERR(globalfifo_class);
        goto class_err;
    }
    for (i=0; i < DEVICE_NUM; i++) {
        /*自动生成设备文件*/
        globalfifo_device = device_create(globalfifo_class, NULL, MKDEV(globalfifo_major, i), NULL, "globalfifo_%d", i);
        if (IS_ERR(globalfifo_device)) {
            ret = PTR_ERR(globalfifo_device);
            goto device_err;
        }
    }
    
    /*申请内存块*/
    globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev) * DEVICE_NUM, GFP_KERNEL);

    if (!globalfifo_devp) {
        ret = -ENOMEM;
        goto malloc_err;
    }

    for (i=0; i < DEVICE_NUM; i++) {
        globalfifo_setup_cdev(globalfifo_devp + i, i);
    }

/*note 1:*/
/*以下代码试图从代码中设置设备节点文件权限为普通用户可读写，但实际测试没有效果*/
/*
    for (i=0; i < DEVICE_NUM; i++) {
        char path[20];
        struct file *filp = NULL;
        struct inode *inode = NULL;
        snprintf(path, 20, "/dev/globalfifo_%d", i);
        filp = filp_open(path, O_RDONLY | O_CREAT, 0644);
        if (IS_ERR(filp)) {
            printk(KERN_EMERG"Device file open error, process exit!\n");
            goto devfile_error;
        } else {
            printk(KERN_INFO"Device file %d open success.\n", i);
        }
        inode = filp->f_path.dentry->d_inode;
        inode->i_mode |= 0644;
        filp_close(filp, NULL);
    }
*/
/*end note 1*/

    printk("globalfifo.ko was loaded.\n");
    return 0;

/*note 2:*/
/*以下代码为设备文件节点权限设置出错的处理代码*/
/*
devfile_error:
    for (i=0; i < DEVICE_NUM; i++) {
        cdev_del(&(globalfifo_devp +i)->cdev);
    }

    kfree(globalfifo_devp);
*/
/*end note 2*/
malloc_err:
    i = DEVICE_NUM;

device_err:
    while (i--) {
        device_destroy(globalfifo_class,MKDEV(globalfifo_major, i-1));
    }
    class_destroy(globalfifo_class);

class_err:
    unregister_chrdev_region(devno, DEVICE_NUM);
    return ret;
}

module_init(globalfifo_init);

/*
 *模块卸载函数
 */
static void __exit globalfifo_exit(void)
{
    int i;
    for (i=0; i < DEVICE_NUM; i++) {    /*从系统注销设备*/
        cdev_del(&(globalfifo_devp +i)->cdev);
    }

    kfree(globalfifo_devp);  /*释放内存块*/

    unregister_chrdev_region(MKDEV(globalfifo_major, 0), DEVICE_NUM);    /*使用设备号*/
    for (i=0; i < DEVICE_NUM; i++) {    /*删除设备文件*/
        device_destroy(globalfifo_class, MKDEV(globalfifo_major, i));
    }
    class_destroy(globalfifo_class); /*注销设备类*/
    printk("Bye, See you next time.\n");
}
module_exit(globalfifo_exit);

MODULE_AUTHOR("Nick Yan");
MODULE_LICENSE("GPL v2");
