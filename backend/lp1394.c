/*
 * "$Id: lp1394.c,v 1.1 2002/05/19 23:10:18 mike Exp $"
 *
 *   IEEE-1394 printer driver module for Linux.
 *
 *   Copyright 2002 by Easy Software Products, all rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the
 *   following conditions are met:
 *
 *     1. Redistributions of source code must retain the above
 *	  copyright notice, this list of conditions and the
 *	  following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the
 *	  above copyright notice, this list of conditions and
 *	  the following disclaimer in the documentation and/or
 *	  other materials provided with the distribution.
 *
 *     3. All advertising materials mentioning features or use
 *	  of this software must display the following
 *	  acknowledgement:
 *
 *	    This product includes software developed by Easy
 *	    Software Products.
 *
 *     4. The name of Easy Software Products may not be used to
 *	  endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS
 *   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 *   BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS
 *   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *   DAMAGE.
 *
 * Contents:
 *
 */

/*
 * This driver module implements the PWG command set for printing over
 * IEEE-1394 and SBP-2.
 */

/*
 * Include necessary headers.
 */

#define __KERNEL__
#include <linux/config.h>
#ifdef CONFIG_SMP
#  define __SMP__
#endif /* CONFIG_SMP */

#define MODULE
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/slab.h>
#include <asm/semaphore.h>
#include <errno.h>


/*
 * Printer device data...
 */

struct lp1394_dev
{
  struct lp1394_dev	*prev,		/* Previous device in list */
			*next;		/* Next device in list */
  devfs_handle_t	ds;		/* Device file */
  char			device_id[256];	/* Device ID string */
  unsigned char		guid[8];	/* Global unique ID */
  int			port,		/* Port number */
			node;		/* Node number */
  struct semaphore	sem;		/* Semaphore to control access */
};


/*
 * Local functions...
 */

static void		lp1394_cleanup(void);
static void		lp1394_free(struct lp1394_dev *);
static int		lp1394_init(void);
static int		lp1394_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
static int		lp1394_open(struct inode *, struct file *);
static unsigned int	lp1394_poll(struct file *, struct poll_table_struct *);
static ssize_t		lp1394_read(struct file *, char *, size_t, loff_t *);
static int		lp1394_release(struct inode *, struct file *);
static void		lp1394_scan(void);
static ssize_t		lp1394_write(struct file *, const char *, size_t, loff_t *);


/*
 * Define driver entry points...
 */

module_init(lp1394_init);
module_exit(lp1394_cleanup);


/*
 * Globals...
 */

int			lp1394_major = 0;	/* Major device number */
struct file_operations	lp1394_fileops =	/* File operations */
			{
			  ioctl: lp1394_ioctl,
			  open: lp1394_open,
			  poll: lp1394_poll,
			  read: lp1394_read,
			  release: lp1394_release,
			  write: lp1394_write
			};
struct lp1394_dev	*lp1394_first = 0,	/* First device in list */
			*lp1394_last = 0;	/* Last device in list */


/*
 * 'lp1394_cleanup()' - Shutdown the driver.
 */

static void
lp1394_cleanup(void)
{
 /*
  * Show the unload message...
  */

  printk(KERN_ALERT "lp1394: Unloading Linux IEEE-1394 Printer Driver v0.1\n");
  printk(KERN_ALERT "lp1394: Copyright 2002 by Easy Software Products, all rights reserved.\n");

 /*
  * Unregister devices...
  */

  while (lp1394_first)
    lp1394_free(lp1394_first);

 /*
  * Unregister the major number...
  */

  unregister_chrdev(lp1394_major, "lp1394");
}


/*
 * 'lp1394_free()' - Free a printer device...
 */

static void
lp1394_free(struct lp1394_dev *lp)		/* I - Printer device */
{
  if (!lp)
    return;

 /*
  * Unregister the device file...
  */

  devfs_unregister(lp->ds);

 /*
  * Update pointers...
  */

  if (lp->prev)
    lp->prev->next = lp->next;
  else
    lp1394_first = lp->next;

  if (lp->next)
    lp->next->prev = lp->prev;
  else
    lp1394_last = lp->prev;

 /*
  * Free memory...
  */

  kfree(lp);
}


/*
 * 'lp1394_init()' - Initialize the driver.
 */

static int
lp1394_init(void)
{
  int	i;					/* Looping var */
  int	result;					/* Result of registration... */
  char	devname[1024];				/* Device name */


 /*
  * Show the load message...
  */

  printk(KERN_ALERT "lp1394: Loading Linux IEEE-1394 Printer Driver v0.1\n");
  printk(KERN_ALERT "lp1394: Copyright 2002 by Easy Software Products, all rights reserved.\n");

 /*
  * Register the device...
  */

  result = register_chrdev(lp1394_major, "lp1394", &lp1394_fileops);

  if (result < 0)
  {
   /*
    * An error occurred; report it and return...
    */

    printk(KERN_WARNING "lp1394: Can't get major number %d!\n", lp1394_major);

    return (result);
  }

 /*
  * Set the major number for this device...
  */

  if (lp1394_major == 0)
  {
    lp1394_major = result;
    printk(KERN_INFO "lp1394: Using major number %d.\n", lp1394_major);
  }

 /*
  * Scan for printer devices...
  */

  lp1394_scan();

 /*
  * Return successfully...
  */

  return (0);
}


/*
 * 'lp1394_ioctl()' - Do an ioctl() on the printer.
 */

static int					/* O - Return status */
lp1394_ioctl(struct inode  *ip,			/* I - INode for device */
             struct file   *filp,		/* I - Open file */
             unsigned int  op,			/* I - Operation code */
	     unsigned long arg)			/* I - Argument */
{
  return (0);
}


/*
 * 'lp1394_open()' - Open a printer device.
 */

static int					/* O - Return status */
lp1394_open(struct inode *ip,			/* I - INode for device */
            struct file  *filp)			/* I - Open file */
{
  int			number;			/* Device number */
  struct lp1394_dev	*temp;			/* Current device */


 /*
  * Figure out which device to access...
  */

  for (number = MINOR(ip->i_rdev), temp = lp1394_first;
       temp && number > 0;
       number --, temp = temp->next);

  if (!temp)
  {
   /*
    * No such device...
    */

    return (-ENODEV);
  }

 /*
  * Set the client data in the file structure...
  */

  filp->private_data = temp;

  printk(KERN_DEBUG "lp1394: Opened lp%d\n", temp->port);

 /*
  * OK, everything went OK, return successful status...
  */

  return (0);
}


/*
 * 'lp1394_poll()' - See if we are ready to read or write.
 */

static unsigned int				/* I - Status of poll */
lp1394_poll(struct file              *filp,	/* I - Open file */
            struct poll_table_struct *pp)	/* I - Polling data */
{
  return (0);
}


/*
 * 'lp1394_read()' - Read data from the printer.
 */

static ssize_t					/* O  - Number of bytes read */
lp1394_read(struct file *filp,			/* I  - Open file */
            char        *buf,			/* O  - Buffer for data */
	    size_t      buflen,			/* I  - Size of buffer */
	    loff_t      *fpos)			/* IO - File position */
{
  struct lp1394_dev	*temp;			/* Device */


  temp = (struct lp1394_dev *)(filp->private_data);

  printk(KERN_DEBUG "lp1394: Writing %d bytes to lp%d\n", buflen, temp->port);

  *fpos = *fpos + buflen;

  return (buflen);
}


/*
 * 'lp1394_release()' - Release resources for an open file.
 */

static int
lp1394_release(struct inode *ip,		/* I - INode for device */
               struct file  *filp)		/* I - Open file */
{
  struct lp1394_dev	*temp;			/* Device */


  temp = (struct lp1394_dev *)(filp->private_data);

  printk(KERN_DEBUG "lp1394: Closed lp%d\n", temp->port);

  return (0);
}


/*
 * 'lp1394_scan()' - Scan the IEEE-1394 bus for devices...
 */

static void
lp1394_scan(void)
{
  int 			i;			/* Looping var */
  struct lp1394_dev	*temp;			/* New device */
  char			name[1024];		/* Device filename */


  for (i = 0; i < 4; i ++)
  {
   /*
    * Allocate memory for the device...
    */

    temp = (struct lp1394_dev *)kmalloc(sizeof(struct lp1394_dev), GFP_KERNEL);

   /*
    * Add the device to the list...
    */

    temp->prev = lp1394_last;
    temp->next = 0;

    if (lp1394_last)
      lp1394_last->next = temp;
    else
      lp1394_first = temp;

    lp1394_last = temp;

   /*
    * Add the device filename to the kernel...
    */

    strcpy(name, "ieee1394/lpN");
    name[11] = '0' + i;

    temp->ds = devfs_register(NULL, name, DEVFS_FL_AUTO_OWNER,
                              lp1394_major, i, 0644, &lp1394_fileops, temp);

   /*
    * Add device info...
    */

    strcpy(temp->device_id, "MFG:EPSON;MDL:Stylus Pro 10000CF");

    temp->guid[0] = 0;
    temp->guid[1] = 0;
    temp->guid[2] = 0;
    temp->guid[3] = 0;
    temp->guid[4] = 0;
    temp->guid[5] = 0;
    temp->guid[6] = 0;
    temp->guid[7] = 0;

    temp->port = 0;
    temp->node = i;

   /*
    * Initialize semaphore...
    */

    sema_init(&(temp->sem), 1);

   /*
    * Log the device we've found...
    */

    printk(KERN_INFO "lp1394: Added device \"/dev/%s\" - device_id=\"%s\"\n",
           name, temp->device_id);
  }
}


/*
 * 'lp1394_write()' - Write data to the printer.
 */

static ssize_t					/* O  - Number of bytes written */
lp1394_write(struct file *filp,			/* I  - Open file */
             const char  *buf,			/* I  - Buffer for data */
	     size_t      buflen,		/* I  - Number of bytes */
	     loff_t      *fpos)			/* IO - File position */
{
  struct lp1394_dev	*temp;			/* Device */


  temp = (struct lp1394_dev *)(filp->private_data);

  printk(KERN_DEBUG "lp1394: Writing %d bytes to lp%d\n", buflen, temp->port);

  *fpos = *fpos + buflen;

  return (buflen);
}


/*
 * End of "$Id: lp1394.c,v 1.1 2002/05/19 23:10:18 mike Exp $".
 */
