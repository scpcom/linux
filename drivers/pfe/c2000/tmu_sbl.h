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


#ifndef _TMU_SBL_H_
#define _TMU_SBL_H_

#include "types.h"

#if defined(COMCERTO_2000_TMU)

#define MAX_SHAPERS		10
#define MAX_SCHEDULERS		8
#define MAX_QUEUES		16
#define MAX_SCHEDULER_QUEUES	8

#define NUM_SHAPERS		5
#define NUM_SCHEDULERS		4
#define NUM_QUEUES		16
#define NUM_SCHEDULER_QUEUES	4


#define QM_ALG_PQ	0
#define QM_ALG_CBWFQ	1
#define QM_ALG_DWRR	2
#define QM_ALG_LAST	QM_ALG_DWRR

#define QM_RATE_LIMIT_OFF 0
#define QM_RATE_LIMIT_ON  1


//#define QM_RX_MTU 			1518

//#define QM_IFG_SIZE	20
//#define QM_FCS_SIZE	4

typedef struct tQM_QDesc {
//	PMetadata head;			// output queue head pointer
//	PMetadata tail;			// output queue tail pointer
//	U16 qdepth;			// number of packets on this output queue
//	U16 max_qdepth;			// maximum number of packets allowed on this output queue
	U8 shaper_num;			// number of shaper assigned to this queue
	U8 sched_num;			// number of scheduler assigned to this queue
//	U8 sched_num_config;		// configured scheduler assigned to this queue
//	U8 init_flag;			// set to TRUE when first packet is enqueued
	U8 alg;				// scheduling algorithm for assigned scheduler
//	U8 unused[3];
//	U32 weight;			// queue weight
#if 0
	union {           		// each struct in this union is for a specific algorithm
		struct {      // CBWFQ
			U32 finish_time;    	// "time" to transmit next packet
		};

		struct {      // DWRR
			int credits;		// available credits remaining
		};
	};
	U32 pad[1];			// keep structure size a multiple of 32 (cache line size)
#endif
} QM_QDesc, *PQM_QDesc;

typedef struct tQM_Shaper_QDesc {
	U32 baseaddr;
	U32 qmask;			// mask of queues assigned to this shaper
//	U8 enable_flag;			// set to TRUE if shaper is enabled (and qos is enabled)
//	U8 enable_flag_config;		// configured shaper enable flag
//	U8 ifg;				// extra IFG amount to add to packet length
//	U8 unused[1];
//	U32 tokens_per_clock_period;	// bits worth of tokens available on every 1 msec clock period
//	int tokens_available;		// bits available to transmit 
//	int bucket_size;		// max bucket size in bytes 
//	U32 pad2[3];			// keep structure size a multiple of 32 (cache line size)
} QM_ShaperDesc, *PQM_ShaperDesc;

typedef struct tQM_Sched_QDesc {
	U32 baseaddr;
	U32 qmask;			// mask of queues assigned to this scheduler
	U8 alg;				// current scheduling algorithm
	U32 queue_mask[NUM_SCHEDULER_QUEUES];	// absolute queue number (written as a mask) for each of the scheduler queues
//	U8 alg_config;			// configured scheduling algorithm
#if 0
	union {				// each struct in this union is for a specific algorithm
		struct {	// DWRR
			U8 current_qnum;	// current active queue number
			U8 next_flag;		// set to TRUE when current queue number must be updated
		};
	};
	U8 init_flag;
#endif
} QM_SchedDesc, *PQM_SchedDesc;


// commands

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
	unsigned short unused;
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
	U32 tokens_per_clock_period[NUM_SHAPERS];	// bits worth of tokens available on every 1 msec clock period
	U32 bucket_size[NUM_SHAPERS];		// max bucket size in bytes 

	U32 sched_qmask[NUM_SCHEDULERS];
	U8 sched_alg[NUM_SCHEDULERS];				// current scheduling algorithm
	
	U16 max_qdepth[NUM_QUEUES];
	

}__attribute__((packed)) QosQueryCmd, *pQosQueryCmd;


void tmu_init(void);
void M_QM_process_packet(void);

#elif defined(COMCERTO_2000_CLASS) || defined(COMCERTO_2000_CONTROL)

#define	NUM_DSCP_VALUES		64

typedef struct _tQoSDSCPQmodCommand {
	unsigned short queue ;
	unsigned short num_dscp;
	unsigned char dscp[NUM_DSCP_VALUES];
}QoSDSCPQmodCommand, *PQoSDSCPQmodCommand;

extern U8 DSCP_to_Qmod[];

#endif

#endif /* _TMU_SBL_H_ */
