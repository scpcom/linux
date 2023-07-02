/*
 *  Copyright (c) 2011, 2014 Freescale Semiconductor, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 */


#ifndef _MODULE_QM_H_
#define _MODULE_QM_H_

#include "types.h"
#include "system.h"

// TMU 3 queue assignments
#define	TMU_QUEUE_IPSEC_IN	0
#define	TMU_QUEUE_IPSEC_OUT	1
#define	TMU_QUEUE_RTP_RELAY	2
#define	TMU_QUEUE_REASSEMBLY	3
#define	TMU_QUEUE_PCAP		4
#define	TMU_QUEUE_LRO		5
#ifdef CFG_WIFI_GUEST
#define	TMU_QUEUE_WIFI_LOPRI	TMU_QUEUE_LRO
#endif
#define	TMU_QUEUE_EXPT_0	6
#define	TMU_QUEUE_EXPT_1	7
#define	TMU_QUEUE_EXPT_2	8
#define	TMU_QUEUE_EXPT_3	9
#define	TMU_QUEUE_EXPT_ARP	10
#define	TMU_QUEUE_WIFI_0	11
#define	TMU_QUEUE_WIFI_1	12
#define	TMU_QUEUE_WIFI_2	13
#define	TMU_QUEUE_WIFI_3	14
#define	TMU_QUEUE_RTP_CUTTHRU	15

#define TMU3_EXPT_ETH_QM ((1 << TMU_QUEUE_EXPT_0) | (1 << TMU_QUEUE_EXPT_1) | (1 << TMU_QUEUE_EXPT_2) | (1 << TMU_QUEUE_EXPT_3) | (1 << TMU_QUEUE_EXPT_ARP) | (1 << TMU_QUEUE_PCAP))
#ifdef CFG_WIFI_GUEST
#define TMU3_EXPT_WIFI_QM ((1 << TMU_QUEUE_WIFI_0) | (1 << TMU_QUEUE_WIFI_1) | (1 << TMU_QUEUE_WIFI_2) | (1 << TMU_QUEUE_WIFI_3) | (1 << TMU_QUEUE_WIFI_LOPRI))
#else
#define TMU3_EXPT_WIFI_QM ((1 << TMU_QUEUE_WIFI_0) | (1 << TMU_QUEUE_WIFI_1) | (1 << TMU_QUEUE_WIFI_2) | (1 << TMU_QUEUE_WIFI_3))
#endif
#define TMU3_EXPT_ARP_QM (1 << TMU_QUEUE_EXPT_ARP)
#define TMU3_EXPT_PCAP_QM (1 << TMU_QUEUE_PCAP)

#if defined(COMCERTO_2000_CLASS) || defined(COMCERTO_2000_UTIL)
/** Retrieves TMU3 queue
 *
 * @param port		port number
 * @param prio_queue	priority queue number
 */
static inline u32 get_tmu3_queue(u32 port, u32 prio_queue)
{
	u32 tmu3_queue;

#ifdef CFG_WIFI_OFFLOAD
	if (IS_WIFI_PORT(port) || IS_EXPT_WIFI_PORT(port))
	{
		if (prio_queue > 3)
			prio_queue = 3;
#ifdef CFG_WIFI_GUEST
		if (IS_WIFI_GUEST_PORT(port))
			tmu3_queue = TMU_QUEUE_WIFI_LOPRI;
		else
#endif
			tmu3_queue = TMU_QUEUE_WIFI_0 + prio_queue;
	}
	else
#endif
	{
		if (prio_queue > 4)
			prio_queue = 4;
		tmu3_queue = TMU_QUEUE_EXPT_0 + prio_queue;
	}

	return tmu3_queue;
}
#endif

#if defined(COMCERTO_2000_CONTROL) || defined (COMCERTO_2000_TMU)

// NOTE: The NUM_SHAPERS value is the number of hardware shapers
//	configurable from CMM (not including the port shaper).
#define NUM_SHAPERS		8

#define NUM_HW_SHAPERS		10
#define NUM_SW_SHAPERS		4
#define NUM_SCHEDULERS		8
#define NUM_QUEUES		16
#define MAX_SCHEDULER_QUEUES	8
#define NO_SHAPER		0xFF
#define DEFAULT_SHAPER		0
#define PORT_SHAPER_NUM		0xffff
#define PORT_SHAPER_QMASK	((1 << NUM_QUEUES) - 1)
#define PORT_SHAPER_QMASK_TMU3	0x7ff0
#define	PORT_SHAPER_INDEX	(NUM_HW_SHAPERS - 1)

//#define DEFAULT_MAX_QDEPTH 	96

#define QM_ALG_PQ	0
#define QM_ALG_CBWFQ	1
#define QM_ALG_DWRR	2
#define QM_ALG_RR	3
#define QM_ALG_LAST	QM_ALG_RR

#define QM_RX_MTU       1518

#define QM_IFG_SIZE     20
#define QM_CMD_SIZE	128


#define QDEPTH_CONFIG		(1 << 0)
//#define QWEIGHT_CONFIG		(1 << 1)
#define IFG_CONFIG      	(1 << 2)
#define SCHEDULER_CONFIG	(1 << 3)
#define SHAPER_CONFIG 		(1 << 4)

#define EXPT_TYPE_ETH	0x0
#define EXPT_TYPE_WIFI	0x1
#define EXPT_TYPE_ARP	0x2
#define EXPT_TYPE_PCAP	0x3

typedef struct tQM_QDesc {
	U8 shaper_num;			// number of shaper assigned to this queue
	U8 sched_num;			// number of scheduler assigned to this queue
} QM_QDesc, *PQM_QDesc;

typedef struct tQM_ShaperDesc {
	U32 qmask;			// mask of queues assigned to this shaper
	U32 shaper_rate;
} QM_ShaperDesc, *PQM_ShaperDesc;

typedef struct tQM_SchedDesc {
	U32 qmask;			// mask of queues assigned to this scheduler
	U8 alg;				// current scheduling algorithm
	U8 numqueues;			// number of queues assigned to this scheduler
	U8 current_qnum;		// saved qnum for RR algorithm
	U8 pad[1];
	U8 queue_list[MAX_SCHEDULER_QUEUES];	// queue numbers assigned to this scheduler
} QM_SchedDesc, *PQM_SchedDesc;

typedef struct tQM_context {
	U8 tmu_id;			// TMU id #
	U8 num_hw_shapers;		// number of active HW shapers (not including port shaper)
	U8 num_sw_shapers;		// number of active SW shapers
	U8 num_sched;			// number of schedulers
	U8 chip_revision;		// chip revision level
	QM_ShaperDesc hw_shaper[NUM_HW_SHAPERS];	// HW shaper descriptors
	QM_ShaperDesc sw_shaper[NUM_SW_SHAPERS];	// SW shaper descriptors
	QM_SchedDesc sched[NUM_SCHEDULERS];	// scheduler descriptors
	U32 q_len_masks[NUM_QUEUES];
	U8 qresult_regoffset[NUM_QUEUES];
	U16 weight[NUM_QUEUES];
} __attribute__((aligned(32))) QM_context, *PQM_context;

typedef struct tQM_ShaperDesc_ctl {
        U8  enable;                    // enable or disable of a shaper.
	U8  int_wt;
	U16 frac_wt;
	U32 clk_div;
        U32 max_credit;
} QM_ShaperDesc_ctl, *PQM_ShaperDesc_ctl;



typedef struct tQM_context_ctl {
        U8 port;
	U8 tmu_id;
	U8 num_hw_shapers;
	U8 num_sw_shapers;
	U8 num_sched;
	U8 sched_mask;
        U8 ifg;
	U8 pad[1];

        U16 weight[NUM_QUEUES];
        U16 max_qdepth[NUM_QUEUES];

	QM_ShaperDesc hw_shaper[NUM_HW_SHAPERS];
	QM_ShaperDesc sw_shaper[NUM_SW_SHAPERS];
	U32 bucket_size[NUM_HW_SHAPERS];
	QM_SchedDesc  sched[NUM_SCHEDULERS];
	QM_QDesc      q[NUM_QUEUES];
        QM_ShaperDesc_ctl hw_shaper_ctl[NUM_HW_SHAPERS];
        QM_ShaperDesc_ctl sw_shaper_ctl[NUM_SW_SHAPERS];

	U32 q_len_masks[NUM_QUEUES];
	U8 qresult_regoffset[NUM_QUEUES];
} __attribute__((aligned(32))) QM_context_ctl, *PQM_context_ctl;

extern QM_context g_qm_context;
extern U8 g_qm_cmd_info[QM_CMD_SIZE];
extern U32 tx_trans[NUM_QUEUES];

#endif

#if defined(COMCERTO_2000_CONTROL)

#include "pfe_mod.h"

#define QM_GET_CONTEXT(output_port) (gQMpCtx[output_port])
#define QM_GET_QOSOFF_CONTEXT(output_port) (&gQMQosOffCtx[output_port])

// commands
#define QM_ENABLE 	0x1
#define QM_DISABLE 	0x0

#define	QM_INITIAL_ENABLE_STATE	QM_ENABLE

#define EXPT_PORT_ID	GEM_PORTS

#define DEFAULT_EXPT_RATE	1000  /* 10000 packets per msec
					(i.e 1000 packets per usec) */

#define EXPT_CTRLQ_CONFIG	(1 << 0)
#define EXPT_DSCP_CONFIG	(1 << 1)

typedef struct _tQosEnableCommand {
	unsigned short port;
	unsigned short enable_flag;
}QosEnableCommand, *PQosEnableCommand;

typedef struct _tQueueQosEnableCommand {
	unsigned short port;
	unsigned short enable_flag;
	unsigned int queue_qosenable_mask; // Bit mask of queues on which Qos is enabled
}QueueQosEnableCommand, *PQueueQosEnableCommand;

typedef struct _tQosSchedulerCommand {
	unsigned short port;
	unsigned short alg;
}QosSchedulerCommand, *PQosSchedulerCommand;

typedef struct _tQosNhighCommand {
	unsigned short port;
	unsigned short nhigh;
}QosNhighCommand, *PQosNhighCommand;

typedef struct _tQosMaxtxdepthCommand {
	unsigned short port;
	unsigned short maxtxdepth;
}QosMaxtxdepthCommand, *PQosMaxtxdepthCommand;

typedef struct _tQosMaxqdepthCommand {
	unsigned short port;
	unsigned short maxqdepth[NUM_QUEUES];
}QosMaxqdepthCommand, *PQosMaxqdepthCommand;

typedef struct _tQosWeightCommand {
	unsigned short port;
	unsigned short weight[NUM_QUEUES];
}QosWeightCommand, *PQosWeightCommand;

typedef struct _tQosResetCommand {
	unsigned short port;
}QosResetCommand, *PQosResetCommand;

typedef struct _tQosShaperConfigCommand {
	unsigned short port;
	unsigned short shaper_num;
	unsigned short enable_disable_control;
	unsigned char ifg;
	unsigned char ifg_change_flag;
	unsigned int rate;
	unsigned int bucket_size;
	unsigned int qmask;
}QosShaperConfigCommand, *PQosShaperConfigCommand;

typedef struct _tQosSchedulerConfigCommand {
	unsigned short port;
	unsigned short sched_num;
	unsigned char alg;
	unsigned char alg_change_flag;
	unsigned int qmask;
}QosSchedulerConfigCommand, *PQosSchedulerConfigCommand;

typedef struct _tQosExptRateCommand {
	unsigned short expt_iftype; // WIFI or ETH or PCAP
	unsigned short pkts_per_msec;
}QosExptRateCommand, *PQosExptRateCommand;

// Data structure passed from CMM to QM containing Rate Limiting configuration information
typedef struct _tQosRlCommand {
    U16	port;	// Ethernet Port
    U16    action;   // Rate_Limiting On or Off
    U32    mask;     // bit mask of rate-limited queues attached to this combination
    U32	aggregate_bandwidth; //Configured Aggregate bandwidth in Kbps
    U32 	bucket_size; // Configurable bucket Sizes in bytes 
} QosRlCommand;

typedef struct _tQosRlQuery
{
	unsigned short action;
	unsigned short mask;
	unsigned int   aggregate_bandwidth;
	unsigned int   bucket_size;	

} __attribute__((packed)) QosRlQuery,*pQosRlQuery;

typedef struct _tQosQueryCommand
{
	U16 action;
	U16 port;
	U32 queue_qosenable_mask;         // bit mask of queues on which Qos is enabled
	U32 max_txdepth;

	U32 shaper_qmask[NUM_SHAPERS];			// mask of queues assigned to this shaper
	U32 shaper_rate[NUM_SHAPERS];		// shaper rate (Kbps)
	U32 bucket_size[NUM_SHAPERS];		// max bucket size in bytes 

	U32 sched_qmask[NUM_SCHEDULERS];
	U8 sched_alg[NUM_SCHEDULERS];				// current scheduling algorithm
	
	U16 max_qdepth[NUM_QUEUES];
	

}__attribute__((packed)) QosQueryCmd, *pQosQueryCmd;

typedef struct _tQosQueryPortInfoCommand
{
	U16 status;
	U16 port;
	U32 queue_qosenable_mask;		// 0 or 0xFFFFFFFF on C2000
	U16 max_txdepth;			// ignored on C2000
	U8 ifg;					// IFG is per-port on C2000
	U8 unused;
}__attribute__((packed)) QosQueryPortInfoCmd, *pQosQueryPortInfoCmd;

typedef struct _tQosQueryQueueCommand
{
	U16 status;
	U16 port;
	U16 queue_num;
	U16 qweight;
	U16 max_qdepth;
	U16 unused;
}__attribute__((packed)) QosQueryQueueCmd, *pQosQueryQueueCmd;

typedef struct _tQosQueryShaperCommand
{
	U16 status;
	U16 port;
	U16 shaper_num;
	U8 enabled;
	U8 unused;				// no per-shaper IFG on C2000
	U32 qmask;				// mask of queues assigned to this shaper
	U32 rate;				// shaper rate (Kbps)
	U32 bucket_size;			// max bucket size in bytes 
}__attribute__((packed)) QosQueryShaperCmd, *pQosQueryShaperCmd;

typedef struct _tQosQuerySchedCommand
{
	U16 status;
	U16 port;
	U16 sched_num;
	U8 alg;
	U8 unused;
	U32 qmask;				// mask of queues assigned to this scheduler
}__attribute__((packed)) QosQuerySchedCmd, *pQosQuerySchedCmd;

int qm_init(void);
void qm_exit(void);
u32 qm_read_drop_stat(u32 tmu, u32 queue, u32 *total_drops, int do_reset);
int qm_cal_shaperwts(u32 rate, PQM_ShaperDesc_ctl shaper_ctl);

extern PQM_context_ctl gQMpCtx[];
extern QM_context_ctl gQMCtx[];
extern QM_context_ctl gQMExptCtx;
extern QM_context_ctl gQMQosOffCtx[];
extern U16 M_expt_cmdproc(U16 cmd_code, U16 cmd_len, U16 *p);

#endif

#if defined(COMCERTO_2000_TMU)
extern U32 pending_ticks;
extern int sw_shaper_tokens_available[NUM_SW_SHAPERS];

void tmu_init(void);
U32 M_QM_check_packet(void);
void M_QM_process_packet(U32 ready_mask);
static inline void QM_shp_soft_tick_update(void)
{
	int i, j;
	U32 bucket_size;

	/* Update all soft shapers token buckets */
	for (i = 0; i < g_qm_context.num_sw_shapers; i++) {
		if(g_qm_context.sw_shaper[i].shaper_rate)
		{
			bucket_size = g_qm_context.sw_shaper[i].shaper_rate * 8;
			for (j = 0; j < pending_ticks; j++)
			{
				sw_shaper_tokens_available[i] += g_qm_context.sw_shaper[i].shaper_rate;
				if (sw_shaper_tokens_available[i] >= bucket_size)
				{
					sw_shaper_tokens_available[i] = bucket_size;
					break;
				}
			}
		}
	}
}


#elif defined(COMCERTO_2000_CLASS) || defined(COMCERTO_2000_CONTROL) || defined(COMCERTO_2000_UTIL)

#define	NUM_DSCP_VALUES		64

typedef struct _tQoSDSCPQmodCommand {
	unsigned short queue ;
	unsigned short num_dscp;
	unsigned char dscp[NUM_DSCP_VALUES];
}QoSDSCPQmodCommand, *PQoSDSCPQmodCommand;

extern U8 DSCP_to_Qmod[NUM_DSCP_VALUES];
extern U8 DSCP_to_Q[NUM_DSCP_VALUES];

#endif

#endif /* _MODULE_QM_H_ */
