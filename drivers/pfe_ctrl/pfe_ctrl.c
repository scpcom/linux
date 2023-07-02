/*
 *
 *  Copyright (C) 2007 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/kthread.h>
#else
#include "platform.h"
#endif

#include "pfe_mod.h"
#include "pfe_ctrl.h"

#include "pfe_ctrl_hal.h"
#include "__pfe_ctrl.h"

static struct pe_sync_mailbox CLASS_DMEM_SH2(sync_mailbox);
static struct pe_sync_mailbox TMU_DMEM_SH2(sync_mailbox);
static struct pe_sync_mailbox UTIL_DMEM_SH2(sync_mailbox);

static struct pe_msg_mailbox CLASS_DMEM_SH2(msg_mailbox);
static struct pe_msg_mailbox TMU_DMEM_SH2(msg_mailbox);
static struct pe_msg_mailbox UTIL_DMEM_SH2(msg_mailbox);

static u32 CLASS_DMEM_SH2(resume);
static u32 TMU_DMEM_SH2(resume);
static u32 UTIL_DMEM_SH2(resume);

static int pfe_ctrl_timer(void *data);

static int initialized = 0;

#define TIMEOUT_MS	1000

int relax(unsigned long end)
{
#ifdef __KERNEL__
	if (time_after(jiffies, end)) {
		if (time_after(jiffies, end + (TIMEOUT_MS * HZ) / 1000)) {
			return -1;
		}

		if (need_resched())
			schedule();
	}
#else
                udelay(1);
#endif

	return 0;
}


int pe_send_cmd(unsigned short cmd_type, unsigned short action, unsigned long param1, unsigned long param2)
{
        struct pfe_ctrl *ctrl = &pfe->ctrl;
        if (pe_request(ctrl, UTIL_ID, cmd_type, action, param1, param2) < 0)
                return -1;
        return 0;
}


void pfe_ctrl_suspend(struct pfe_ctrl *ctrl)
{
	int id;

	kthread_stop(ctrl->timer_thread);

	mutex_lock(&ctrl->mutex);

	initialized = 0;
	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
			pe_dmem_write(id, cpu_to_be32(0x1), (unsigned long)virt_to_class_dmem(&class_resume), 4);

	for (id = TMU0_ID; id <= TMU_MAX_ID; id++)
			pe_dmem_write(id, cpu_to_be32(0x1), (unsigned long)virt_to_class_dmem(&tmu_resume), 4);

	pe_dmem_write(UTIL_ID, cpu_to_be32(0x1), (unsigned long)virt_to_class_dmem(&util_resume), 4);

	pe_sync_stop(&pfe->ctrl, 0xFF);

	mutex_unlock(&ctrl->mutex);
}

void pfe_ctrl_resume(struct pfe_ctrl *ctrl)
{
	mutex_lock(&ctrl->mutex);
	initialized = 1;
	pe_start(&pfe->ctrl, 0xFF);
	mutex_unlock(&ctrl->mutex);

	ctrl->timer_thread = kthread_create(pfe_ctrl_timer, ctrl, "pfe_ctrl_timer");

	wake_up_process(ctrl->timer_thread);
}

/** PE sync stop.
* Stops packet processing for a list of PE's (specified using a bitmask).
* The caller must hold ctrl->mutex.
*
* @param ctrl		Control context
* @param pe_mask	Mask of PE id's to stop
*
*/
int pe_sync_stop(struct pfe_ctrl *ctrl, int pe_mask)
{
	struct pe_sync_mailbox *mbox;
	int pe_stopped = 0;
	unsigned long end = jiffies + 2;
	int i;

	for (i = 0; i < MAX_PE; i++)
		if (pe_mask & (1 << i)) {
			mbox = (void *)ctrl->sync_mailbox_baseaddr[i];

			pe_dmem_write(i, cpu_to_be32(0x1), (unsigned long)&mbox->stop, 4);
		}

	while (pe_stopped != pe_mask) {
		for (i = 0; i < MAX_PE; i++)
			if ((pe_mask & (1 << i)) && !(pe_stopped & (1 << i))) {
				mbox = (void *)ctrl->sync_mailbox_baseaddr[i];

				if (pe_dmem_read(i, (unsigned long)&mbox->stopped, 4) & cpu_to_be32(0x1))
					pe_stopped |= (1 << i);
			}

		if (relax(end) < 0)
			goto err;
	}

	return 0;

err:
	printk(KERN_ERR "%s: timeout, %x %x\n", __func__, pe_mask, pe_stopped);

	for (i = 0; i < MAX_PE; i++)
		if (pe_mask & (1 << i)) {
			mbox = (void *)ctrl->sync_mailbox_baseaddr[i];

			pe_dmem_write(i, cpu_to_be32(0x0), (unsigned long)&mbox->stop, 4);
	}

	return -EIO;
}

/** PE start.
* Starts packet processing for a list of PE's (specified using a bitmask).
* The caller must hold ctrl->mutex.
*
* @param ctrl		Control context
* @param pe_mask	Mask of PE id's to start
*
*/
void pe_start(struct pfe_ctrl *ctrl, int pe_mask)
{
	struct pe_sync_mailbox *mbox;
	int i;

	for (i = 0; i < MAX_PE; i++)
		if (pe_mask & (1 << i)) {

			mbox = (void *)ctrl->sync_mailbox_baseaddr[i];

			pe_dmem_write(i, cpu_to_be32(0x0), (unsigned long)&mbox->stop, 4);
		}
}


/** Sends a control request to a given PE (to copy data to/from internal memory from/to DDR).
* The caller must hold ctrl->mutex.
*
* @param ctrl		Control context
* @param id		PE id
* @param dst		Physical destination address of data
* @param src		Physical source address of data
* @param len		Data length
*
*/
int pe_request(struct pfe_ctrl *ctrl, int id, unsigned short cmd_type, unsigned long dst, unsigned long src, int len)
{
	struct pe_msg_mailbox mbox = {
		.dst = cpu_to_be32(dst),
		.src = cpu_to_be32(src),
		.len = cpu_to_be32(len),
		.request = cpu_to_be32((cmd_type << 16) | 0x1),
	};
	struct pe_msg_mailbox *pmbox = (void *)ctrl->msg_mailbox_baseaddr[id];
	unsigned long end = jiffies + 2;
	u32 rc;

	/* This works because .request is written last */
	pe_dmem_memcpy_to32(id, (unsigned long)pmbox, &mbox, sizeof(mbox));

	while ((rc = pe_dmem_read(id, (unsigned long)&pmbox->request, 4)) & cpu_to_be32(0xffff)) {
		if (relax(end) < 0)
			goto err;
	}

	rc = be32_to_cpu(rc);

	return rc >> 16;

err:
	printk(KERN_ERR "%s: timeout, %x\n", __func__, be32_to_cpu(rc));
	pe_dmem_write(id, cpu_to_be32(0), (unsigned long)&pmbox->request, 4);
	return -EIO;
}



int comcerto_fpp_send_command(u16 fcode, u16 length, u16 *payload, u16 *rlen, u16 *rbuf)
{
	struct pfe_ctrl *ctrl;

	if (!initialized) {
		printk(KERN_ERR "pfe control not initialized\n");
		*rlen = 2;
		rbuf[0] =  1; /* ERR_UNKNOWN_COMMAND */
		goto out;
	}

	ctrl = &pfe->ctrl;

	mutex_lock(&ctrl->mutex);

	__pfe_ctrl_cmd_handler(fcode, length, payload, rlen, rbuf);

	mutex_unlock(&ctrl->mutex);

out:
	return 0;
}
EXPORT_SYMBOL(comcerto_fpp_send_command);

/**
 * comcerto_fpp_send_command_simple - 
 *
 *	This function is used to send command to FPP in a synchronous way. Calls to the function blocks until a response
 *	from FPP is received. This API can not be used to query data from FPP
 *	
 * Parameters
 *	fcode:		Function code. FPP function code associated to the specified command payload
 *	length:		Command length. Length in bytes of the command payload
 *	payload:	Command payload. Payload of the command sent to the FPP. 16bits buffer allocated by the client's code and sized up to 256 bytes
 *
 * Return values
 *	0:	Success
 *	<0:	Linux system failure (check errno for detailed error condition)
 *	>0:	FPP returned code
 */
int comcerto_fpp_send_command_simple(u16 fcode, u16 length, u16 *payload)
{
	u16 rbuf[128];
	u16 rlen;
	int rc;

	if (!initialized) {
		printk(KERN_ERR "pfe control not initialized\n");
		rbuf[0] = 1; /* ERR_UNKNOWN_COMMAND */
		goto out;
	}

	rc = comcerto_fpp_send_command(fcode, length, payload, &rlen, rbuf);

	/* if a command delivery error is detected, do not check command returned code */
	if (rc < 0)
		return rc;

out:
	/* retrieve FPP command returned code. Could be error or acknowledgment */
	rc = rbuf[0];

	return rc;
}
EXPORT_SYMBOL(comcerto_fpp_send_command_simple);


static void comcerto_fpp_workqueue(struct work_struct *work)
{
	struct pfe_ctrl *ctrl = container_of(work, struct pfe_ctrl, work);
	struct fpp_msg *msg;
	unsigned long flags;
	u16 rbuf[128];
	u16 rlen;
	int rc;

	spin_lock_irqsave(&ctrl->lock, flags);

	while (!list_empty(&ctrl->msg_list)) {

		msg = list_entry(ctrl->msg_list.next, struct fpp_msg, list);

		list_del(&msg->list);

		spin_unlock_irqrestore(&ctrl->lock, flags);

		rc = comcerto_fpp_send_command(msg->fcode, msg->length, msg->payload, &rlen, rbuf);

		/* send command response to caller's callback */
		if (msg->callback != NULL)
			msg->callback(msg->data, rc, rlen, rbuf);

		pfe_kfree(msg);

		spin_lock_irqsave(&ctrl->lock, flags);
	}

	spin_unlock_irqrestore(&ctrl->lock, flags);
}

/**
 * comcerto_fpp_send_command_atomic -
 *
 *	This function is used to send command to FPP in an asynchronous way. The Caller specifies a function pointer
 *	that is called by the FPP Comcerto driver when command reponse from FPP engine is received. This API can be also
 *	used to query data from FPP. Queried data are returned through the specified client's callback function
 *
 * Parameters
 *	fcode:		Function code. FPP function code associated to the specified command payload
 *	length:		Command length. Length in bytes of the command payload
 *	payload:	Command payload. Payload of the command sent to the FPP. 16bits buffer allocated by the client's code and sized up to 256 bytes
 *	callback:	Client's callback handler for FPP response processing
 *	data:		Client's private data. Not interpreted by the FPP driver and sent back to the Client as a reference (client's code own usage)
 *
 * Return values
 *	0:	Success
 *	<0:	Linux system failure (check errno for detailed error condition)
 **/
int comcerto_fpp_send_command_atomic(u16 fcode, u16 length, u16 *payload, void (*callback)(unsigned long, int, u16, u16 *), unsigned long data)
{
	struct pfe_ctrl *ctrl;
	struct fpp_msg *msg;
	unsigned long flags;
	int rc;

	if (!initialized) {
		printk(KERN_ERR "pfe control not initialized\n");
		rc = -EIO;
		goto err0;
	}

	ctrl = &pfe->ctrl;

	if (length > FPP_MAX_MSG_LENGTH) {
		rc = -EINVAL;
		goto err0;
	}

	msg = pfe_kmalloc(sizeof(struct fpp_msg) + length, GFP_ATOMIC);
	if (!msg) {
		rc = -ENOMEM;
		goto err0;
	}

	/* set caller's callback function */
	msg->callback = callback;
	msg->data = data;

	msg->payload = (u16 *)(msg + 1);

	msg->fcode = fcode;
	msg->length = length;
	memcpy(msg->payload, payload, length);

	spin_lock_irqsave(&ctrl->lock, flags);

	list_add(&msg->list, &ctrl->msg_list);

	spin_unlock_irqrestore(&ctrl->lock, flags);

	schedule_work(&ctrl->work);

	return 0;

err0:
	return rc;
}

EXPORT_SYMBOL(comcerto_fpp_send_command_atomic);



/** Sends a control request to TMU PE ).
* The caller must hold ctrl->mutex.
*
* @param ctrl           Control context
* @param id             TMU id
* @param tmu_cmd_bitmask   Bitmask of commands sent to TMU
*
*/
int tmu_pe_request(struct pfe_ctrl *ctrl, int id, unsigned int tmu_cmd_bitmask)
{
	if ((id < TMU0_ID) || (id > TMU_MAX_ID))
		return -EIO;

	return (pe_request(ctrl, id, 0, tmu_cmd_bitmask, 0, 0));
}





static int pfe_ctrl_send_command_simple(u16 fcode, u16 length, u16 *payload)
{
	u16 rbuf[128];
	u16 rlen;
	int rc;

	/* send command to FE */
	comcerto_fpp_send_command(fcode, length, payload, &rlen, rbuf);

	/* retrieve FE command returned code. Could be error or acknowledgment */
	rc = rbuf[0];

	return rc;
}


/**
 * comcerto_fpp_register_event_cb -
 *
 */
int comcerto_fpp_register_event_cb(int (*event_cb)(u16, u16, u16*))
{
	struct pfe_ctrl *ctrl;

	if (!initialized) {
		printk(KERN_ERR "pfe control not initialized\n");
		return -EIO;
	}

	ctrl = &pfe->ctrl;

	/* register FCI callback used for asynchrounous event */
	ctrl->event_cb = event_cb;

	return 0;
}
EXPORT_SYMBOL(comcerto_fpp_register_event_cb);


int pfe_ctrl_set_eth_state(int id, unsigned int state, unsigned char *mac_addr)
{
	unsigned char msg[20];
	int rc;

	memset(msg, 0, 20);

	msg[0] = id;

	if (state) {
		memcpy(msg + 14, mac_addr, 6);

		if ((rc = pfe_ctrl_send_command_simple(CMD_TX_ENABLE, 20, (unsigned short *)msg)) != 0)
			goto err;

	} else {
		if ((rc = pfe_ctrl_send_command_simple(CMD_TX_DISABLE, 2, (unsigned short *)msg)) != 0)
			goto err;
	}

	return 0;

err:
	return -1;
}


int pfe_ctrl_set_lro(char enable)
{
	unsigned short msg = 0;
	int rc;

	if (enable)
		msg = lro_mode;

	if ((rc = pfe_ctrl_send_command_simple(CMD_RX_LRO, 2, (unsigned short *)&msg)) != 0)
		return -1;

	return 0;
}

#ifdef CFG_PCAP
typedef struct _tQosExptRateCommand {
        unsigned short expt_iftype; // PCAP
        unsigned short pkts_per_msec;
}QosExptRateCommand, *PQosExptRateCommand;

int pfe_ctrl_set_pcap(char enable)
{
	unsigned short msg = 0;
	int rc;
	msg = enable;

	if ((rc = pfe_ctrl_send_command_simple(CMD_PKTCAP_ENABLE, 2, (unsigned short *)&msg)) != 0)
		return -1;

	return 0;
}

int pfe_ctrl_set_pcap_ratelimit(u32 pkts_per_msec)
{
	QosExptRateCommand pcap_ratelimit;
	int rc;

	pcap_ratelimit.expt_iftype = EXPT_TYPE_PCAP;	
	pcap_ratelimit.pkts_per_msec = pkts_per_msec;

	if ((rc = pfe_ctrl_send_command_simple(CMD_QM_EXPT_RATE, sizeof(QosExptRateCommand), (unsigned short *)&pcap_ratelimit)) != 0)
		return -1;

        return 0;

}
#endif
/** Control code timer thread.
*
* A kernel thread is used so that the timer code can be run under the control path mutex.
* The thread wakes up regularly and checks if any timer in the timer list as expired.
* The timers are re-started automatically.
* The code tries to keep the number of times a timer runs per unit time constant on average,
* if the thread scheduling is delayed, it's possible for a particular timer to be scheduled in
* quick succession to make up for the lost time.
*
* @param data	Pointer to the control context structure
*
* @return	0 on sucess, a negative value on error
*
*/
static int pfe_ctrl_timer(void *data)
{
	struct pfe_ctrl *ctrl = data;
	TIMER_ENTRY *timer, *next;

	printk(KERN_INFO "%s\n", __func__);

	while (1)
	{
		schedule_timeout_interruptible(ctrl->timer_period);

		mutex_lock(&ctrl->mutex);

		list_for_each_entry_safe(timer, next, &ctrl->timer_list, list)
		{
			if (time_after(jiffies, timer->timeout))
			{
				timer->timeout += timer->period;

				timer->handler();
			}
		}

		mutex_unlock(&ctrl->mutex);

		if (kthread_should_stop())
			break;
	}

	printk(KERN_INFO "%s exiting\n", __func__);

	return 0;
}


int pfe_ctrl_init(struct pfe *pfe)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	int id;
	int rc;

	printk(KERN_INFO "%s\n", __func__);

	mutex_init(&ctrl->mutex);
	spin_lock_init(&ctrl->lock);

	ctrl->timer_period = HZ / TIMER_TICKS_PER_SEC;

	INIT_LIST_HEAD(&ctrl->timer_list);

	INIT_WORK(&ctrl->work, comcerto_fpp_workqueue);

	INIT_LIST_HEAD(&ctrl->msg_list);

	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++) {
		ctrl->sync_mailbox_baseaddr[id] = virt_to_class_dmem(&class_sync_mailbox);
		ctrl->msg_mailbox_baseaddr[id] = virt_to_class_dmem(&class_msg_mailbox);
	}

	for (id = TMU0_ID; id <= TMU_MAX_ID; id++) {
		ctrl->sync_mailbox_baseaddr[id] = virt_to_tmu_dmem(&tmu_sync_mailbox);
		ctrl->msg_mailbox_baseaddr[id] = virt_to_tmu_dmem(&tmu_msg_mailbox);
	}

#if !defined(CONFIG_UTIL_DISABLED)
	ctrl->sync_mailbox_baseaddr[UTIL_ID] = virt_to_util_dmem(&util_sync_mailbox);
	ctrl->msg_mailbox_baseaddr[UTIL_ID] = virt_to_util_dmem(&util_msg_mailbox);
#endif

	ctrl->hash_array_baseaddr = pfe->ddr_baseaddr + ROUTE_TABLE_BASEADDR;
	ctrl->hash_array_phys_baseaddr = pfe->ddr_phys_baseaddr + ROUTE_TABLE_BASEADDR;
	ctrl->ipsec_lmem_phys_baseaddr =  CBUS_VIRT_TO_PFE(LMEM_BASE_ADDR + IPSEC_LMEM_BASEADDR);
	ctrl->ipsec_lmem_baseaddr = (LMEM_BASE_ADDR + IPSEC_LMEM_BASEADDR);

	ctrl->timer_thread = kthread_create(pfe_ctrl_timer, ctrl, "pfe_ctrl_timer");
	if (IS_ERR(ctrl->timer_thread))
	{
		printk (KERN_ERR "%s: kthread_create() failed\n", __func__);
		rc = PTR_ERR(ctrl->timer_thread);
		goto err0;
	}
	
	ctrl->dma_pool = dma_pool_create("pfe_dma_pool_256B", pfe->dev, DMA_BUF_SIZE_256, DMA_BUF_MIN_ALIGNMENT, DMA_BUF_BOUNDARY);
	if (!ctrl->dma_pool)
	{
		printk (KERN_ERR "%s: dma_pool_create() failed\n", __func__);
		rc = -ENOMEM;
		goto err1;
	}

	ctrl->dma_pool_512 = dma_pool_create("pfe_dma_pool_512B", pfe->dev, DMA_BUF_SIZE_512, DMA_BUF_MIN_ALIGNMENT, DMA_BUF_BOUNDARY);
	if (!ctrl->dma_pool_512)
	{
		printk (KERN_ERR "%s: dma_pool_create() failed\n", __func__);
		rc = -ENOMEM;
		goto err2;
	}

	ctrl->dma_pool_128 = dma_pool_create("pfe_dma_pool_128B", pfe->dev, DMA_BUF_SIZE_128, DMA_BUF_MIN_ALIGNMENT, DMA_BUF_BOUNDARY);
	if (!ctrl->dma_pool_128)
	{
		printk (KERN_ERR "%s: dma_pool_create(pfe_dma_pool_128B) failed\n", __func__);
		rc = -ENOMEM;
		goto err3;
	}


	ctrl->dev = pfe->dev;

	mutex_lock(&ctrl->mutex);

	/* Initialize interface to fci */
	rc = __pfe_ctrl_init();

	mutex_unlock(&ctrl->mutex);

	if (rc < 0)
		goto err4;

	wake_up_process(ctrl->timer_thread);

	printk(KERN_INFO "%s finished\n", __func__);

	initialized = 1;

	return 0;

err4:
	dma_pool_destroy(ctrl->dma_pool_128);
err3:
	dma_pool_destroy(ctrl->dma_pool_512);

err2:
	dma_pool_destroy(ctrl->dma_pool);

err1:
	kthread_stop(ctrl->timer_thread);

err0:
	return rc;
}


void pfe_ctrl_exit(struct pfe *pfe)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;

	printk(KERN_INFO "%s\n", __func__);

	initialized = 0;

	__pfe_ctrl_exit();

	dma_pool_destroy(ctrl->dma_pool);

	dma_pool_destroy(ctrl->dma_pool_512);
	dma_pool_destroy(ctrl->dma_pool_128);

	kthread_stop(ctrl->timer_thread);
}
