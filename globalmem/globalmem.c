/*
 * a simple char device driver: globalmem without mutex
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

#define GLOBALMEM_SIZE			0x1000  /*全局内存大小，用于模拟读写操作的内存区域*/
#define MEM_CLEAR				0x1     /*ioctl操作命令                      */
#define GLOBALMEM_MAJOR			230     /*主设备号                           */
#define DEVICE_NUM              10      /*设备数目                           */

static int globalmem_major = GLOBALMEM_MAJOR;
module_param(globalmem_major, int, S_IRUGO);    /*声明insmod时的参数*/

struct globalmem_dev {
	struct cdev cdev;                   /*字符设备结构体*/
	unsigned char mem[GLOBALMEM_SIZE];  /*用于模拟读写操作的内存空间*/
    struct mutex mutex;                 /*用于多用户(进程)访问时的控制，不能用自旋锁，因为读写操作中有调用可能导致阻塞的copy_to_user及copy_from_user; 只能使用互斥体*/
};

static struct globalmem_dev *globalmem_devp;

/*
 *文件打开函数，对应于用户空间的open函数，用户空间调用open函数时，系统内部经过各种处理后，最终调用本函数
 */
static int globalmem_open(struct inode *inode, struct file *filep)
{
    /*获取包含cdev结构体的globalmem_dev结构体指针*/
    struct globalmem_dev *dev = container_of(inode->i_cdev, struct globalmem_dev, cdev);
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
static int globalmem_release(struct inode *inode, struct file *filep)
{
	return 0;
}

/*
 *ioctl设备控制函数
 */
static long golbalmem_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct globalmem_dev *dev = filep->private_data;

	switch(cmd) {
    case MEM_CLEAR:
        mutex_lock(&dev->mutex);

		memset(dev->mem, 0, GLOBALMEM_SIZE);
		printk(KERN_INFO "globalmem is set to zero\n");

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
static ssize_t globalmem_read(struct file *filep, char __user * buf, size_t size, loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned int count = size;
    int ret = 0;
    struct globalmem_dev *dev = filep->private_data;    /*获取设备结构体指针*/

    if (p >= GLOBALMEM_SIZE) {  /*若操作范围大于本设备最大空间，则直接返回*/
        return 0;
    }

    if (count > GLOBALMEM_SIZE - p) {   /*若读取数据量大于设备内剩余数据量，则设置读取数据量为设备内剩余数据量*/
        count = GLOBALMEM_SIZE - p;
    }

    mutex_lock(&dev->mutex);

    /*buf为用户空间指针，不能直接使用memcpy()等方法，内核空间不能直接访问用户空间*/
    /*copy_to_user：完成数据从内核空间向用户空间的复制*/
    if (copy_to_user(buf, dev->mem + p, count)) {
        ret = -EFAULT;
    } else {
        *ppos += count;
        ret = count;

        printk(KERN_INFO "read %u bytes from %lu\n", count, p);
    }

    mutex_unlock(&dev->mutex);

    return ret;
}

/*
 *写入设备函数
 */
static ssize_t globalmem_write(struct file *filep, const char __user *buf, size_t size, loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned int count = size;
    int ret = 0;
    struct globalmem_dev *dev = filep->private_data;

    if (p > GLOBALMEM_SIZE) {   /*若操作范围大于本设备最大空间，则直接返回*/
        return 0;
    }

    if (count > GLOBALMEM_SIZE - p) {   /*若读取数据量大于设备内剩余数据量，则设置读取数据量为设备内剩余数据量*/
        count = GLOBALMEM_SIZE - p;
    }

    mutex_lock(&dev->mutex);

    /*将数据从用户空间拷贝的内核空间*/
    if (copy_from_user(dev->mem + p, buf, count)) {
        ret = -EFAULT;
    } else {
        *ppos += count;
        ret = count;

        printk(KERN_INFO "written %u bytes from %lu\n", count, p);
    }

    mutex_unlock(&dev->mutex);

    return ret;
}

/*
 *文件定位函数
 */
static loff_t globalmem_llseek(struct file *filep, loff_t offset, int orig)
{
    loff_t ret = 0;
    switch (orig) {
    case 0:
        if (offset < 0) {
            ret = -EINVAL;
            break;
        }
        if ((unsigned int)offset > GLOBALMEM_SIZE) {
            ret = -EINVAL;
            break;
        }
        filep->f_pos = (unsigned int)offset;
        ret = filep->f_pos;
        break;
    case 1:
        if ((filep->f_pos + offset) > GLOBALMEM_SIZE) {
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
static const struct file_operations globalmem_fops = {
    .owner          = THIS_MODULE,
    .llseek         = globalmem_llseek,
    .read           = globalmem_read,
    .write          = globalmem_write,
    .unlocked_ioctl = golbalmem_ioctl,
    .open           = globalmem_open,
    .release        = globalmem_release,
};

/*
 *初始化字符设备结构体，并注册设备
 */
static void globalmem_setup_cdev(struct globalmem_dev *dev, int index)
{
    int err, devno = MKDEV(globalmem_major, index);

    mutex_init(&dev->mutex);
    cdev_init(&dev->cdev, &globalmem_fops);
    dev->cdev.owner = THIS_MODULE;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_INFO "Error %d adding globalmem%d", err, index);
    }
}

/*
 *设备驱动模块加载函数
 */
static struct class *globalmem_class;
static int __init globalmem_init(void)
{
    int ret, i;
    struct device *globalmem_device = NULL;
    dev_t devno = MKDEV(globalmem_major, 0);

    if (globalmem_major) {  /*如果设备号为非0,则注册设备号*/
        ret = register_chrdev_region(devno, DEVICE_NUM, "globalmem");
    } else {    /*设备号为0,动态申请设备号*/
        ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "globalmem");
        globalmem_major = MAJOR(devno);
    }
    if (ret < 0) {
        return ret;
    }
    /*注册设备类，使可以自动生成设备文件*/
    globalmem_class = class_create(THIS_MODULE, "globalmem_class");
    if (IS_ERR(globalmem_class)) {
        ret = PTR_ERR(globalmem_class);
        goto class_err;
    }
    for (i=0; i < DEVICE_NUM; i++) {
        /*自动生成设备文件*/
        globalmem_device = device_create(globalmem_class, NULL, MKDEV(globalmem_major, i), NULL, "globalmem_%d", i);
        if (IS_ERR(globalmem_device)) {
            ret = PTR_ERR(globalmem_device);
            goto device_err;
        }
    }
    
    /*申请内存块*/
    globalmem_devp = kzalloc(sizeof(struct globalmem_dev) * DEVICE_NUM, GFP_KERNEL);

    if (!globalmem_devp) {
        ret = -ENOMEM;
        goto malloc_err;
    }

    for (i=0; i < DEVICE_NUM; i++) {
        globalmem_setup_cdev(globalmem_devp + i, i);
    }

/*note 1:*/
/*以下代码试图从代码中设置设备节点文件权限为普通用户可读写，但实际测试没有效果*/
/*
    for (i=0; i < DEVICE_NUM; i++) {
        char path[20];
        struct file *filp = NULL;
        struct inode *inode = NULL;
        snprintf(path, 20, "/dev/globalmem_%d", i);
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

    printk("globalmem.ko was loaded.\n");
    return 0;

/*note 2:*/
/*以下代码为设备文件节点权限设置出错的处理代码*/
/*
devfile_error:
    for (i=0; i < DEVICE_NUM; i++) {
        cdev_del(&(globalmem_devp +i)->cdev);
    }

    kfree(globalmem_devp);
*/
/*end note 2*/
malloc_err:
    i = DEVICE_NUM;

device_err:
    while (i--) {
        device_destroy(globalmem_class,MKDEV(globalmem_major, i-1));
    }
    class_destroy(globalmem_class);

class_err:
    unregister_chrdev_region(devno, DEVICE_NUM);
    return ret;
}

module_init(globalmem_init);

/*
 *模块卸载函数
 */
static void __exit globalmem_exit(void)
{
    int i;
    for (i=0; i < DEVICE_NUM; i++) {    /*从系统注销设备*/
        cdev_del(&(globalmem_devp +i)->cdev);
    }

    kfree(globalmem_devp);  /*释放内存块*/

    unregister_chrdev_region(MKDEV(globalmem_major, 0), DEVICE_NUM);    /*使用设备号*/
    for (i=0; i < DEVICE_NUM; i++) {    /*删除设备文件*/
        device_destroy(globalmem_class, MKDEV(globalmem_major, i));
    }
    class_destroy(globalmem_class); /*注销设备类*/
    printk("Bye, See you next time.\n");
}
module_exit(globalmem_exit);

MODULE_AUTHOR("Nick Yan");
MODULE_LICENSE("GPL v2");
