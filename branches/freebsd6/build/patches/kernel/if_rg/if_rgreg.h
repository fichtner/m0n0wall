/*
 * Copyright (c) 1997, 1998
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/re/if_rereg.h,v 1.14.2.1 2001/07/19 18:33:07 wpaul Exp $
 */

/*#define VERSION(_MainVer,_MinorVer)	((_MainVer)*10+(_MinorVer))*/
/*#define OS_VER	VERSION(5,1)*/
#if __FreeBSD_version < 500000
#define VERSION(_MainVer,_MinorVer)	((_MainVer)*100000+(_MinorVer)*10000)
#else
#define VERSION(_MainVer,_MinorVer)	((_MainVer)*100000+(_MinorVer)*1000)
#endif
#define OS_VER	__FreeBSD_version


/*
 * RealTek RTL8110S/SB/SC register offsets
 */

#define	RG_TPPOLL	0x0038		/* transmit priority polling */

/*
 * RealTek RTL8110S/SB/SC register contents
 */

/* Transmit Priority Polling --- 0x40 */
#define	RG_HPQ		0x80		/* high priority queue polling */
#define	RG_NPQ		0x40		/* normal priority queue polling */
#define	RG_FSWInt	0x01		/* Forced Software Interrupt */


/*
 * RealTek 8129/8139 register offsets
 */

#define	RG_IDR0		0x0000		/* ID register 0 (station addr) */
#define RG_IDR1		0x0001		/* Must use 32-bit accesses (?) */
#define RG_IDR2		0x0002
#define RG_IDR3		0x0003
#define RG_IDR4		0x0004
#define RG_IDR5		0x0005
					/* 0006-0007 reserved */
#define RG_MAR0		0x0008		/* Multicast hash table */
#define RG_MAR1		0x0009
#define RG_MAR2		0x000A
#define RG_MAR3		0x000B
#define RG_MAR4		0x000C
#define RG_MAR5		0x000D
#define RG_MAR6		0x000E
#define RG_MAR7		0x000F

#define RG_TXSTAT0	0x0010		/* status of TX descriptor 0 */
#define RG_TXSTAT1	0x0014		/* status of TX descriptor 1 */
#define RG_TXSTAT2	0x0018		/* status of TX descriptor 2 */
#define RG_TXSTAT3	0x001C		/* status of TX descriptor 3 */

#define RG_TXADDR0	0x0020		/* address of TX descriptor 0 */
#define RG_TXADDR1	0x0024		/* address of TX descriptor 1 */
#define RG_TXADDR2	0x0028		/* address of TX descriptor 2 */
#define RG_TXADDR3	0x002C		/* address of TX descriptor 3 */

#define RG_RXADDR	0x0030		/* RX ring start address */
#define RG_COMMAND	0x0037		/* command register */
#define RG_CURRXADDR	0x0038		/* current address of packet read */
#define RG_CURRXBUF	0x003A		/* current RX buffer address */
#define RG_IMR		0x003C		/* interrupt mask register */
#define RG_ISR		0x003E		/* interrupt status register */
#define RG_TXCFG	0x0040		/* transmit config */
#define RG_RXCFG	0x0044		/* receive config */
#define RG_TIMERCNT	0x0048		/* timer count register */
#define RG_MISSEDPKT	0x004C		/* missed packet counter */
#define RG_EECMD	0x0050		/* EEPROM command register */
#define RG_CFG0		0x0051		/* config register #0 */
#define RG_CFG1		0x0052		/* config register #1 */
#define RG_CFG2		0x0053		/* config register #2 */
#define RG_CFG3		0x0054		/* config register #3 */
#define RG_CFG4		0x0055		/* config register #4 */
#define RG_CFG5		0x0056		/* config register #5 */
					/* 0053-0057 reserved */
#define RG_MEDIASTAT	0x0058		/* media status register (8139) */
					/* 0059-005A reserved */
#define RG_MII		0x005A		/* 8129 chip only */
#define RG_HALTCLK	0x005B
#define RG_MULTIINTR	0x005C		/* multiple interrupt */
#define RG_PCIREV	0x005E		/* PCI revision value */
					/* 005F reserved */
#define RG_PHYAR	0x0060		/* PHY register access */
#define RG_CSIDR	0x0064
#define RG_CSIAR	0x0068
#define RG_PHY_STATUS	0x006C		/* PHY status */
#define RG_ERIDR	0x70
#define RG_ERIAR	0x74
#define RG_EPHYAR	0x0080
#define RG_DBG_reg	0x00D1
#define RG_RxMaxSize	0x00DA
#define RG_CPlusCmd	0x00E0
#define	RG_MTPS		0x00EC

/* Direct PHY access registers only available on 8139 */
#define RG_BMCR		0x0062		/* PHY basic mode control */
#define RG_BMSR		0x0064		/* PHY basic mode status */
#define RG_ANAR		0x0066		/* PHY autoneg advert */
#define RG_LPAR		0x0068		/* PHY link partner ability */
#define RG_ANER		0x006A		/* PHY autoneg expansion */

#define RG_DISCCNT	0x006C		/* disconnect counter */
#define RG_FALSECAR	0x006E		/* false carrier counter */
#define RG_NWAYTST	0x0070		/* NWAY test register */
#define RG_RX_ER	0x0072		/* RX_ER counter */
#define RG_CSCFG	0x0074		/* CS configuration register */
#define RG_LDPS		0x0082		/* Link Down Power Saving */
#define RG_CPCR		0x00E0
#define	RG_IM		0x00E2


/*
 * TX config register bits
 */
#define RG_TXCFG_CLRABRT	0x00000001	/* retransmit aborted pkt */
#define RG_TXCFG_MAXDMA		0x00000700	/* max DMA burst size */
#define RG_TXCFG_CRCAPPEND	0x00010000	/* CRC append (0 = yes) */
#define RG_TXCFG_LOOPBKTST	0x00060000	/* loopback test */
#define RG_TXCFG_IFG		0x03000000	/* interframe gap */

#define RG_TXDMA_16BYTES	0x00000000
#define RG_TXDMA_32BYTES	0x00000100
#define RG_TXDMA_64BYTES	0x00000200
#define RG_TXDMA_128BYTES	0x00000300
#define RG_TXDMA_256BYTES	0x00000400
#define RG_TXDMA_512BYTES	0x00000500
#define RG_TXDMA_1024BYTES	0x00000600
#define RG_TXDMA_2048BYTES	0x00000700

/*
 * Transmit descriptor status register bits.
 */
#define RG_TXSTAT_LENMASK	0x00001FFF
#define RG_TXSTAT_OWN		0x00002000
#define RG_TXSTAT_TX_UNDERRUN	0x00004000
#define RG_TXSTAT_TX_OK		0x00008000
#define RG_TXSTAT_COLLCNT	0x0F000000
#define RG_TXSTAT_CARR_HBEAT	0x10000000
#define RG_TXSTAT_OUTOFWIN	0x20000000
#define RG_TXSTAT_TXABRT	0x40000000
#define RG_TXSTAT_CARRLOSS	0x80000000

/*
 * Interrupt status register bits.
 */
#define RG_ISR_RX_OK		0x0001
#define RG_ISR_RX_ERR		0x0002
#define RG_ISR_TX_OK		0x0004
#define RG_ISR_TX_ERR		0x0008
#define RG_ISR_RX_OVERRUN	0x0010
#define RG_ISR_PKT_UNDERRUN	0x0020
#define RG_ISR_LINKCHG		0x0020
#define RG_ISR_FIFO_OFLOW	0x0040	/* 8139 only */
#define RG_ISR_TDU		0x0080
#define RG_ISR_PCS_TIMEOUT	0x4000	/* 8129 only */
#define RG_ISR_SYSTEM_ERR	0x8000

/*
#define RG_INTRS	\
	(RG_ISR_TX_OK|RG_ISR_RX_OK|RG_ISR_RX_ERR|RG_ISR_TX_ERR|		\
	RG_ISR_RX_OVERRUN|RG_ISR_PKT_UNDERRUN|RG_ISR_FIFO_OFLOW|	\
	RG_ISR_PCS_TIMEOUT|RG_ISR_SYSTEM_ERR)
*/

#define RG_INTRS	\
	(RG_ISR_TX_OK|RG_ISR_RX_OK|RG_ISR_RX_ERR|RG_ISR_TX_ERR|		\
	RG_ISR_RX_OVERRUN|RG_ISR_PKT_UNDERRUN|	\
	RG_ISR_PCS_TIMEOUT|RG_ISR_SYSTEM_ERR)

/*
 * Media status register. (8139 only)
 */
#define RG_MEDIASTAT_RXPAUSE	0x01
#define RG_MEDIASTAT_TXPAUSE	0x02
#define RG_MEDIASTAT_LINK	0x04
#define RG_MEDIASTAT_SPEED10	0x08
#define RG_MEDIASTAT_RXFLOWCTL	0x40	/* duplex mode */
#define RG_MEDIASTAT_TXFLOWCTL	0x80	/* duplex mode */

/*
 * Receive config register.
 */
#define RG_RXCFG_RX_ALLPHYS	0x00000001	/* accept all nodes */
#define RG_RXCFG_RX_INDIV	0x00000002	/* match filter */
#define RG_RXCFG_RX_MULTI	0x00000004	/* accept all multicast */
#define RG_RXCFG_RX_BROAD	0x00000008	/* accept all broadcast */
#define RG_RXCFG_RX_RUNT	0x00000010
#define RG_RXCFG_RX_ERRPKT	0x00000020
#define RG_RXCFG_RX_9356SEL	0x00000040
#define RG_RXCFG_WRAP		0x00000080
#define RG_RXCFG_MAXDMA		0x00000700
#define RG_RXCFG_BUFSZ		0x00001800

#define RG_RXDMA_16BYTES	0x00000000
#define RG_RXDMA_32BYTES	0x00000100
#define RG_RXDMA_64BYTES	0x00000200
#define RG_RXDMA_128BYTES	0x00000300
#define RG_RXDMA_256BYTES	0x00000400
#define RG_RXDMA_512BYTES	0x00000500
#define RG_RXDMA_1024BYTES	0x00000600
#define RG_RXDMA_UNLIMITED	0x00000700

#define RG_RXBUF_8		0x00000000
#define RG_RXBUF_16		0x00000800
#define RG_RXBUF_32		0x00001000
#define RG_RXBUF_64		0x00001800

#define RG_RXRESVERED		0x0000E000

/*
 * Bits in RX status header (included with RX'ed packet
 * in ring buffer).
 */
#define RG_RXSTAT_RXOK		0x00000001
#define RG_RXSTAT_ALIGNERR	0x00000002
#define RG_RXSTAT_CRCERR	0x00000004
#define RG_RXSTAT_GIANT		0x00000008
#define RG_RXSTAT_RUNT		0x00000010
#define RG_RXSTAT_BADSYM	0x00000020
#define RG_RXSTAT_BROAD		0x00002000
#define RG_RXSTAT_INDIV		0x00004000
#define RG_RXSTAT_MULTI		0x00008000
#define RG_RXSTAT_LENMASK	0xFFFF0000

#define RG_RXSTAT_UNFINISHED	0xFFF0		/* DMA still in progress */
/*
 * Command register.
 */
#define RG_CMD_EMPTY_RXBUF	0x0001
#define RG_CMD_TX_ENB		0x0004
#define RG_CMD_RX_ENB		0x0008
#define RG_CMD_RESET		0x0010

/*
 * EEPROM control register
 */
#define RG_EE_DATAOUT		0x01	/* Data out */
#define RG_EE_DATAIN		0x02	/* Data in */
#define RG_EE_CLK		0x04	/* clock */
#define RG_EE_SEL		0x08	/* chip select */
#define RG_EE_MODE		(0x40|0x80)

#define RG_EEMODE_OFF		0x00
#define RG_EEMODE_AUTOLOAD	0x40
#define RG_EEMODE_PROGRAM	0x80
#define RG_EEMODE_WRITECFG	(0x80|0x40)

/* 9346 EEPROM commands */
#define RG_EECMD_WRITE		0x140
#define RG_EECMD_READ		0x180
#define RG_EECMD_ERASE		0x1c0

#define RG_EE_ID			0x00
#define RG_EE_PCI_VID		0x01
#define RG_EE_PCI_DID		0x02
/* Location of station address inside EEPROM */
 #define RG_EE_EADDR		0x07

/*
 * MII register (8129 only)
 */
#define RG_MII_CLK		0x01
#define RG_MII_DATAIN		0x02
#define RG_MII_DATAOUT		0x04
#define RG_MII_DIR		0x80	/* 0 == input, 1 == output */

/*
 * Config 0 register
 */
#define RG_CFG0_ROM0		0x01
#define RG_CFG0_ROM1		0x02
#define RG_CFG0_ROM2		0x04
#define RG_CFG0_PL0		0x08
#define RG_CFG0_PL1		0x10
#define RG_CFG0_10MBPS		0x20	/* 10 Mbps internal mode */
#define RG_CFG0_PCS		0x40
#define RG_CFG0_SCR		0x80

/*
 * Config 1 register
 */
#define RG_CFG1_PWRDWN		0x01
#define RG_CFG1_SLEEP		0x02
#define RG_CFG1_IOMAP		0x04
#define RG_CFG1_MEMMAP		0x08
#define RG_CFG1_RSVD		0x10
#define RG_CFG1_DRVLOAD		0x20
#define RG_CFG1_LED0		0x40
#define RG_CFG1_FULLDUPLEX	0x40	/* 8129 only */
#define RG_CFG1_LED1		0x80

/*
 * The RealTek doesn't use a fragment-based descriptor mechanism.
 * Instead, there are only four register sets, each or which represents
 * one 'descriptor.' Basically, each TX descriptor is just a contiguous
 * packet buffer (32-bit aligned!) and we place the buffer addresses in
 * the registers so the chip knows where they are.
 *
 * We can sort of kludge together the same kind of buffer management
 * used in previous drivers, but we have to do buffer copies almost all
 * the time, so it doesn't really buy us much.
 *
 * For reception, there's just one large buffer where the chip stores
 * all received packets.
 */

#define RG_RX_BUF_SZ		RG_RXBUF_64
#define RG_RXBUFLEN		(1 << ((RG_RX_BUF_SZ >> 11) + 13))
#define RG_TX_LIST_CNT		4		/*  C mode Tx buffer number */
#define RG_TX_BUF_NUM		256		/* Tx buffer number */
#define RG_RX_BUF_NUM		256		/* Rx buffer number */
#define RG_BUF_SIZE		1600		/* Buffer size of descriptor buffer */
#define RG_MIN_FRAMELEN		60
#define RG_TXREV(x)		((x) << 11)
#define RG_RX_RESVERED		RG_RXRESVERED
#define RG_RX_MAXDMA		RG_RXDMA_UNLIMITED
#define RG_TX_MAXDMA		RG_TXDMA_2048BYTES
#define	RG_NTXSEGS		32

#define RG_RXCFG_CONFIG_1 (RG_RX_RESVERED | RG_RX_MAXDMA | RG_RX_BUF_SZ | 0x0E)
#define RG_RXCFG_CONFIG_2 (RG_RX_MAXDMA | RG_RX_BUF_SZ | 0x0E | 0x8000)

#define RG_TXCFG_CONFIG	(RG_TXCFG_IFG|RG_TX_MAXDMA)

#define RG_DESC_ALIGN	256		/* descriptor alignment */

#define RG_ETHER_ALIGN	2

struct rg_chain_data {
	u_int16_t		cur_rx;
	caddr_t			rg_rx_buf;
	caddr_t			rg_rx_buf_ptr;

	struct mbuf		*rg_tx_chain[RG_TX_LIST_CNT];
	u_int8_t		last_tx;	/* Previous Tx OK */
	u_int8_t		cur_tx;		/* Next to TX */
};

union RxDesc
{
	u_int32_t	ul[4];
	struct
	{
		u_int32_t Frame_Length:13;
		u_int32_t TCPF:1;
		u_int32_t UDPF:1;
		u_int32_t IPF:1;
		u_int32_t CRC:1;
		u_int32_t RUNT:1;
		u_int32_t EMS:1;
		u_int32_t RES:1;
		u_int32_t RWT:1;
		u_int32_t FOVF:1;
		u_int32_t BOVF:1;
		u_int32_t RESV:1;
		u_int32_t BAR:1;
		u_int32_t PAM:1;
		u_int32_t MAR:1;
		u_int32_t FAE:1;
		u_int32_t LS:1;
		u_int32_t FS:1;
		u_int32_t EOR:1;
		u_int32_t OWN:1;

		u_int32_t VLAN_TAG:16;
		u_int32_t TAVA:1;
		u_int32_t RESV1:15;

		u_int32_t RxBuffL;
		u_int32_t RxBuffH;
	}so0;	/* symbol owner=0 */
};

union TxDesc
{
	u_int32_t	ul[4];
	struct
	{
		u_int32_t Frame_Length:16;
		u_int32_t TCPCS:1;
		u_int32_t UDPCS:1;
		u_int32_t IPCS:1;
		u_int32_t SCRC:1;
		u_int32_t RESV:6;
		u_int32_t TDMA:1;
		u_int32_t LGSEN:1;
		u_int32_t LS:1;
		u_int32_t FS:1;
		u_int32_t EOR:1;
		u_int32_t OWN:1;

		u_int32_t VLAN_TAG:16;
		u_int32_t TAGC0:1;
		u_int32_t TAGC1:1;
		u_int32_t RESV1:14;

		u_int32_t TxBuffL;
		u_int32_t TxBuffH;
	}so1;	/* symbol owner=1 */
};

struct rg_descriptor
{
	u_int8_t		rx_cur_index;
	u_int8_t		rx_last_index;
	union RxDesc 		*rx_desc;	/* 8 bits alignment */
	struct mbuf		*rx_buf[RG_RX_BUF_NUM];

	u_int8_t		tx_cur_index;
	u_int8_t		tx_last_index;
	union TxDesc		*tx_desc;	/* 8 bits alignment */
	struct mbuf		*tx_buf[RG_TX_BUF_NUM];
	bus_dma_tag_t		rx_desc_tag;
	bus_dmamap_t		rx_desc_dmamap;
	bus_dma_tag_t		rg_rx_mtag;	/* mbuf RX mapping tag */
	bus_dmamap_t		rg_rx_dmamap[RG_RX_BUF_NUM];

	bus_dma_tag_t		tx_desc_tag;
	bus_dmamap_t		tx_desc_dmamap;
	bus_dma_tag_t		rg_tx_mtag;	/* mbuf TX mapping tag */
	bus_dmamap_t		rg_tx_dmamap[RG_TX_BUF_NUM];
};

#define RG_INC(x)		(x = (x + 1) % RG_TX_LIST_CNT)
#define RG_CUR_TXADDR(x)	((x->rg_cdata.cur_tx * 4) + RG_TXADDR0)
#define RG_CUR_TXSTAT(x)	((x->rg_cdata.cur_tx * 4) + RG_TXSTAT0)
#define RG_CUR_TXMBUF(x)	(x->rg_cdata.rg_tx_chain[x->rg_cdata.cur_tx])
#define RG_LAST_TXADDR(x)	((x->rg_cdata.last_tx * 4) + RG_TXADDR0)
#define RG_LAST_TXSTAT(x)	((x->rg_cdata.last_tx * 4) + RG_TXSTAT0)
#define RG_LAST_TXMBUF(x)	(x->rg_cdata.rg_tx_chain[x->rg_cdata.last_tx])

struct rg_type {
	u_int16_t		rg_vid;
	u_int16_t		rg_did;
	char			*rg_name;
};

struct rg_mii_frame {
	u_int8_t		mii_stdelim;
	u_int8_t		mii_opcode;
	u_int8_t		mii_phyaddr;
	u_int8_t		mii_regaddr;
	u_int8_t		mii_turnaround;
	u_int16_t		mii_data;
};

/*
 * MII constants
 */
#define RG_MII_STARTDELIM	0x01
#define RG_MII_READOP		0x02
#define RG_MII_WRITEOP		0x01
#define RG_MII_TURNAROUND	0x02

enum
{
	RG_8129 = 0x01,
	RG_8139,
	MACFG_3,
	MACFG_4,
	MACFG_5,
	MACFG_6,

	MACFG_11,
	MACFG_12,
	MACFG_13,
	MACFG_14,
	MACFG_15,
	MACFG_16,
	MACFG_17,
	MACFG_18,
	MACFG_19,

	MACFG_21,
	MACFG_22,
	MACFG_23,
	MACFG_24,
	MACFG_25,
	MACFG_26,
	MACFG_27,
	MACFG_28,

	MACFG_31,
	MACFG_32,
	MACFG_33,
	MACFG_34,
	MACFG_35,
	MACFG_36,
	MACFG_37,

	MACFG_41,
	MACFG_42,

	MACFG_FF = 0xFF
};

//#define MAC_STYLE_1	1	/* RTL8110S/SB/SC, RTL8111B and RTL8101E */
//#define MAC_STYLE_2	2	/* RTL8111C/CP/D and RTL8102E */

struct rg_softc
{
#if OS_VER<VERSION(6,0)
	struct arpcom		arpcom;			/* interface info */
#else
	struct ifnet		*rg_ifp;
#endif

	bus_space_handle_t	rg_bhandle;		/* bus space handle */
	bus_space_tag_t		rg_btag;			/* bus space tag */
	struct resource		*rg_res;
	struct resource		*rg_irq;
	void			*rg_intrhand;
	struct ifmedia		media;			/* used to instead of MII */

	/* Variable for 8169 family */
	u_int8_t		rg_8169_MacVersion;
	u_int8_t		rg_8169_PhyVersion;

	u_int8_t		rx_fifo_overflow;
	u_int8_t		driver_detach;

	u_int8_t		rg_unit;			/* interface number */
	u_int8_t		rg_type;
	u_int8_t		rg_stats_no_timeout;
	u_int8_t		rg_revid;
	u_int16_t		rg_device_id;

	struct rg_chain_data	rg_cdata;		/* Tx buffer chain, Used only in ~C+ mode */
	struct rg_descriptor	rg_desc;			/* Descriptor, Used only in C+ mode */

	struct callout_handle	rg_stat_ch;
	struct mtx		mtx;
	bus_dma_tag_t		rg_parent_tag;
	device_t		dev;
#if OS_VER>=VERSION(7,0)
	struct task		rg_inttask;
#endif
};

#define RG_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define RG_UNLOCK(_sc)		mtx_unlock(&(_sc)->mtx)
#define RG_LOCK_INIT(_sc,_name)	mtx_init(&(_sc)->mtx,_name,MTX_NETWORK_LOCK,MTX_DEF)
#define RG_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->mtx)
#define RG_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->mtx,MA_OWNED)

/*
 * register space access macros
 */
#if OS_VER>VERSION(5,9)
#define CSR_WRITE_STREAM_4(sc, reg, val)	\
	bus_space_write_stream_4(sc->rg_btag, sc->rg_bhandle, reg, val)
#endif
#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->rg_btag, sc->rg_bhandle, reg, val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->rg_btag, sc->rg_bhandle, reg, val)
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->rg_btag, sc->rg_bhandle, reg, val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->rg_btag, sc->rg_bhandle, reg)
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2(sc->rg_btag, sc->rg_bhandle, reg)
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->rg_btag, sc->rg_bhandle, reg)

#define RG_TIMEOUT		1000

/*
 * General constants that are fun to know.
 *
 * RealTek PCI vendor ID
 */
#define	RT_VENDORID				0x10EC

/*
 * RealTek chip device IDs.
 */
#define RT_DEVICEID_8129			0x8129
#define RT_DEVICEID_8139			0x8139
#define RT_DEVICEID_8169			0x8169		/* For RTL8169 */
#define RT_DEVICEID_8169SC			0x8167		/* For RTL8169SC */
#define RT_DEVICEID_8168			0x8168		/* For RTL8168B */
#define RT_DEVICEID_8136			0x8136		/* For RTL8101E */

/*
 * Accton PCI vendor ID
 */
#define ACCTON_VENDORID				0x1113

/*
 * Accton MPX 5030/5038 device ID.
 */
#define ACCTON_DEVICEID_5030			0x1211

/*
 * Delta Electronics Vendor ID.
 */
#define DELTA_VENDORID				0x1500

/*
 * Delta device IDs.
 */
#define DELTA_DEVICEID_8139			0x1360

/*
 * Addtron vendor ID.
 */
#define ADDTRON_VENDORID			0x4033

/*
 * Addtron device IDs.
 */
#define ADDTRON_DEVICEID_8139			0x1360

/*
 * D-Link vendor ID.
 */
#define DLINK_VENDORID				0x1186

/*
 * D-Link DFE-530TX+ device ID
 */
#define DLINK_DEVICEID_530TXPLUS		0x1300

/*
 * PCI low memory base and low I/O base register, and
 * other PCI registers.
 */

#define RG_PCI_VENDOR_ID	0x00
#define RG_PCI_DEVICE_ID	0x02
#define RG_PCI_COMMAND		0x04
#define RG_PCI_STATUS		0x06
#define RG_PCI_REVISION_ID	0x08	/* 8 bits */
#define RG_PCI_CLASSCODE	0x09
#define RG_PCI_LATENCY_TIMER	0x0D
#define RG_PCI_HEADER_TYPE	0x0E
#define RG_PCI_LOIO		0x10
#define RG_PCI_LOMEM		0x14
#define RG_PCI_BIOSROM		0x30
#define RG_PCI_INTLINE		0x3C
#define RG_PCI_INTPIN		0x3D
#define RG_PCI_MINGNT		0x3E
#define RG_PCI_MINLAT		0x0F
#define RG_PCI_RESETOPT		0x48
#define RG_PCI_EEPROM_DATA	0x4C

#define RG_PCI_CAPID		0x50 /* 8 bits */
#define RG_PCI_NEXTPTR		0x51 /* 8 bits */
#define RG_PCI_PWRMGMTCAP	0x52 /* 16 bits */
#define RG_PCI_PWRMGMTCTRL	0x54 /* 16 bits */

#define RG_PSTATE_MASK		0x0003
#define RG_PSTATE_D0		0x0000
#define RG_PSTATE_D1		0x0002
#define RG_PSTATE_D2		0x0002
#define RG_PSTATE_D3		0x0003
#define RG_PME_EN		0x0010
#define RG_PME_STATUS		0x8000

#ifdef __alpha__
#undef vtophys
#define vtophys(va)     alpha_XXX_dmamap((vm_offset_t)va)
#endif

#ifndef TRUE
#define TRUE		1
#endif
#ifndef FALSE
#define FALSE		0
#endif

#define PHYAR_Flag		0x80000000
#define RG_CPlusMode		0x20		/* In Revision ID */
#define RG_MINI_DESC_SIZE	4

/* interrupt service routine loop time*/
/* the minimum value is 1 */
#define	INTR_MAX_LOOP	1

/*#define RG_DBG*/

#ifdef RG_DBG
	#define DBGPRINT(_unit, _msg)			printf ("re%d: %s\n", _unit,_msg)
	#define DBGPRINT1(_unit, _msg, _para1)	\
		{									\
			char buf[100];					\
			sprintf(buf,_msg,_para1);		\
			printf ("re%d: %s\n", _unit,buf);	\
		}
#else
	#define DBGPRINT(_unit, _msg)
	#define DBGPRINT1(_unit, _msg, _para1)
#endif

#if OS_VER<VERSION(4,9)
	#define IFM_1000_T		IFM_1000_TX
#elif OS_VER<VERSION(6,0)
	#define RG_GET_IFNET(SC)	&SC->arpcom.ac_if
	#define if_drv_flags		if_flags
	#define IFF_DRV_RUNNING		IFF_RUNNING
	#define IFF_DRV_OACTIVE		IFF_OACTIVE
#else
	#define RG_GET_IFNET(SC)	SC->rg_ifp
#endif

