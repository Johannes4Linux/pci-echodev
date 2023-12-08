#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/dmaengine.h>

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
} mydev;

static int dma_transfer(struct echodev *echo, void *buffer, int count, dma_addr_t addr, enum dma_data_direction dir)
{
	return 0;
}

static ssize_t echo_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offs)
{
        char *buf;
        int not_copied, to_copy = (count + *offs < 4096) ? count : 4096 - *offs;
        struct echodev *echo = &mydev;

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
        struct echodev *echo = &mydev;
        int not_copied, to_copy = (count + *offs < pci_resource_len(echo->pdev, 1)) ? count : pci_resource_len(echo->pdev, 1) - *offs;

        if(to_copy == 0)
                return 0;

        buf = kmalloc(to_copy, GFP_ATOMIC);

        dma_transfer(&mydev, buf, to_copy, *offs, DMA_FROM_DEVICE);

        mdelay(5);
        not_copied = copy_to_user(user_buffer, buf, to_copy);

        kfree(buf);
        *offs += to_copy - not_copied;
        return to_copy - not_copied;
}

static int echo_mmap(struct file *file, struct vm_area_struct *vma)
{
	int status;

	vma->vm_pgoff = pci_resource_start(mydev.pdev, 1) >> PAGE_SHIFT;

	status = io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end
	- vma->vm_start, vma->vm_page_prot);
	if(status) {
		printk("echodev-drv - Error allocating device number\n");
		return -status;
	}
	return 0;
}

static struct file_operations fops = {
	.mmap = echo_mmap,
	.read = echo_read,
	.write = echo_write,
};

static struct pci_device_id echo_ids[] = {
	{PCI_DEVICE(VID, DID)},
	{},
};
MODULE_DEVICE_TABLE(pci, echo_ids);

static int echo_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int status;
	void __iomem *ptr_bar0, __iomem *ptr_bar1;

	mydev.pdev = pdev;

	status = register_chrdev(DEVNR, DEVNRNAME, &fops);
	if(status < 0) {
		printk("echodev-drv - Error allocating device number\n");
		goto fdev;
	}

	status = pcim_enable_device(pdev);
	if(status != 0) {
		printk("echodev-drv - Error enabling devie\n");
		goto fdev;
	}

	pci_set_master(pdev);

	ptr_bar0 = pcim_iomap(pdev, 0, pci_resource_len(pdev, 0));
	mydev.ptr_bar0 = ptr_bar0;
	if(!ptr_bar0) {
		printk("echodev-drv - Error mapping BAR0\n");
		status = -ENODEV;
		goto fdev;
	}
	ptr_bar1 = pcim_iomap(pdev, 1, pci_resource_len(pdev, 1));
	if(!ptr_bar1) {
		printk("echodev-drv - Error mapping BAR1\n");
		status = -ENODEV;
		goto fdev;
	}

	printk("echodev-drv - ID: 0x%x\n", ioread32(ptr_bar0));
	printk("echodev-drv - Random Value: 0x%x\n", ioread32(ptr_bar0 + 0xc));

	iowrite32(0x11223344, ptr_bar0 + 4);
	mdelay(1);
	printk("echodev-drv - Inverse Pattern: 0x%x\n", ioread32(ptr_bar0 + 0x4));

	iowrite32(0x44332211, ptr_bar1);
	printk("echodev-drv - BAR1 Offset 0: 0x%x\n", ioread8(ptr_bar1));
	printk("echodev-drv - BAR1 Offset 0: 0x%x\n", ioread16(ptr_bar1));
	printk("echodev-drv - BAR1 Offset 0: 0x%x\n", ioread32(ptr_bar1));

	return 0;

fdev:
	unregister_chrdev(DEVNR, DEVNRNAME);
	return status;

}

static void echo_remove(struct pci_dev *pdev)
{
	printk("echodev-drv - Removing the device\n");
	unregister_chrdev(DEVNR, DEVNRNAME);
}

static struct pci_driver echo_driver = {
	.name = "echodev-driver",
	.probe = echo_probe,
	.remove = echo_remove,
	.id_table = echo_ids,
};

module_pci_driver(echo_driver);

MODULE_LICENSE("GPL");
