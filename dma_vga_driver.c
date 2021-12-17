#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>


#define DEVICE_NAME	"dma_vga"
#define CLASS_NAME	"dma_zynq"
#define BASE_MINOR	0
#define MINORS_REQUIRED 1

/* ====== ADDRESSES ======*/
// FFT
#define AXI_DMA_0 0x40400000
// VGA
#define AXI_DMA_1 0x40410000

/* ====== DMA BUFFERS ====== */
#define TAILLE_BUF_X 640
#define TAILLE_BUF_Y 480

uint8_t *buf1;
uint8_t *buf2;
dma_addr_t dma_handle1;
dma_addr_t dma_handle2;

/* ====== IOCTL ====== */
#define WAIT 0
#define START 1

/* ====== Device ====== */
static struct class* charClass  = NULL;
static struct device* charDevice = NULL;

dev_t dev;
static struct cdev cdevice;

/* ====== File operation function definition ====== */
static int dma_vga_mmap(struct file *filp, struct vm_area_struct *vma);
static int dma_vga_close(struct inode* inode, struct file *filp);
static long dma_vga_ioctl(struct file *file, unsigned int cmd, unsigned long arg);


static struct file_operations fops = {
	.owner = THIS_MODULE,
	.mmap = dma_vga_mmap,
	.unlocked_ioctl = dma_vga_ioctl,
	.release = dma_vga_close
};

/* ====== Module function definition ====== */
static int dma_vga_init(void);
static void dma_vga_exit(void);

/* ====== File operation function ====== */
static int dma_vga_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return 0;
}

static long dma_vga_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static int dma_vga_close(struct inode* inode, struct file *filp)
{
	return 0;
}

/* ====== Module functions ======*/
static int dma_vga_init(void)
{
	if (alloc_chrdev_region(&dev, BASE_MINOR , MINORS_REQUIRED , DEVICE_NAME)) {
		pr_err("Failed to allocate char dev region\n");
		goto fail_alloc_chrdev;
	}

	cdev_init( &cdevice , &fops );

	if (cdev_add(&cdevice, dev, MINORS_REQUIRED)) {
		pr_err("Failed to add device\n");
		goto fail_cdev_add;
	}

	charClass = class_create(THIS_MODULE, CLASS_NAME);
	if (charClass == NULL) {
		pr_err("Failed to create class\n");
		goto fail_create_class;
	}

	charDevice = device_create(charClass, NULL, dev, NULL, DEVICE_NAME);
	if (charDevice == NULL) {
		pr_err("Failed to create device\n");
		goto fail_create_device;
	}

	/* Allocate buffers */
	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32))) {
		dev_warn(dev, "mydev: No suitable DMA available\n");
		goto ignore_this_device;
	}

	buf1 = dma_alloc_coherent(dev, TAILLE_BUF_X * TAILLE_BUF_Y, 
					    &dma_handle1, GFP_DMA);
	if (buf1 == NULL) {
		pr_err("Failed to allocate buffer 1\n");
		goto fail_buf1;
	}

	buf2 = dma_alloc_coherent(dev, TAILLE_BUF_X * TAILLE_BUF_Y, 
					    &dma_handle2, GFP_DMA);
	if (buf2 == NULL) {
		pr_err("Failed to allocate buffer 2\n");
		goto fail_buf2;
	}
	/*
	buf1 = (uint8_t *)kmalloc_array(TAILLE_BUF_X * TAILLE_BUF_Y, 
				       sizeof(uint8_t), GFP_KERNEL);
	if (buf1 == NULL) {
		pr_err("Failed to allocate buf1 memory\n");
		goto fail_buf1;
	}

	buf2 = (uint8_t *)kmalloc_array(TAILLE_BUF_X * TAILLE_BUF_Y, 
				       sizeof(uint8_t), GFP_KERNEL);
	if (buf2 == NULL) {
		pr_err("Failed to allocate buf2 memory\n");
		goto fail_buf2;
	}
	*/

	/* Init vma */
	pr_info("Loaded\n");

	return 0;

ignore_this_device:
fail_buf2:
	dma_free_coherent(dev, TAILLE_BUF_X * TAILLE_BUF_Y, buf1, dma_handle1);
fail_buf1:
fail_create_device:
	device_destroy(charClass, dev);
fail_create_class:
	class_destroy(charClass);
fail_cdev_add:
	cdev_del(&cdevice);
fail_alloc_chrdev:
	unregister_chrdev_region(dev, 1);
	return -1;
}

static void dma_vga_exit(void)
{
	dma_free_coherent(dev, TAILLE_BUF_X * TAILLE_BUF_Y, buf1, dma_handle1);
	dma_free_coherent(dev, TAILLE_BUF_X * TAILLE_BUF_Y, buf2, dma_handle1);
	device_destroy(charClass, dev);
	class_destroy(charClass);
	cdev_del(&cdevice);
	unregister_chrdev_region(dev, 1);
}

module_init(dma_vga_init);
module_exit(dma_vga_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Clément Roger, Clément Leboulenger");
MODULE_DESCRIPTION("DMA driver for image converter to VGA");