#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/dmaengine.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/mutex.h>

#include "echodev-cmd.h"

#define DEVNR 64
#define DEVNRNAME "echodev"

#define VID 0x1234
#define DID 0xbeef

#define DMA_SRC 0x10
#define DMA_DST 0x18
#define DMA_CNT 0x20
#define DMA_CMD 0x28
#define DMA_RUN 1

struct echodev {
	struct pci_dev *pdev;
	void __iomem *ptr_bar0;
	struct list_head list;
	struct cdev cdev;
	dev_t dev_nr;
};

/* Global Variables */
LIST_HEAD(card_list);
static struct mutex lock;
static int card_count = 0;

static irqreturn_t echo_irq_handler(int irq_nr, void *data)
{
	struct echodev *echo = (struct echodev *) data;
	if(ioread32(echo->ptr_bar0 + 8) & 0x1) {
		printk("echodev-drv - Legacy IRQ triggered!\n");
		iowrite32(2, echo->ptr_bar0 + 8);
	}
	return IRQ_HANDLED;
}

static int dma_transfer(struct echodev *echo, void *buffer, int count, dma_addr_t addr, enum dma_data_direction dir)
{
	dma_addr_t buffer_dma_addr = dma_map_single(&echo->pdev->dev, buffer, count, dir);

	/* Setup the DMA controller */
	iowrite32(count, echo->ptr_bar0 + DMA_CNT);

	switch(dir) {
		case DMA_TO_DEVICE: /* 1 */
			iowrite32(buffer_dma_addr, echo->ptr_bar0 + DMA_SRC);
			iowrite32(addr, echo->ptr_bar0 + DMA_DST);
			break;
		case DMA_FROM_DEVICE: /* 2 */
			iowrite32(buffer_dma_addr, echo->ptr_bar0 + DMA_DST);
			iowrite32(addr, echo->ptr_bar0 + DMA_SRC);
			break;
		default:
			return -EFAULT;
	}

	/* Let's fire the dma */
	iowrite32(DMA_RUN | dir, echo->ptr_bar0 + DMA_CMD);

	dma_unmap_single(&echo->pdev->dev, buffer_dma_addr, count, dir);
	return 0;
}

static int echo_open(struct inode *inode, struct file *file)
{
	struct echodev *echo;
	dev_t dev_nr = inode->i_rdev;

	list_for_each_entry(echo, &card_list, list) {
		if(echo->dev_nr == dev_nr) {
			file->private_data = echo;
			return 0;
		}
	}
	return -ENODEV;
}

static ssize_t echo_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offs)
{
        char *buf;
        int not_copied, to_copy = (count + *offs < 4096) ? count : 4096 - *offs;
        struct echodev *echo = (struct echodev *) file->private_data;

        if(*offs >= pci_resource_len(echo->pdev, 1))
                return 0;

        buf = kmalloc(to_copy, GFP_ATOMIC);
        not_copied = copy_from_user(buf, user_buffer, to_copy);

        dma_transfer(echo, buf, to_copy, *offs, DMA_TO_DEVICE);

        kfree(buf);
        *offs += to_copy - not_copied;
        return to_copy - not_copied;
}

static ssize_t echo_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offs)
{
        char *buf;
        struct echodev *echo = (struct echodev *) file->private_data;
        int not_copied, to_copy = (count + *offs < pci_resource_len(echo->pdev, 1)) ? count : pci_resource_len(echo->pdev, 1) - *offs;

        if(to_copy == 0)
                return 0;

        buf = kmalloc(to_copy, GFP_ATOMIC);

        dma_transfer(echo, buf, to_copy, *offs, DMA_FROM_DEVICE);

        mdelay(5);
        not_copied = copy_to_user(user_buffer, buf, to_copy);

        kfree(buf);
        *offs += to_copy - not_copied;
        return to_copy - not_copied;
}

static int echo_mmap(struct file *file, struct vm_area_struct *vma)
{
	int status;
	struct echodev *echo = (struct echodev *) file->private_data;

	vma->vm_pgoff = pci_resource_start(echo->pdev, 1) >> PAGE_SHIFT;

	status = io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end
	- vma->vm_start, vma->vm_page_prot);
	if(status) {
		printk("echodev-drv - Error allocating device number\n");
		return -status;
	}
	return 0;
}

static long int echo_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	struct echodev *echo = (struct echodev *) file->private_data;
	u32 val;

	switch(cmd) {
		case GET_ID:
			val = ioread32(echo->ptr_bar0 + 0x00);
			return copy_to_user((u32 *) arg, &val, sizeof(val));
		case GET_INV:
			val = ioread32(echo->ptr_bar0 + 0x04);
			return copy_to_user((u32 *) arg, &val, sizeof(val));
		case GET_RAND:
			val = ioread32(echo->ptr_bar0 + 0x0C);
			return copy_to_user((u32 *) arg, &val, sizeof(val));
		case SET_INV:
			if(0 != copy_from_user(&val, (u32 *) arg, sizeof(val)))
				return -EFAULT;
			iowrite32(val, echo->ptr_bar0 + 0x4);
			return 0;
		case IRQ:
			iowrite32(1, echo->ptr_bar0 + 0x8);
			return 0;
		default:
			return -EINVAL;
	}
}

static struct file_operations fops = {
	.open = echo_open,
	.mmap = echo_mmap,
	.read = echo_read,
	.write = echo_write,
	.unlocked_ioctl = echo_ioctl,
};

static struct pci_device_id echo_ids[] = {
	{PCI_DEVICE(VID, DID)},
	{},
};
MODULE_DEVICE_TABLE(pci, echo_ids);

static int echo_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int status, irq_nr;
	struct echodev *echo;

	echo = devm_kzalloc(&pdev->dev, sizeof(struct echodev), GFP_KERNEL);
	if(!echo)
		return -ENOMEM;

	mutex_lock(&lock);
	cdev_init(&echo->cdev, &fops);
	echo->cdev.owner = THIS_MODULE;

	echo->dev_nr = MKDEV(DEVNR, card_count++);
	status = cdev_add(&echo->cdev, echo->dev_nr, 1);
	if(status < 0) {
		printk("echodev-drv - Error adding cdev\n");
		return status;
	}

	list_add_tail(&echo->list, &card_list);
	mutex_unlock(&lock);

	echo->pdev = pdev;

	status = pcim_enable_device(pdev);
	if(status != 0) {
		printk("echodev-drv - Error enabling device\n");
		goto fdev;
	}

	pci_set_master(pdev);

	echo->ptr_bar0 = pcim_iomap(pdev, 0, pci_resource_len(pdev, 0));
	if(!echo->ptr_bar0) {
		printk("echodev-drv - Error mapping BAR0\n");
		status = -ENODEV;
		goto fdev;
	}

	pci_set_drvdata(pdev, echo);

	status = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_ALL_TYPES);
	if(status != 1) {
		printk("echodev-drv - Error alloc_irq returned %d\n", status);
		status = -ENODEV;
		goto fdev;
	}

	irq_nr = pci_irq_vector(pdev, 0);
	printk("echodev-drv - IRQ Number: %d\n", irq_nr);

	status = devm_request_irq(&pdev->dev, irq_nr, echo_irq_handler, 0,
	"echodev-irq", echo);
	if(status != 0) {
		printk("echodev-drv - Error requesting interrupt\n");
		goto fdev;
	}

	return 0;

fdev:
	/* Removing echo from list is missing */
	cdev_del(&echo->cdev);
	return status;

}

static void echo_remove(struct pci_dev *pdev)
{
	struct echodev *echo = (struct echodev *) pci_get_drvdata(pdev);
	printk("echodev-drv - Removing the device with Device Number %d:%d\n",
	MAJOR(echo->dev_nr), MINOR(echo->dev_nr));
	if(echo) {
		mutex_lock(&lock);
		list_del(&echo->list);
		mutex_unlock(&lock);
		cdev_del(&echo->cdev);
	}
	pci_free_irq_vectors(pdev);
}

static struct pci_driver echo_driver = {
	.name = "echodev-driver",
	.probe = echo_probe,
	.remove = echo_remove,
	.id_table = echo_ids,
};

static int __init echo_init(void)
{
	int status;
	dev_t dev_nr = MKDEV(DEVNR, 0);

	status = register_chrdev_region(dev_nr, MINORMASK + 1, DEVNRNAME);
	if(status < 0) {
		printk("echodev-drv - Error registering Device numbers\n");
		return status;
	}

	mutex_init(&lock);

	status = pci_register_driver(&echo_driver);
	if(status < 0) {
		printk("echodev-drv - Error registering driver\n");
		unregister_chrdev_region(dev_nr, MINORMASK + 1);
		return status;
	}
	return 0;
}

static void __exit echo_exit(void)
{
	dev_t dev_nr = MKDEV(DEVNR, 0);
	unregister_chrdev_region(dev_nr, MINORMASK + 1);
	pci_unregister_driver(&echo_driver);
}

module_init(echo_init);
module_exit(echo_exit);

MODULE_LICENSE("GPL");
