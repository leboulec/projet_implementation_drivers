#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/slab.h>

#define DEVICE_NAME	"dma_vga"
#define CLASS_NAME	"dma_zynq"
#define BASE_MINOR	0
#define MINORS_REQUIRED 1

#define IOC_BIT 12 // bit de l'interruption IOC des registres de contrôle

#define MM2S_CR 0x00 // offset du registre MM1S_DMACR
#define MM2S_SR 0x04 // offset du registre MM2S_DMASR
#define MM2S_SA 0x18 // offset du registre MM2S_SA
#define MM2S_LENGTH 0x28 // offset du registre MM2S_LENGTH

#define S2MM_CR 0x30 // offset du registre S2MM_DMACR
#define S2MM_SR 0x34 // offset du registre S2MM_DMASR
#define S2MM_DA 0x48 // offset du registre S2MM_DA
#define S2MM_LENGTH 0x58// offset du registre S2MM_LENGTH

/* ====== DMA BUFFERS ====== */
#define TAILLE_BUF_X 640
#define TAILLE_BUF_Y 480
#define REGISTER_ADDRESS
#define BUFFER_ADDRESS
#define BUFFER_SIZE

uint8_t *buf1;
uint8_t *buf2;

typedef unsigned int u32;
typedef u32 size_t;

/* ====== Device ====== */
static struct class* charClass  = NULL;
static struct device* charDevice = NULL;

dev_t dev;
static struct cdev cdevice;

/* ====== File operation function definition ====== */
static int dma_vga_mmap(struct file *filp, struct vm_area_struct *vma);
static int dma_vga_close(struct inode* inode, struct file *filp);
static long dma_vga_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

extern u32 ioread32(volatile void *addr);
extern void iowrite32(u32 value, volatile void *addr);

/* ====== Module function definition ====== */
static int dma_vga_init(void);
static void dma_vga_exit(void);

/* ====== File operation structure ====== */
static struct file_operations fops = {
	.owner = THIS_MODULE,
	.mmap = dma_vga_mmap,
	.unlocked_ioctl = dma_vga_ioctl,
	.release = dma_vga_close
};

/* ====== DMA STRUCTURES ====== */

struct axi_dma
{
  volatile char *register_space; // adresse de base des registres
  volatile int   rx_done; // booléen pour savor si une lecture est terminée
  volatile int   tx_done; // booléen pour savoir si une écriture est terminée
};

struct axi_dma_buffer
{
  volatile void *data; // pointeur vers l'adresse de base du buffer
  size_t size; // taille du buffer
};

struct axi_dma dma;
struct axi_dma_buffer dma_buffer;

/* ====== Write function definition ====== */
void write_dma_simple( struct axi_dma *dma , struct axi_dma_buffer *buffer) {

  dma->tx_done = 0; // on remet le tx_done à 0 pour pouvoir détecter une nouvelle interruption
  iowrite32(1 | (1 << IOC_BIT), dma->register_space + MM2S_CR); // on active le DMA et l'interruption IOC
  iowrite32((u32)buffer->data, dma->register_space + MM2S_SA); // on écrit l'adresse du buffer dans SA
  iowrite32(buffer->size, dma->register_space + MM2S_LENGTH); // on écrit la taille du buffer dans LENGTH
}

/* ====== Read function definition ====== */
void read_dma_simple( struct axi_dma *dma , struct axi_dma_buffer *buffer) {
  
  dma->rx_done = 0; // on remet le tx_done à 0 pour pouvoir détecter une nouvelle interruption
  iowrite32(1 | (1 << IOC_BIT), dma->register_space + S2MM_CR); // on active le DMA et l'interruption IOC
  iowrite32((u32)buffer->data, dma->register_space + S2MM_DA); // on écrit l'adresse du buffer dans DA
  iowrite32(buffer->size, dma->register_space + S2MM_LENGTH); // on écrit la taille du buffer dans LENGTH
}

/* ====== Wait transmission function definition ====== */
void wait_tx_completion(struct axi_dma *dma){
  while(!dma->tx_done);
}

/* ====== Wait reception function definition ====== */
void wait_rx_completion(struct axi_dma *dma)
{
  while(!dma->rx_done);
}

/* ====== Reception IRQ function definition ====== */
void dma_irq_rx(struct axi_dma *dma)
{
  u32 status;
  status = ioread32(dma->register_space + S2MM_SR); // on lit le registre de status
  if(status & (1 << IOC_BIT)) // si le bit IOC est à 1 -> une transaction vient de se terminer
  {
    iowrite32(status | (1 << IOC_BIT), dma->register_space + S2MM_SR); // on écrit 1 sur le bit IOC du registre SR
    dma->rx_done = 1; // on met le rx_done à 1 pour débloquer wait_rx_completion
  }
}

/* ====== Transmission IRQ function definition ====== */
void dma_irq_tx(struct axi_dma *dma)
{
  u32 status;
  status = ioread32(dma->register_space + MM2S_SR); // on lit le registre de status
  if(status & (1 << IOC_BIT)) // si le bit IOC est à 1 -> une transaction vient de se terminer
  {
    iowrite32(status | (1 << IOC_BIT), dma->register_space + MM2S_SR); // on écrit 1 sur le bit IOC du registre SR
    dma->tx_done = 1; // on met le tx_done à 1 pour débloquer wait_tx_completion
  }
}

/* ====== File operation function ====== */
static int dma_vga_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return 0;
}

static long dma_vga_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch( cmd ){

		case WAIT_TX:
			wait_tx_completion( dma );
			break;
		case START:
			start_dma();	
			break;
		default:
			pr_err("Unknown command in ioctl syscall\n");	
	}

	return 0;
}

static int dma_vga_close(struct inode* inode, struct file *filp)
{
	return 0;
}

static void start_dma(void){

}

static void fill_dma_struct(void){

	dma->register_space = REGISTER_ADDRESS;
	dma->tx_done = 0;
	dma->rx_done = 0;

	dma_buffer->data = *BUFFER_ADDRESS;
	dma_buffer->size = BUFFER_SIZE;
}

/* ====== Module functions ======*/
static int dma_vga_init(void)
{
	fill_dma_struct();

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

	/* Init vma */
	pr_info("Loaded\n");

	return 0;

fail_buf2:
	kfree(buf1);
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