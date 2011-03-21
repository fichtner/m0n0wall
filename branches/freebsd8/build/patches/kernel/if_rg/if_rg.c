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
 * $FreeBSD: src/sys/dev/if_rl.c,v 1.38.2.7 2001/07/19 18:33:07 wpaul Exp $
 */

/*
 * RealTek 8129/8139 PCI NIC driver
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

 /*
 * This driver also support Realtek 8139C+, 8110S/SB/SC, RTL8111B/C/CP/D and RTL8101E/8102E/8103E.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>      /* for DELAY */

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/endian.h>

#include <dev/mii/mii.h>
#include <dev/rg/if_rgreg.h>
#if OS_VER < VERSION(5,3)
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#else
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <sys/module.h>
#endif

#if OS_VER > VERSION(5,9)
#include <sys/cdefs.h>
#include <sys/endian.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#endif


/*
 * Default to using PIO access for this driver. On SMP systems,
 * there appear to be problems with memory mapped mode: it looks like
 * doing too many memory mapped access back to back in rapid succession
 * can hang the bus. I'm inclined to blame this on crummy design/construction
 * on the part of RealTek. Memory mapped mode does appear to work on
 * uniprocessor systems though.
 */
#define RG_USEIOSPACE


#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/sys/dev/re/if_re.c,v 1.38.2.7 2001/07/19 18:33:07 wpaul Exp $";
#endif

#define EE_SET(x)					\
	CSR_WRITE_1(sc, RG_EECMD,			\
		CSR_READ_1(sc, RG_EECMD) | x)

#define EE_CLR(x)					\
	CSR_WRITE_1(sc, RG_EECMD,			\
		CSR_READ_1(sc, RG_EECMD) & ~x)

#ifdef RG_USEIOSPACE
#define RG_RES			SYS_RES_IOPORT
#define RG_RID			RG_PCI_LOIO
#else
#define RG_RES			SYS_RES_MEMORY
#define RG_RID			RG_PCI_LOMEM
#endif

/*
 * Various supported device vendors/types and their names.
 */
static struct rg_type rg_devs[] = {
	{ RT_VENDORID, RT_DEVICEID_8169,
		"Realtek PCI GBE Family Controller" },
	{ RT_VENDORID, RT_DEVICEID_8169SC,
		"Realtek PCI GBE Family Controller" },
	{ RT_VENDORID, RT_DEVICEID_8168,
		"Realtek PCIe GBE Family Controller" },
	{ RT_VENDORID, RT_DEVICEID_8136,
		"Realtek PCIe FE Family Controller" },
	{ 0, 0, NULL }
};

static int	rg_probe			__P((device_t));
static int	rg_attach			__P((device_t));
static int	rg_detach			__P((device_t));
static int	rg_shutdown			__P((device_t));

static void MP_WritePhyUshort			__P((struct rg_softc*, u_int8_t, u_int16_t));
static u_int16_t MP_ReadPhyUshort		__P((struct rg_softc *,u_int8_t));
static void MP_WriteEPhyUshort			__P((struct rg_softc*, u_int8_t, u_int16_t));
static u_int16_t MP_ReadEPhyUshort		__P((struct rg_softc *,u_int8_t));
static u_int8_t MP_ReadEfuse			__P((struct rg_softc *,u_int16_t));

static void rg_8169s_init			__P((struct rg_softc *));
static void rg_init				__P((void *));
static int 	rg_var_init			__P((struct rg_softc *));
static void rg_reset				__P((struct rg_softc *));
static void rg_stop				__P((struct rg_softc *));

static void rg_start				__P((struct ifnet *));
static int rg_encap				__P((struct rg_softc *, struct mbuf * ));
static void WritePacket				__P((struct rg_softc *, caddr_t, int, int, int));
static int CountFreeTxDescNum			__P((struct rg_descriptor));
static int CountMbufNum				__P((struct mbuf *, int *));
static void rg_txeof				__P((struct rg_softc *));

static void	rg_rxeof			__P((struct rg_softc *));

static void rg_intr				__P((void *));
static void rg_setmulti				__P((struct rg_softc *));
static int	rg_ioctl			__P((struct ifnet *, u_long, caddr_t));
static void rg_tick				__P((void *));
#if OS_VER<VERSION(7,0)
static void rg_watchdog				__P((struct ifnet *));
#endif

static int	rg_ifmedia_upd			__P((struct ifnet *));
static void rg_ifmedia_sts			__P((struct ifnet *, struct ifmediareq *));

static void rg_eeprom_ShiftOutBits		__P((struct rg_softc *, int, int));
static u_int16_t rg_eeprom_ShiftInBits		__P((struct rg_softc *));
static void rg_eeprom_EEpromCleanup		__P((struct rg_softc *));
static void rg_eeprom_getword			__P((struct rg_softc *, int, u_int16_t *));
static void rg_read_eeprom			__P((struct rg_softc *, caddr_t, int, int, int));
static void rg_int_task				(void *, int);

static void rg_phy_power_up(device_t dev);
#if 0
static void rg_phy_power_down(device_t dev);
#endif


static device_method_t rg_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rg_probe),
	DEVMETHOD(device_attach,	rg_attach),
	DEVMETHOD(device_detach,	rg_detach),
	DEVMETHOD(device_shutdown,	rg_shutdown),
	{ 0, 0 }
};

static driver_t rg_driver = {
	"rg",
	rg_methods,
	sizeof(struct rg_softc)
};

static devclass_t rg_devclass;

DRIVER_MODULE(if_rg, pci, rg_driver, rg_devclass, 0, 0);

static void rg_phy_power_up(dev)
	device_t		dev;
{
	struct rg_softc		*sc;

	sc = device_get_softc(dev);

	MP_WritePhyUshort(sc, 0x1f, 0x0000);
	MP_WritePhyUshort(sc, 0x00, 0x1000);
	switch (sc->rg_type) {
	case MACFG_4:
	case MACFG_5:
	case MACFG_6:
	case MACFG_21:
	case MACFG_22:
	case MACFG_23:
	case MACFG_24:
	case MACFG_25:
	case MACFG_26:
	case MACFG_27:
	case MACFG_28:
	case MACFG_31:
	case MACFG_32:
	case MACFG_33:
		MP_WritePhyUshort(sc, 0x0e, 0x0000);
		break;
//	case MACFG_11:
//	case MACFG_12:
//	case MACFG_13:
//	case MACFG_14:
//	case MACFG_15:
//	case MACFG_17:
	default:
		break;
	};
}

static void rg_dma_map_buf(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	union RxDesc *rxptr = arg;

	if(error)
	{
		rxptr->so0.Frame_Length = 0;
		*((uint64_t *)&rxptr->so0.RxBuffL) = 0;
		return;
	}
//	rxptr->so0.RxBuffL = segs->ds_addr & 0xFFFFFFFF;
	*((uint64_t *)&rxptr->so0.RxBuffL) = htole64(segs->ds_addr);
}

static void rg_dma_map_rxdesc(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct rg_softc *sc = arg;
	uint32_t ds_addr[2];

	if(error)
		return;

	*((uint64_t *)ds_addr) = htole64(segs->ds_addr);
	CSR_WRITE_4(sc, 0xe4, ds_addr[0] );
	CSR_WRITE_4(sc, 0xe8, ds_addr[1]);
}

static void rg_dma_map_txdesc(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct rg_softc *sc = arg;
	uint32_t ds_addr[2];

	if(error)
		return;

	*((uint64_t *)ds_addr) = htole64(segs->ds_addr);
	CSR_WRITE_4(sc, 0x20, ds_addr[0] );
	CSR_WRITE_4(sc, 0x24, ds_addr[1]);
}

#if 0
static void rg_phy_power_down(dev)
	device_t		dev;
{
	struct rg_softc		*sc;

	sc = device_get_softc(dev);

	MP_WritePhyUshort(sc, 0x1f, 0x0000);
	switch (sc->rg_type) {
	case MACFG_21:
	case MACFG_22:
	case MACFG_23:
	case MACFG_24:
	case MACFG_25:
	case MACFG_26:
	case MACFG_27:
	case MACFG_28:
	case MACFG_31:
	case MACFG_32:
	case MACFG_33:
		MP_WritePhyUshort(sc, 0x0e, 0x0200);
		MP_WritePhyUshort(sc, 0x00, 0x0800);
		break;
//	case MACFG_4:
//	case MACFG_5:
//	case MACFG_6:
//	case MACFG_11:
//	case MACFG_12:
//	case MACFG_13:
//	case MACFG_14:
//	case MACFG_17:
	default:
		MP_WritePhyUshort(sc, 0x00, 0x0800);
		break;
	}
}
#endif

/*
 * Probe for a RealTek 8129/8139 chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int rg_probe(dev)	/* Search for Realtek NIC chip */
	device_t		dev;
{
	struct rg_type		*t; t = rg_devs;
	while(t->rg_name != NULL) {
		if ((pci_get_vendor(dev) == t->rg_vid) &&
		    (pci_get_device(dev) == t->rg_did))
		{
			device_set_desc(dev, t->rg_name);
			return(BUS_PROBE_VENDOR);
		}
		t++;
	}

	return(ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int rg_attach(device_t dev)
{
	/*int			s;*/
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct rg_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		rg_did = 0;
	int			unit, error = 0, rid, i;
//	int			mac_version;
//	int			mode;
//	u_int8_t		data8;

	/*s = splimp();*/

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	bzero(sc, sizeof(struct rg_softc));
	RG_LOCK_INIT(sc,device_get_nameunit(dev));
	sc->dev = dev;

	sc->driver_detach = 0;

	sc->rg_device_id = pci_get_device(dev);
	sc->rg_revid = pci_get_revid(dev);
	pci_enable_busmaster(dev);
	/*
	 * Handle power management nonsense.
	 */
	command = pci_read_config(dev, RG_PCI_CAPID, 4) & 0x000000FF;
	if (command == 0x01) {

		command = pci_read_config(dev, RG_PCI_PWRMGMTCTRL, 4);
		if (command & RG_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_read_config(dev, RG_PCI_LOIO, 4);
			membase = pci_read_config(dev, RG_PCI_LOMEM, 4);
			irq = pci_read_config(dev, RG_PCI_INTLINE, 4);

			/* Reset the power state. */
			printf("re%d: chip is is in D%d power mode "
			"-- setting to D0\n", unit, command & RG_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_write_config(dev, RG_PCI_PWRMGMTCTRL, command, 4);

			/* Restore PCI config data. */
			pci_write_config(dev, RG_PCI_LOIO, iobase, 4);
			pci_write_config(dev, RG_PCI_LOMEM, membase, 4);
			pci_write_config(dev, RG_PCI_INTLINE, irq, 4);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_read_config(dev, PCIR_COMMAND, 4);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, command, 4);
	command = pci_read_config(dev, PCIR_COMMAND, 4);

#ifdef RG_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("re%d: failed to enable I/O ports!\n", unit);
		error = ENXIO;
		goto fail;
	}
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("re%d: failed to enable memory mapping!\n", unit);
		error = ENXIO;
		goto fail;
	}
#endif

	rid = RG_RID;
	sc->rg_res = bus_alloc_resource(dev, RG_RES, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->rg_res == NULL) {
		device_printf(dev,"couldn't map ports/memory\n");
		error = ENXIO;
		goto fail;
	}

	sc->rg_btag = rman_get_bustag(sc->rg_res);
	sc->rg_bhandle = rman_get_bushandle(sc->rg_res);

	rid = 0;
	sc->rg_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->rg_irq == NULL) {
		device_printf(dev,"couldn't map interrupt\n");
		error = ENXIO;
		goto fail;
	}

	callout_handle_init(&sc->rg_stat_ch);

	/*
	 * Reset the adapter. Only take the lock here as it's needed in
	 * order to call rg_reset().
	 */
	RG_LOCK(sc);
	rg_reset(sc);
	RG_UNLOCK(sc);

	/* Get station address from the EEPROM. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		eaddr[i] = CSR_READ_1(sc, RG_IDR0 + i);

	/*
	 * A RealTek chip was detected. Inform the world.
	 */
	device_printf(dev,"version:1.80\n");
	device_printf(dev,"Ethernet address: %6D\n", eaddr, ":");
	printf("\nThis product is covered by one or more of the following patents: US5,307,459, US5,434,872, US5,732,094, US6,570,884, US6,115,776, and US6,327,625.\n");

	sc->rg_unit = unit;

#if OS_VER<VERSION(6,0)
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);
#endif

	/*
	 * Now read the exact device type from the EEPROM to find
	 * out if it's an 8129 or 8139.
	 */
	rg_read_eeprom(sc, (caddr_t)&rg_did, RG_EE_PCI_DID, 1, 0);

	switch(CSR_READ_4(sc, RG_TXCFG) & 0xFCF00000)
	{
		case 0x00800000:
		case 0x04000000:
			sc->rg_type = MACFG_3;
			break;

		case 0x10000000:
			sc->rg_type = MACFG_4;
			break;

		case 0x18000000:
			sc->rg_type = MACFG_5;
			break;

		case 0x98000000:
			sc->rg_type = MACFG_6;
			break;

		case 0x34000000:
		case 0xB4000000:
			sc->rg_type = MACFG_11;
			break;

		case 0x34200000:
		case 0xB4200000:
			sc->rg_type = MACFG_12;
			break;

		case 0x34300000:
		case 0xB4300000:
			sc->rg_type = MACFG_13;
			break;

		case 0x34900000:
		case 0x24900000:
			sc->rg_type = MACFG_14;
			break;

		case 0x34A00000:
		case 0x24A00000:
			sc->rg_type = MACFG_15;
			break;

		case 0x34B00000:
		case 0x24B00000:
			sc->rg_type = MACFG_16;
			break;

		case 0x34C00000:
		case 0x24C00000:
			sc->rg_type = MACFG_17;
			break;

		case 0x34D00000:
		case 0x24D00000:
			sc->rg_type = MACFG_18;
			break;

		case 0x34E00000:
		case 0x24E00000:
			sc->rg_type = MACFG_19;
			break;

		case 0x30000000:
			sc->rg_type = MACFG_21;
			break;

		case 0x38000000:
			sc->rg_type = MACFG_22;
			break;

		case 0x38500000:
		case 0xB8500000:
		case 0x38700000:
		case 0xB8700000:
			sc->rg_type = MACFG_23;
			break;

		case 0x3C000000:
			sc->rg_type = MACFG_24;
			break;

		case 0x3C200000:
			sc->rg_type = MACFG_25;
			break;

		case 0x3C400000:
			sc->rg_type = MACFG_26;
			break;

		case 0x3C900000:
			sc->rg_type = MACFG_27;
			break;

		case 0x3CB00000:
			sc->rg_type = MACFG_28;
			break;

		case 0x28100000:
			sc->rg_type = MACFG_31;
			break;

		case 0x28200000:
			sc->rg_type = MACFG_32;
			break;

		case 0x28300000:
			sc->rg_type = MACFG_33;
			break;

		case 0x2C100000:
			sc->rg_type = MACFG_36;
			break;

		case 0x2C200000:
			sc->rg_type = MACFG_37;
			break;

		case 0x24000000:
			sc->rg_type = MACFG_41;
			break;

		case 0x40900000:
			sc->rg_type = MACFG_42;
			break;

		default:
	printf("%x\t",(CSR_READ_4(sc, RG_TXCFG) & 0xFCF00000));
			device_printf(dev,"unknown device\n");
			sc->rg_type = MACFG_FF;
			error = ENXIO;
			goto fail;
	}

	if(sc->rg_type==MACFG_3)
	{	/* Change PCI Latency time*/
		pci_write_config(dev, RG_PCI_LATENCY_TIMER, 0x40, 1);
	}

	error = bus_dma_tag_create(
#if OS_VER<VERSION(7,0)
		NULL,
#else
		bus_get_dma_tag(dev),		/* parent */
#endif
		1, 0,		/* alignment, boundary */
		BUS_SPACE_MAXADDR,		/* lowaddr */
		BUS_SPACE_MAXADDR,		/* highaddr */
		NULL, NULL,			/* filter, filterarg */
		BUS_SPACE_MAXSIZE_32BIT,	/* maxsize */
		0,				/* nsegments */
		BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
		0,				/* flags */
		NULL, NULL,			/* lockfunc, lockarg */
		&sc->rg_parent_tag);

	i = roundup2(sizeof(union RxDesc)*RG_RX_BUF_NUM, RG_DESC_ALIGN);
	error = bus_dma_tag_create(
		sc->rg_parent_tag,
		RG_DESC_ALIGN, 0,		/* alignment, boundary */
		BUS_SPACE_MAXADDR,		/* lowaddr */
		BUS_SPACE_MAXADDR,		/* highaddr */
		NULL, NULL,			/* filter, filterarg */
		i,				/* maxsize */
		1,				/* nsegments */
		i,				/* maxsegsize */
		0,				/* flags */
		NULL, NULL,			/* lockfunc, lockarg */
		&sc->rg_desc.rx_desc_tag);
	if (error)
	{
		device_printf(dev,"bus_dma_tag_create fail\n");
		goto fail;
	}

	error = bus_dmamem_alloc(sc->rg_desc.rx_desc_tag,
			(void**) &sc->rg_desc.rx_desc,
			BUS_DMA_WAITOK|BUS_DMA_COHERENT|BUS_DMA_ZERO,
			&sc->rg_desc.rx_desc_dmamap);
	if (error)
	{
		device_printf(dev,"bus_dmamem_alloc fail\n");
		goto fail;
	}

	i = roundup2(sizeof(union TxDesc)*RG_TX_BUF_NUM, RG_DESC_ALIGN);
	error = bus_dma_tag_create(
			sc->rg_parent_tag,
			RG_DESC_ALIGN, 0,		/* alignment, boundary */
			BUS_SPACE_MAXADDR,		/* lowaddr */
			BUS_SPACE_MAXADDR,		/* highaddr */
			NULL, NULL,			/* filter, filterarg */
			i,				/* maxsize */
			1,				/* nsegments */
			i,				/* maxsegsize */
			0,				/* flags */
			NULL, NULL,			/* lockfunc, lockarg */
			&sc->rg_desc.tx_desc_tag);
	if (error)
	{
		device_printf(dev,"bus_dma_tag_create fail\n");
		goto fail;
	}

	error = bus_dmamem_alloc(sc->rg_desc.tx_desc_tag,
			(void**) &sc->rg_desc.tx_desc,
			BUS_DMA_WAITOK|BUS_DMA_COHERENT|BUS_DMA_ZERO,
			&sc->rg_desc.tx_desc_dmamap);

	if (error)
	{
		device_printf(dev,"bus_dmamem_alloc fail\n");
		goto fail;
	}

	error = bus_dma_tag_create(sc->rg_parent_tag, 1, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, MCLBYTES * RG_NTXSEGS, RG_NTXSEGS, 4096, 0,
	    NULL, NULL, &sc->rg_desc.rg_tx_mtag);

	if (error)
	{
		device_printf(dev,"rg_tx_mtag fail\n");
		goto fail;
	}

	error = bus_dma_tag_create(
			sc->rg_parent_tag,
			RG_DESC_ALIGN, 0,		/* alignment, boundary */
			BUS_SPACE_MAXADDR,		/* lowaddr */
			BUS_SPACE_MAXADDR,		/* highaddr */
			NULL, NULL,			/* filter, filterarg */
			MCLBYTES, 1,			/* maxsize,nsegments */
			MCLBYTES,			/* maxsegsize */
			0,				/* flags */
			NULL, NULL,			/* lockfunc, lockarg */
			&sc->rg_desc.rg_rx_mtag);
	if (error)
	{
		device_printf(dev,"rg_rx_mtag fail\n");
		goto fail;
	}

	for(i=0;i<RG_RX_BUF_NUM;i++)
	{
		sc->rg_desc.rx_buf[i] = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if(!sc->rg_desc.rx_buf[i])
		{
			device_printf(dev, "m_getcl fail!!!\n");
			error = ENXIO;
			goto fail;
		}

		error = bus_dmamap_create(sc->rg_desc.rg_rx_mtag, BUS_DMA_NOWAIT, &sc->rg_desc.rg_rx_dmamap[i]);
		if(error)
		{
			device_printf(dev, "bus_dmamap_create fail!!!\n");
			goto fail;
		}
	}

	for(i=0;i<RG_TX_BUF_NUM;i++)
	{
		error = bus_dmamap_create(sc->rg_desc.rg_tx_mtag, BUS_DMA_NOWAIT, &sc->rg_desc.rg_tx_dmamap[i]);
		if(error)
		{
			device_printf(dev, "bus_dmamap_create fail!!!\n");
			goto fail;
		}
	}

	sc->rg_8169_MacVersion=(CSR_READ_4(sc, RG_TXCFG)&0x7c800000)>>25;		/* Get bit 26~30 	*/
	sc->rg_8169_MacVersion|=((CSR_READ_4(sc, RG_TXCFG)&0x00800000)!=0 ? 1:0);	/* Get bit 23 		*/
	DBGPRINT1(sc->rg_unit,"8169 Mac Version %d",sc->rg_8169_MacVersion);

	/* Rtl8169s single chip detected */
	if(sc->rg_8169_MacVersion > 0) {
		sc->rg_8169_PhyVersion=(MP_ReadPhyUshort(sc, 0x03)&0x000f);
		DBGPRINT1(sc->rg_unit,"8169 Phy Version %d",sc->rg_8169_PhyVersion);

		if(sc->rg_8169_MacVersion == 1) {
			CSR_WRITE_1(sc, 0x82, 0x01);
			MP_WritePhyUshort(sc, 0x0b, 0x00);
		}

		rg_phy_power_up(dev);
		rg_8169s_init(sc);
	}

#if OS_VER<VERSION(6,0)
	ifp = &sc->arpcom.ac_if;
#else
	ifp = sc->rg_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		error = ENOSPC;
		goto fail;
	}
#endif
	ifp->if_softc = sc;
#if OS_VER < VERSION(5,3)
	ifp->if_unit = unit;
	ifp->if_name = "re";
#else
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
#endif
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = rg_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = rg_start;
#if OS_VER<VERSION(7,0)
	ifp->if_watchdog = rg_watchdog;
#endif
	ifp->if_init = rg_init;
	switch(sc->rg_device_id)
	{
		case RT_DEVICEID_8169:
		case RT_DEVICEID_8169SC:
		case RT_DEVICEID_8168:
			ifp->if_baudrate = 1000000000;
			break;

		default:
			ifp->if_baudrate = 100000000;
			break;
	}
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

#if OS_VER>=VERSION(7,0)
	TASK_INIT(&sc->rg_inttask, 0, rg_int_task, sc);
#endif

	/*
	 * Call MI attach routine.
	 */
/*#if OS_VER < VERSION(5,1)*/
#if OS_VER < VERSION(4,9)
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
#else
	ether_ifattach(ifp, eaddr);
#endif


#if OS_VER<VERSION(7,0)
	error = bus_setup_intr(dev, sc->rg_irq, INTR_TYPE_NET,
	    rg_intr, sc, &sc->rg_intrhand);
#else
	error = bus_setup_intr(dev, sc->rg_irq, INTR_TYPE_NET|INTR_MPSAFE,
			NULL, rg_intr, sc, &sc->rg_intrhand);
#endif

	if (error)
	{
#if OS_VER < VERSION(4,9)
		ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);
#else
		ether_ifdetach(ifp);
#endif
		device_printf(dev,"couldn't set up irq\n");
		goto fail;
	}

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&sc->media, IFM_IMASK, rg_ifmedia_upd, rg_ifmedia_sts);
	ifmedia_add(&sc->media, IFM_ETHER | IFM_10_T, 0, NULL);
	ifmedia_add(&sc->media, IFM_ETHER | IFM_10_T | IFM_FDX, 0, NULL);
	ifmedia_add(&sc->media, IFM_ETHER | IFM_100_TX, 0, NULL);
	ifmedia_add(&sc->media, IFM_ETHER | IFM_100_TX | IFM_FDX, 0, NULL);
	switch(sc->rg_device_id)
	{
		case RT_DEVICEID_8169:
		case RT_DEVICEID_8169SC:
		case RT_DEVICEID_8168:
			ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_T | IFM_FDX, 0, NULL);
			ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_T, 0, NULL);
			break;

		default:
			break;
	}
	ifmedia_add(&sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->media, IFM_ETHER | IFM_AUTO);
	sc->media.ifm_media = IFM_ETHER | IFM_AUTO;
	rg_ifmedia_upd(ifp);

fail:
	if (error)
		rg_detach(dev);

	return(error);
}

static int rg_detach(device_t dev)
{
	struct rg_softc		*sc;
	struct ifnet		*ifp;
	/*int			s;*/
	int			i;

	/*s = splimp();*/

	sc = device_get_softc(dev);

	if (sc->rg_intrhand)
		bus_teardown_intr(dev, sc->rg_irq, sc->rg_intrhand);
	if (sc->rg_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->rg_irq);
	if (sc->rg_res)
		bus_release_resource(dev, RG_RES, RG_RID, sc->rg_res);

	ifp = RG_GET_IFNET(sc);

	/* These should only be active if attach succeeded */
	if (device_is_attached(dev))
	{
		RG_LOCK(sc);
		rg_stop(sc);
		RG_UNLOCK(sc);
#if OS_VER>=VERSION(7,0)
		taskqueue_drain(taskqueue_fast, &sc->rg_inttask);
#endif
#if OS_VER < VERSION(4,9)
		ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);
#else
		ether_ifdetach(ifp);
#endif
	}
	sc->driver_detach = 1;

#if OS_VER>=VERSION(6,0)
	if (ifp)
		if_free(ifp);
#endif
	bus_generic_detach(dev);

	if(sc->rg_desc.rg_rx_mtag)
	{
		for(i=0;i<RG_RX_BUF_NUM;i++)
		{
			if(sc->rg_desc.rx_buf[i]!=NULL)
			{
				bus_dmamap_sync(sc->rg_desc.rg_rx_mtag,
					sc->rg_desc.rg_rx_dmamap[i],
					BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->rg_desc.rg_rx_mtag,
					sc->rg_desc.rg_rx_dmamap[i]);
				bus_dmamap_destroy(sc->rg_desc.rg_rx_mtag,
					sc->rg_desc.rg_rx_dmamap[i]);
				m_freem(sc->rg_desc.rx_buf[i]);
			}
		}
		bus_dma_tag_destroy(sc->rg_desc.rg_rx_mtag);
	}

	if(sc->rg_desc.rg_tx_mtag)
	{
		for(i=0;i<RG_TX_BUF_NUM;i++)
		{
			bus_dmamap_destroy(sc->rg_desc.rg_tx_mtag,
				sc->rg_desc.rg_tx_dmamap[i]);
		}
		bus_dma_tag_destroy(sc->rg_desc.rg_tx_mtag);
	}

	if(sc->rg_desc.rx_desc_tag)
	{
		bus_dmamap_sync(sc->rg_desc.rx_desc_tag,
			sc->rg_desc.rx_desc_dmamap,
			BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE );
		bus_dmamap_unload(sc->rg_desc.rx_desc_tag,
			sc->rg_desc.rx_desc_dmamap);
		bus_dmamem_free(sc->rg_desc.rx_desc_tag,
			sc->rg_desc.rx_desc,
			sc->rg_desc.rx_desc_dmamap);
		bus_dma_tag_destroy(sc->rg_desc.rx_desc_tag);
	}

	if(sc->rg_desc.tx_desc_tag)
	{
		bus_dmamap_sync(sc->rg_desc.tx_desc_tag,
			sc->rg_desc.tx_desc_dmamap,
			BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE );
		bus_dmamap_unload(sc->rg_desc.tx_desc_tag,
			sc->rg_desc.tx_desc_dmamap);
		bus_dmamem_free(sc->rg_desc.tx_desc_tag,
			sc->rg_desc.tx_desc,
			sc->rg_desc.tx_desc_dmamap);
		bus_dma_tag_destroy(sc->rg_desc.tx_desc_tag);
	}

	if (sc->rg_parent_tag)
	{
		bus_dma_tag_destroy(sc->rg_parent_tag);
	}

	/*splx(s);*/
	RG_LOCK_DESTROY(sc);

	return(0);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static int rg_shutdown(dev)	/* The same with rg_stop(sc) */
	device_t		dev;
{
	struct rg_softc		*sc;

	sc = device_get_softc(dev);

	RG_LOCK(sc);
	rg_stop(sc);
	RG_UNLOCK(sc);

	return 0;
}

static void rg_init(void *xsc)		/* Software & Hardware Initialize */
{
	struct rg_softc		*sc = xsc;
	struct ifnet		*ifp;
#if OS_VER<VERSION(6,0)
	int			i;
#endif
	u_int8_t		data8;
	u_int16_t		data16 = 0;
	u_int32_t		rxcfg = 0;
	u_int32_t	macver;


	ifp = RG_GET_IFNET(sc);
	/*s = splimp();*/
	RG_LOCK(sc);

/*	RG_LOCK_ASSERT(sc);*/

	/*mii = device_get_softc(sc->rg_miibus);*/

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	rg_stop(sc);

	/* Init our MAC address */
#if OS_VER<VERSION(6,0)
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		CSR_WRITE_1(sc, RG_IDR0 + i, sc->arpcom.ac_enaddr[i]);
	}
#elif OS_VER<VERSION(7,0)
	CSR_WRITE_1(sc, RG_EECMD, RG_EEMODE_WRITECFG);
	CSR_WRITE_STREAM_4(sc, RG_IDR0,
	    *(u_int32_t *)(&IFP2ENADDR(sc->rg_ifp)[0]));
	CSR_WRITE_STREAM_4(sc, RG_IDR4,
	    *(u_int32_t *)(&IFP2ENADDR(sc->rg_ifp)[4]));
	CSR_WRITE_1(sc, RG_EECMD, RG_EEMODE_OFF);
#else
	CSR_WRITE_1(sc, RG_EECMD, RG_EEMODE_WRITECFG);
	CSR_WRITE_STREAM_4(sc, RG_IDR0,
	    *(u_int32_t *)(&IF_LLADDR(sc->rg_ifp)[0]));
	CSR_WRITE_STREAM_4(sc, RG_IDR4,
	    *(u_int32_t *)(&IF_LLADDR(sc->rg_ifp)[4]));
	CSR_WRITE_1(sc, RG_EECMD, RG_EEMODE_OFF);
#endif

	/* Init descriptors. */
	rg_var_init(sc);

	/*disable Link Down Power Saving(non-LDPS)*/
	/*CSR_WRITE_1(sc, RG_LDPS, 0x05);*/
	/*ldps=CSR_READ_1(sc, RG_LDPS);*/

	CSR_WRITE_2(sc,RG_CPCR,0x2082);
	CSR_WRITE_2(sc,RG_IM,0x5151);

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_2(sc, RG_IMR, RG_INTRS);


	/* Start RX/TX process. */
	CSR_WRITE_4(sc, RG_MISSEDPKT, 0);

	/* Enable receiver and transmitter. */
/*	CSR_WRITE_1(sc, RG_COMMAND, RG_CMD_TX_ENB|RG_CMD_RX_ENB);*/

	macver = CSR_READ_4(sc, RG_TXCFG) & 0xFC800000;
	CSR_WRITE_1(sc, RG_EECMD, RG_EEMODE_WRITECFG);
	if(macver==0x00800000)
	{
		CSR_WRITE_2(sc, RG_CPlusCmd, 0x0003|((sc->rg_type==MACFG_3 && sc->rg_8169_MacVersion==1) ? 0x4008:0));
		CSR_WRITE_4(sc, RG_RXCFG, 0xFF00);
	}
	else if(macver==0x04000000)
	{
		CSR_WRITE_2(sc, RG_CPlusCmd, 0x0003|((sc->rg_type==MACFG_3 && sc->rg_8169_MacVersion==1) ? 0x4008:0));
		CSR_WRITE_4(sc, RG_RXCFG, 0xFF00);
	}
	else if(macver==0x10000000)
	{
		CSR_WRITE_2(sc, RG_CPlusCmd, 0x0003|((sc->rg_type==MACFG_3 && sc->rg_8169_MacVersion==1) ? 0x4008:0));
		CSR_WRITE_4(sc, RG_RXCFG, 0xFF00);
	}
	else if(macver==0x18000000)
	{
		if(CSR_READ_1(sc, RG_CFG2)&1)
		{
			CSR_WRITE_4(sc, 0x7C, 0x000FFFFF);
		}
		else
		{
			CSR_WRITE_4(sc, 0x7C, 0x000FFF00);
		}
		CSR_WRITE_2(sc, RG_CPlusCmd,0x0008);
		CSR_WRITE_2(sc, 0xe2,0x0000);
		CSR_WRITE_4(sc, RG_RXCFG, 0xFF00);
	}
	else if(macver==0x98000000)
	{
		if(CSR_READ_1(sc, RG_CFG2)&1)
		{
			CSR_WRITE_4(sc, 0x7C, 0x003FFFFF);
		}
		else
		{
			CSR_WRITE_4(sc, 0x7C, 0x003FFF00);
		}
		CSR_WRITE_2(sc, RG_CPlusCmd,0x0008);
		CSR_WRITE_2(sc, 0xe2,0x0000);
		CSR_WRITE_4(sc, RG_RXCFG, 0xFF00);
	}
	else if(macver==0x30000000)
	{
		CSR_WRITE_2 (sc, RG_CPlusCmd,0x2000);
		CSR_WRITE_4(sc, RG_RXCFG, 0xE700);
	}
	else if(macver==0x38000000)
	{
		CSR_WRITE_2 (sc, RG_CPlusCmd,0x2000);
		CSR_WRITE_4(sc, RG_RXCFG, 0xE700);
	}
	else if(macver==0x34000000 || macver==0xB4000000)
	{
		CSR_WRITE_2 (sc, RG_CPlusCmd,0x2000);
		CSR_WRITE_4(sc, RG_RXCFG, 0xE700);
	}
	else if(macver==0x34800000 || macver==0x24800000)
	{
		if(pci_read_config(sc->dev, 0x81, 1)==1)
		{
			CSR_WRITE_1(sc, RG_DBG_reg, 0x98);
			CSR_WRITE_1(sc, RG_CFG2, CSR_READ_1(sc, RG_CFG2) | 0x80);
			CSR_WRITE_1(sc, RG_CFG4, CSR_READ_1(sc, RG_CFG4) | 0x04);
			pci_write_config(sc->dev, 0x81, 1, 1);
		}

		data8 = pci_read_config(sc->dev, 0x79, 1);
		data8 &= ~0x70;
		data8 |= 0x50;
		pci_write_config(sc->dev, 0x79, data8, 1);

		/*set configuration space offset 0x70f to 0x3f*/
		CSR_WRITE_4(sc, RG_CSIDR, 0x3F000000);
		CSR_WRITE_4(sc, RG_CSIAR, 0x8000870C);

		CSR_WRITE_2(sc, RG_RxMaxSize, 0x05EF);
		CSR_WRITE_1(sc, RG_CFG3, CSR_READ_1(sc, RG_CFG3) & ~(1 << 0));

		CSR_WRITE_2 (sc, RG_CPlusCmd,0x2020);
		if(sc->rg_type==MACFG_14)
		{
			MP_WriteEPhyUshort(sc, 0x03, 0xC2F9);
		}
		else if(sc->rg_type==MACFG_15)
		{
			MP_WriteEPhyUshort(sc, 0x03, 0x07D9);
		}
		else if(sc->rg_type==MACFG_17)
		{
			data16 = MP_ReadEPhyUshort(sc, 0x01);
			data16 |= 0x0180;
			MP_WriteEPhyUshort(sc, 0x01, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x03) & ~0xC722;
			data16 |= 0x0700;
			MP_WriteEPhyUshort(sc, 0x03, data16);

			MP_WriteEPhyUshort(sc, 0x06, 0xAF35);
		}
		else if(sc->rg_type==MACFG_18)
		{
			CSR_WRITE_1(sc, 0xF5, CSR_READ_1(sc, 0xF5)|0x04);
			MP_WriteEPhyUshort(sc, 0x19, 0xEC90);
			MP_WriteEPhyUshort(sc, 0x01, 0x6FE5);
			MP_WriteEPhyUshort(sc, 0x03, 0x05D9);
			MP_WriteEPhyUshort(sc, 0x06, 0xAF35);
		}
		else if(sc->rg_type==MACFG_19)
		{
			CSR_WRITE_1(sc, 0xF4, CSR_READ_1(sc, 0xF4)|0x08);
			CSR_WRITE_1(sc, 0xF5, CSR_READ_1(sc, 0xF5)|0x04);
			MP_WriteEPhyUshort(sc, 0x19, 0xEC90);
			MP_WriteEPhyUshort(sc, 0x01, 0x6FE5);
			MP_WriteEPhyUshort(sc, 0x03, 0x05D9);
			MP_WriteEPhyUshort(sc, 0x06, 0xAF35);
		}
	}
	else if(macver==0x3C000000)
	{
		data8 = pci_read_config(sc->dev, 0x79, 1);
		data8 &= ~0x70;
		data8 |= 0x50;
		pci_write_config(sc->dev, 0x79, data8, 1);

		/*set configuration space offset 0x70f to 0x3f*/
		CSR_WRITE_4(sc, RG_CSIDR, 0x27000000);
		CSR_WRITE_4(sc, RG_CSIAR, 0x8000870C);

		CSR_WRITE_1(sc, RG_CFG1, CSR_READ_1(sc, RG_CFG1)|0x10);
		CSR_WRITE_2(sc, RG_RxMaxSize, 0x05F3);
		CSR_WRITE_1(sc, RG_CFG3, CSR_READ_1(sc, RG_CFG3) & ~(1 << 0));

		CSR_WRITE_2 (sc, RG_CPlusCmd,0x2020);
		CSR_WRITE_4(sc, RG_RXCFG, 0xC700);
		if(sc->rg_type==MACFG_24)
		{
			/*set mac register offset 0xd1 to 0xf8*/
			CSR_WRITE_1(sc, RG_DBG_reg, 0xF8);

			data16 = MP_ReadEPhyUshort(sc, 0x02) & ~0x1800;
			data16 |= 0x1000;
			MP_WriteEPhyUshort(sc, 0x02, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x03) | 0x0002;
			MP_WriteEPhyUshort(sc, 0x03, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x06) & ~0x0080;
			MP_WriteEPhyUshort(sc, 0x06, data16);
		}
		else if(sc->rg_type==MACFG_25)
		{
			data16 = MP_ReadEPhyUshort(sc, 0x01) | 0x0001;
			MP_WriteEPhyUshort(sc, 0x01, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x03) & ~0x0620;
			data16 |= 0x0220;
			MP_WriteEPhyUshort(sc, 0x03, data16);
		}
	}
	else if(macver==0x3C800000)
	{
		data8 = pci_read_config(sc->dev, 0x79, 1);
		data8 &= ~0x70;
		data8 |= 0x50;
		pci_write_config(sc->dev, 0x79, data8, 1);

		/*set configuration space offset 0x70f to 0x3f*/
		CSR_WRITE_4(sc, RG_CSIDR, 0x27000000);
		CSR_WRITE_4(sc, RG_CSIAR, 0x8000870C);
		CSR_WRITE_2 (sc, RG_CPlusCmd,0x2020);

		CSR_WRITE_2(sc, RG_RxMaxSize, 0x05F3);
		CSR_WRITE_4(sc, RG_RXCFG, 0xC700);
	}
	else if(macver==0x28000000)
	{
		data8 = pci_read_config(sc->dev, 0x79, 1);
		data8 &= ~0x70;
		data8 |= 0x50;
		pci_write_config(sc->dev, 0x79, data8, 1);

		/*set configuration space offset 0x70f to 0x3f*/
		CSR_WRITE_4(sc, RG_CSIDR, 0x27000000);
		CSR_WRITE_4(sc, RG_CSIAR, 0x8000870C);

		CSR_WRITE_1(sc, RG_CFG1, CSR_READ_1(sc, RG_CFG1)|0x10);
		CSR_WRITE_2(sc, RG_RxMaxSize, 0x05F3);
		CSR_WRITE_1(sc, RG_CFG3, CSR_READ_1(sc, RG_CFG3) & ~0x11);
		CSR_WRITE_1(sc, RG_DBG_reg, CSR_READ_1(sc, RG_DBG_reg)|0x82);

		CSR_WRITE_2 (sc, RG_CPlusCmd,0x2020);
		CSR_WRITE_4(sc, RG_RXCFG, 0x8700);
		if(sc->rg_type==MACFG_31)
		{
			MP_WriteEPhyUshort(sc, 0x01, 0x7C7D);
			MP_WriteEPhyUshort(sc, 0x02, 0x091F);
			MP_WriteEPhyUshort(sc, 0x06, 0xB271);
			MP_WriteEPhyUshort(sc, 0x07, 0xCE00);
		}
		else if(sc->rg_type==MACFG_32)
		{
			MP_WriteEPhyUshort(sc, 0x01, 0x7C7D);
			MP_WriteEPhyUshort(sc, 0x02, 0x091F);
			MP_WriteEPhyUshort(sc, 0x03, 0xC5BA);
			MP_WriteEPhyUshort(sc, 0x06, 0xB279);
			MP_WriteEPhyUshort(sc, 0x07, 0xAF00);
			MP_WriteEPhyUshort(sc, 0x1E, 0xB8EB);
		}
		else if(sc->rg_type==MACFG_33)
		{
			MP_WriteEPhyUshort(sc, 0x01, 0x6C7F);
			MP_WriteEPhyUshort(sc, 0x02, 0x011F);
			MP_WriteEPhyUshort(sc, 0x03, 0xC1B2);
			MP_WriteEPhyUshort(sc, 0x1A, 0x0546);
			MP_WriteEPhyUshort(sc, 0x1C, 0x80C4);
			MP_WriteEPhyUshort(sc, 0x1D, 0x78E4);
			MP_WriteEPhyUshort(sc, 0x0A, 0x8100);
		}
	}
	else if(macver==0x2C000000)
	{
		data8 = pci_read_config(sc->dev, 0x79, 1);
		data8 &= ~0x70;
		data8 |= 0x50;
		pci_write_config(sc->dev, 0x79, data8, 1);

		CSR_WRITE_4(sc, RG_CSIDR, 0x27000000);
		CSR_WRITE_4(sc, RG_CSIAR, 0x8000870C);

		CSR_WRITE_1 (sc, RG_MTPS, 0x0C);
		CSR_WRITE_2(sc, RG_RxMaxSize, 0x05F3);

		CSR_WRITE_1(sc, 0xF3, CSR_READ_1(sc, 0xF3)|0x20);
		CSR_WRITE_1(sc, 0xF3, CSR_READ_1(sc, 0xF3)&~0x20);
		CSR_WRITE_1(sc, 0xD0, CSR_READ_1(sc, 0xD0)|0xC0);
		CSR_WRITE_1(sc, 0xF1, CSR_READ_1(sc, 0xF1)|0xF3);
		CSR_WRITE_1(sc, RG_CFG5, (CSR_READ_1(sc, RG_CFG5)&~0x08)|0x01);
		CSR_WRITE_1(sc, RG_CFG2, CSR_READ_1(sc, RG_CFG2)|0x80);
		CSR_WRITE_1(sc, RG_CFG3, CSR_READ_1(sc, RG_CFG3) & ~(1 << 0));
		CSR_WRITE_4(sc, RG_RXCFG, 0x8700);

		if(sc->rg_type==MACFG_36 || sc->rg_type==MACFG_37)
		{
			/* set EPHY registers */
			data16 = MP_ReadEPhyUshort(sc, 0x00) & ~0x0200;
			data16 |= 0x0100;
			MP_WriteEPhyUshort(sc, 0x00, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x00);
			data16 |= 0x0004;
			MP_WriteEPhyUshort(sc, 0x00, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x06) & ~0x0002;
			data16 |= 0x0001;
			MP_WriteEPhyUshort(sc, 0x06, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x06);
			data16 |= 0x0030;
			MP_WriteEPhyUshort(sc, 0x06, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x07);
			data16 |= 0x2000;
			MP_WriteEPhyUshort(sc, 0x07, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x00);
			data16 |= 0x0020;
			MP_WriteEPhyUshort(sc, 0x00, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x03) & ~0x5800;
			data16 |= 0x2000;
			MP_WriteEPhyUshort(sc, 0x03, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x03);
			data16 |= 0x0001;
			MP_WriteEPhyUshort(sc, 0x03, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x01) & ~0x0800;
			data16 |= 0x1000;
			MP_WriteEPhyUshort(sc, 0x01, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x07);
			data16 |= 0x4000;
			MP_WriteEPhyUshort(sc, 0x07, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x1E);
			data16 |= 0x2000;
			MP_WriteEPhyUshort(sc, 0x1E, data16);

			MP_WriteEPhyUshort(sc, 0x19, 0xFE6C);

			data16 = MP_ReadEPhyUshort(sc, 0x0A);
			data16 |= 0x0040;
			MP_WriteEPhyUshort(sc, 0x0A, data16);
		}
	}
	else if(macver==0x24000000)
	{
		if(pci_read_config(sc->dev, 0x81, 1)==1)
		{
			CSR_WRITE_1(sc, RG_DBG_reg, 0x98);
			CSR_WRITE_1(sc, RG_CFG2, CSR_READ_1(sc, RG_CFG2) | 0x80);
			CSR_WRITE_1(sc, RG_CFG4, CSR_READ_1(sc, RG_CFG4) | 0x04);
			pci_write_config(sc->dev, 0x81, 1, 1);
		}
		data8 = pci_read_config(sc->dev, 0x79, 1);
		data8 &= ~0x70;
		data8 |= 0x50;
		pci_write_config(sc->dev, 0x79, data8, 1);

		/*set configuration space offset 0x70f to 0x3f*/
		CSR_WRITE_4(sc, RG_CSIDR, 0x3F000000);
		CSR_WRITE_4(sc, RG_CSIAR, 0x8000870C);

		CSR_WRITE_2(sc, RG_RxMaxSize, 0x05EF);

		CSR_WRITE_2 (sc, RG_CPlusCmd,0x2020);

		MP_WriteEPhyUshort(sc, 0x06, 0xAF25);
		MP_WriteEPhyUshort(sc, 0x07, 0x8E68);
	}
	else if(macver==0x40800000)
	{
		CSR_WRITE_2(sc, RG_RxMaxSize, 0x05F3);
		if(pci_read_config(sc->dev, 0x80, 1)&0x03)
		{
			CSR_WRITE_1(sc, RG_CFG5, CSR_READ_1(sc, RG_CFG5) | 1);
			CSR_WRITE_1(sc, 0xF1, CSR_READ_1(sc, 0xF1) | 0x80);
			CSR_WRITE_1(sc, 0xF2, CSR_READ_1(sc, 0xF2) | 0x80);
			CSR_WRITE_1(sc, RG_CFG2, CSR_READ_1(sc, RG_CFG2) | 0x80);
		}
		CSR_WRITE_1(sc, 0xF1, CSR_READ_1(sc, 0xF1) | 0x28);
		CSR_WRITE_1(sc, 0xF2, CSR_READ_1(sc, 0xF2) & ~0x01);
		CSR_WRITE_1(sc, 0xD3, CSR_READ_1(sc, 0xD3) | 0x0C);
		CSR_WRITE_1(sc, 0xD0, CSR_READ_1(sc, 0xD0) | 0x40);
		CSR_WRITE_2(sc, 0xE0, CSR_READ_2(sc, 0xE0) & ~0xDF9C);
		if(sc->rg_type==MACFG_42)
		{
			/* set EPHY registers */
			data16 = MP_ReadEPhyUshort(sc, 0x07);
			data16 |= 0x4000;
			MP_WriteEPhyUshort(sc, 0x07, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x19);
			data16 |= 0x0200;
			MP_WriteEPhyUshort(sc, 0x19, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x19);
			data16 |= 0x0020;
			MP_WriteEPhyUshort(sc, 0x19, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x1E);
			data16 |= 0x2000;
			MP_WriteEPhyUshort(sc, 0x1E, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x03);
			data16 |= 0x0001;
			MP_WriteEPhyUshort(sc, 0x03, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x19);
			data16 |= 0x0100;
			MP_WriteEPhyUshort(sc, 0x19, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x19);
			data16 |= 0x0004;
			MP_WriteEPhyUshort(sc, 0x19, data16);

			data16 = MP_ReadEPhyUshort(sc, 0x0A);
			data16 |= 0x0020;
			MP_WriteEPhyUshort(sc, 0x0A, data16);
		}
	}

	CSR_WRITE_1(sc, RG_EECMD, RG_EEMODE_OFF);
	CSR_WRITE_1(sc, 0xec, 0x3f);

	/* Enable transmit and receive.*/
	CSR_WRITE_1(sc, RG_COMMAND, RG_CMD_TX_ENB|RG_CMD_RX_ENB);

	/* Set the initial TX configuration.*/
	CSR_WRITE_4(sc, RG_TXCFG, RG_TXCFG_CONFIG);

	/* Set the initial RX configuration.*/
	/* Set the individual bit to receive frames for this host only. */
	rxcfg = CSR_READ_4(sc, RG_RXCFG);
	rxcfg |= RG_RXCFG_RX_INDIV;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		rxcfg |= RG_RXCFG_RX_ALLPHYS;
	} else {
		rxcfg &= ~RG_RXCFG_RX_ALLPHYS;
	}

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		rxcfg |= RG_RXCFG_RX_BROAD;
	} else {
		rxcfg &= ~RG_RXCFG_RX_BROAD;
	}

	CSR_WRITE_4(sc, RG_RXCFG, rxcfg);

	/*
	 * Program the multicast filter, if necessary.
	 */
	rg_setmulti(sc);

	if ((sc->rg_type == MACFG_3) || (sc->rg_type == MACFG_4)) {
		CSR_WRITE_2(sc, RG_RxMaxSize, 0x800);	/* RMS */
	}

	CSR_WRITE_1(sc, RG_CFG1, RG_CFG1_DRVLOAD|RG_CFG1_FULLDUPLEX);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	/*(void)splx(s);*/
	RG_UNLOCK(sc);

	sc->rg_stat_ch = timeout(rg_tick, sc, hz);

	return;
}

/*
 * Initialize the transmit descriptors.
 */
static int rg_var_init(struct rg_softc *sc)
{
	int			i;
	union RxDesc *rxptr;
	union TxDesc *txptr;

	sc->rg_desc.rx_cur_index = 0;
	sc->rg_desc.rx_last_index = 0;
	rxptr = sc->rg_desc.rx_desc;
	for(i=0;i<RG_RX_BUF_NUM;i++)
	{
		memset(&rxptr[i], 0, sizeof(union RxDesc));
		rxptr[i].so0.OWN=1;
		if(i==(RG_RX_BUF_NUM-1))
			rxptr[i].so0.EOR=1;
		rxptr[i].so0.Frame_Length = RG_BUF_SIZE;

		/* Init the RX buffer pointer register. */
		bus_dmamap_load(sc->rg_desc.rg_rx_mtag,
			sc->rg_desc.rg_rx_dmamap[i],
			sc->rg_desc.rx_buf[i]->m_data, MCLBYTES,
			rg_dma_map_buf,
			&rxptr[i],
			0 );
		bus_dmamap_sync(sc->rg_desc.rg_rx_mtag,
			sc->rg_desc.rg_rx_dmamap[i],
			BUS_DMASYNC_PREWRITE);
	}

	bus_dmamap_load(sc->rg_desc.rx_desc_tag,
		sc->rg_desc.rx_desc_dmamap,
		sc->rg_desc.rx_desc,
		sizeof(union RxDesc)*RG_RX_BUF_NUM,
		rg_dma_map_rxdesc,
		sc,
		0 );
	bus_dmamap_sync(sc->rg_desc.rx_desc_tag,
		sc->rg_desc.rx_desc_dmamap,
		BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE );

	sc->rg_desc.tx_cur_index=0;
	sc->rg_desc.tx_last_index=0;
	txptr=sc->rg_desc.tx_desc;
	for(i=0;i<RG_TX_BUF_NUM;i++)
	{
		memset(&txptr[i], 0, sizeof(union TxDesc));
		if(i==(RG_TX_BUF_NUM-1))
			txptr[i].so1.EOR=1;
	}

	bus_dmamap_load(sc->rg_desc.tx_desc_tag,
		sc->rg_desc.tx_desc_dmamap,
		sc->rg_desc.tx_desc,
		sizeof(union RxDesc)*RG_TX_BUF_NUM,
		rg_dma_map_txdesc,
		sc,
		0 );
//	bus_dmamap_sync(sc->rg_desc.tx_desc_tag,
//		sc->rg_desc.tx_desc_dmamap,
//		BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE );

	return(0);
}

static void rg_reset(struct rg_softc *sc)
{
	register int		i;

	CSR_WRITE_4(sc, RG_RXCFG, CSR_READ_4(sc, RG_RXCFG)&~0x3F);

	if(sc->rg_type>=MACFG_11)
	{
		CSR_WRITE_1(sc, RG_COMMAND, 0x8C);
	}

	DELAY(200);
	CSR_WRITE_1(sc, RG_COMMAND, RG_CMD_RESET);

	for (i = 0; i < RG_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_1(sc, RG_COMMAND) & RG_CMD_RESET))
			break;
	}

	if (i == RG_TIMEOUT)
		device_printf(sc->dev,"reset never completed!\n");

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void rg_stop(struct rg_softc *sc)		/* Stop Driver */
{
	struct ifnet		*ifp;

/*	RG_LOCK_ASSERT(sc);*/

	ifp = RG_GET_IFNET(sc);
	ifp->if_timer = 0;

	untimeout(rg_tick, sc, sc->rg_stat_ch);

	CSR_WRITE_2(sc, RG_IMR, 0x0000);
	rg_reset(sc);

	/*
	 * Free the TX list buffers.
	 */
	while(sc->rg_desc.tx_last_index!=sc->rg_desc.tx_cur_index)
	{
		if(sc->rg_desc.rg_tx_mtag)
		{
			bus_dmamap_sync(sc->rg_desc.rg_tx_mtag,
				sc->rg_desc.rg_tx_dmamap[sc->rg_desc.tx_last_index],
				BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->rg_desc.rg_tx_mtag,
				sc->rg_desc.rg_tx_dmamap[sc->rg_desc.tx_last_index] );
		}

		if(sc->rg_desc.tx_buf[sc->rg_desc.tx_last_index]!=NULL)
		{
			m_freem(sc->rg_desc.tx_buf[sc->rg_desc.tx_last_index]);
			sc->rg_desc.tx_buf[sc->rg_desc.tx_last_index] = NULL;
		}
		sc->rg_desc.tx_last_index = (sc->rg_desc.tx_last_index+1)%RG_TX_BUF_NUM;
	}

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	return;
}

/*
 * Main transmit routine.
 */
static void rg_start(struct ifnet *ifp)	/* Transmit Packet*/
{
	struct rg_softc		*sc;
	struct mbuf		*m_head = NULL;

#if 0	/*print the destination and source MAC addresses of tx packets*/
	int i;
#endif

	sc = ifp->if_softc;	/* Paste to ifp in function rg_attach(dev) */

	RG_LOCK(sc);

/*	RG_LOCK_ASSERT(sc);*/

	if((sc->driver_detach == 1)||(sc->rx_fifo_overflow != 0)){
		RG_UNLOCK(sc);
		return;
	}

	while(1)
	{
		int fs=1,ls=0,TxLen=0,PktLen,limit;
		struct mbuf *ptr;
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);	/* Remove(get) data from system transmit queue */
		if (m_head == NULL){
			break;
		}

		if(CountMbufNum(m_head,&limit)>CountFreeTxDescNum(sc->rg_desc))	/* No enough descriptor */
		{
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}

		if (ifp->if_bpf)			/* If there's a BPF listener, bounce a copy of this frame to him. */
		{
/*#if OS_VER < VERSION(5,1)*/
#if OS_VER < VERSION(4,9)
			bpf_mtap(ifp,m_head);
#else
			bpf_mtap(ifp->if_bpf,m_head);
#endif
		}

		if(limit)	/* At least one mbuf data size small than RG_MINI_DESC_SIZE */
		{
#ifdef _DEBUG_
			ptr=m_head;
			printf("Limit=%d",limit);
			while(ptr!=NULL)
			{
				printf(", len=%d T=%d F=%d",ptr->m_len,ptr->m_type,ptr->m_flags);
				ptr=ptr->m_next;
			}
			printf("\n===== Reach limit ======\n");
#endif
			if (rg_encap(sc, m_head))
			{
				IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				break;
			}
			m_head = sc->rg_desc.tx_buf[sc->rg_desc.tx_cur_index];
			WritePacket(sc,m_head->m_data,m_head->m_len,1,1);
			continue;
		}

		ptr = m_head;
		PktLen = ptr->m_pkthdr.len;
#ifdef _DEBUG_
			printf("PktLen=%d",PktLen);
#endif
		while(ptr!=NULL)
		{
			if(ptr->m_len >0)
			{
#ifdef _DEBUG_
					printf(", len=%d T=%d F=%d",ptr->m_len,ptr->m_type,ptr->m_flags);
#endif
				TxLen += ptr->m_len;
				if(TxLen >= PktLen)
				{
					ls=1;
					sc->rg_desc.tx_buf[sc->rg_desc.tx_cur_index] = m_head;
				}
				else
					sc->rg_desc.tx_buf[sc->rg_desc.tx_cur_index] = NULL;

				WritePacket(sc,ptr->m_data,ptr->m_len,fs,ls);
				fs=0;
			}
			ptr = ptr->m_next;
		}
#ifdef _DEBUG_
			printf("\n");
#endif
	}
	ifp->if_timer = 5;

	RG_UNLOCK(sc);

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int rg_encap(struct rg_softc *sc,struct mbuf *m_head)		/* Only used in ~C+ mode */
{
	struct mbuf		*m_new = NULL;

	m_new = m_defrag(m_head, M_DONTWAIT);

	if (m_new == NULL) {
		printf("re%d: no memory for tx list", sc->rg_unit);
		return (1);
	}
	m_head = m_new;

	/* Pad frames to at least 60 bytes. */
	if (m_head->m_pkthdr.len < RG_MIN_FRAMELEN) {	/* Case length < 60 bytes */
		/*
		 * Make security concious people happy: zero out the
		 * bytes in the pad area, since we don't know what
		 * this mbuf cluster buffer's previous user might
		 * have left in it.
		 */
		bzero(mtod(m_head, char *) + m_head->m_pkthdr.len,
		     RG_MIN_FRAMELEN - m_head->m_pkthdr.len);
		m_head->m_pkthdr.len = RG_MIN_FRAMELEN;
		m_head->m_len = m_head->m_pkthdr.len;
	}

	sc->rg_desc.tx_buf[sc->rg_desc.tx_cur_index] = m_head;

	return(0);
}

static void WritePacket(struct rg_softc	*sc, caddr_t addr, int len,int fs_flag,int ls_flag)
{
	union TxDesc *txptr;

	bus_dmamap_sync(sc->rg_desc.tx_desc_tag,
		sc->rg_desc.tx_desc_dmamap,
		BUS_DMASYNC_POSTWRITE);

	txptr=&(sc->rg_desc.tx_desc[sc->rg_desc.tx_cur_index]);

	txptr->ul[0]&=0x40000000;
	txptr->ul[1]=0;

	if(fs_flag)
		txptr->so1.FS=1;
	if(ls_flag)
		txptr->so1.LS=1;
	txptr->so1.Frame_Length = len;
	bus_dmamap_load(sc->rg_desc.rg_tx_mtag,
		sc->rg_desc.rg_tx_dmamap[sc->rg_desc.tx_cur_index],
		addr,
		len,
		rg_dma_map_buf, txptr,
		0 );
	bus_dmamap_sync(sc->rg_desc.rg_tx_mtag,
		sc->rg_desc.rg_tx_dmamap[sc->rg_desc.tx_cur_index],
		BUS_DMASYNC_PREREAD);
	txptr->so1.OWN = 1;

	bus_dmamap_sync(sc->rg_desc.tx_desc_tag,
		sc->rg_desc.tx_desc_dmamap,
		BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE );

	if (ls_flag) {
		CSR_WRITE_1(sc, RG_TPPOLL, RG_NPQ);
	}

	sc->rg_desc.tx_cur_index = (sc->rg_desc.tx_cur_index+1)%RG_TX_BUF_NUM;
}

static int CountFreeTxDescNum(struct rg_descriptor desc)
{
	int ret=desc.tx_last_index-desc.tx_cur_index;
	if(ret<=0)
		ret+=RG_TX_BUF_NUM;
	ret--;
	return ret;
}

static int CountMbufNum(struct mbuf *m_head, int *limit)
{
	int ret=0;
	struct mbuf *ptr=m_head;

	*limit=0;	/* 0:no limit find, 1:intermediate mbuf data size < RG_MINI_DESC_SIZE byte */
	while(ptr!=NULL)
	{
		if(ptr->m_len >0)
		{
			ret++;
			if(ptr->m_len<RG_MINI_DESC_SIZE && ptr->m_next!=NULL)	/* except last descriptor */
				*limit=1;
		}
		ptr=ptr->m_next;
	}
	return ret;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
static void rg_txeof(struct rg_softc *sc)	/* Transmit OK/ERR handler */
{
	union TxDesc *txptr;
	struct ifnet		*ifp;

	/*	printf("X");*/

	ifp = RG_GET_IFNET(sc);

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	bus_dmamap_sync(sc->rg_desc.tx_desc_tag,
		sc->rg_desc.tx_desc_dmamap,
		BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	txptr=&(sc->rg_desc.tx_desc[sc->rg_desc.tx_last_index]);
	while(txptr->so1.OWN==0 && sc->rg_desc.tx_last_index!=sc->rg_desc.tx_cur_index)
	{
#ifdef _DEBUG_
			printf("**** Tx OK  ****\n");
#endif
		bus_dmamap_sync(sc->rg_desc.rg_tx_mtag,
			sc->rg_desc.rg_tx_dmamap[sc->rg_desc.tx_last_index],
			BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->rg_desc.rg_tx_mtag,
			sc->rg_desc.rg_tx_dmamap[sc->rg_desc.tx_last_index] );

		if(sc->rg_desc.tx_buf[sc->rg_desc.tx_last_index]!=NULL)
		{
			m_freem(sc->rg_desc.tx_buf[sc->rg_desc.tx_last_index]);	/* Free Current MBuf in a Mbuf list*/
			sc->rg_desc.tx_buf[sc->rg_desc.tx_last_index] = NULL;
		}

		sc->rg_desc.tx_last_index = (sc->rg_desc.tx_last_index+1)%RG_TX_BUF_NUM;
		txptr=&sc->rg_desc.tx_desc[sc->rg_desc.tx_last_index];
		ifp->if_opackets++;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	}

	bus_dmamap_sync(sc->rg_desc.tx_desc_tag,
		sc->rg_desc.tx_desc_dmamap,
		BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	return;
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 *
 * You know there's something wrong with a PCI bus-master chip design
 * when you have to use m_devget().
 *
 * The receive operation is badly documented in the datasheet, so I'll
 * attempt to document it here. The driver provides a buffer area and
 * places its base address in the RX buffer start address register.
 * The chip then begins copying frames into the RX buffer. Each frame
 * is preceeded by a 32-bit RX status word which specifies the length
 * of the frame and certain other status bits. Each frame (starting with
 * the status word) is also 32-bit aligned. The frame length is in the
 * first 16 bits of the status word; the lower 15 bits correspond with
 * the 'rx status register' mentioned in the datasheet.
 *
 * Note: to make the Alpha happy, the frame payload needs to be aligned
 * on a 32-bit boundary. To achieve this, we cheat a bit by copying from
 * the ring buffer starting at an address two bytes before the actual
 * data location. We can then shave off the first two bytes using m_adj().
 * The reason we do this is because m_devget() doesn't let us specify an
 * offset into the mbuf storage space, so we have to artificially create
 * one. The ring is allocated in such a way that there are a few unused
 * bytes of space preceecing it so that it will be safe for us to do the
 * 2-byte backstep even if reading from the ring at offset 0.
 */
static void rg_rxeof(sc)	/* Receive Data OK/ERR handler */
	struct rg_softc		*sc;
{
	struct ether_header	*eh;
	struct mbuf		*m;
	struct ifnet		*ifp;
	union RxDesc *rxptr;
	int bError;
	struct mbuf *buf;

/*		RG_LOCK_ASSERT(sc);*/

	ifp = RG_GET_IFNET(sc);

	bus_dmamap_sync(sc->rg_desc.rx_desc_tag,
		sc->rg_desc.rx_desc_dmamap,
		BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	rxptr=&(sc->rg_desc.rx_desc[sc->rg_desc.rx_cur_index]);
	while(rxptr->so0.OWN==0)	/* Receive OK */
	{
		bError = 0;

		/* Check if this packet is received correctly*/
		if(rxptr->ul[0]&0x200000)	/*Check RES bit*/
		{
			bError=1;
			goto update_desc;
		}

		buf = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR); /* Alloc a new mbuf */
		if(buf==NULL)
		{
			bError=1;
			goto update_desc;
		}

		bus_dmamap_sync(sc->rg_desc.rg_rx_mtag,
			sc->rg_desc.rg_rx_dmamap[sc->rg_desc.rx_cur_index],
			BUS_DMASYNC_POSTREAD );
		bus_dmamap_unload(sc->rg_desc.rg_rx_mtag,
			sc->rg_desc.rg_rx_dmamap[sc->rg_desc.rx_cur_index] );

		m = sc->rg_desc.rx_buf[sc->rg_desc.rx_cur_index];
		sc->rg_desc.rx_buf[sc->rg_desc.rx_cur_index] = buf;
		m->m_pkthdr.len = m->m_len = rxptr->so0.Frame_Length-ETHER_CRC_LEN;
		m->m_pkthdr.rcvif = ifp;

		eh = mtod(m, struct ether_header *);
		ifp->if_ipackets++;
#ifdef _DEBUG_
			printf("Rcv Packet, Len=%d \n",m->m_len);
#endif

		RG_UNLOCK(sc);

/*#if OS_VER < VERSION(5,1)*/
#if OS_VER < VERSION(4,9)
			/* Remove header from mbuf and pass it on. */
		m_adj(m, sizeof(struct ether_header));
		ether_input(ifp, eh, m);
#else
		(*ifp->if_input)(ifp, m);
#endif
		RG_LOCK(sc);

update_desc:
		rxptr->ul[0]&=0x40000000;	/* keep EOR bit */
		rxptr->ul[1]=0;

		rxptr->so0.Frame_Length = RG_BUF_SIZE;
		if(!bError)
		{
			bus_dmamap_load(sc->rg_desc.rg_rx_mtag,
				sc->rg_desc.rg_rx_dmamap[sc->rg_desc.rx_cur_index],
				sc->rg_desc.rx_buf[sc->rg_desc.rx_cur_index]->m_data,
				MCLBYTES,
				rg_dma_map_buf, rxptr,
				0 );
			bus_dmamap_sync(sc->rg_desc.rg_rx_mtag,
				sc->rg_desc.rg_rx_dmamap[sc->rg_desc.rx_cur_index],
				BUS_DMASYNC_PREWRITE);
		}
		rxptr->so0.OWN=1;
		sc->rg_desc.rx_cur_index = (sc->rg_desc.rx_cur_index+1)%RG_RX_BUF_NUM;
		rxptr=&sc->rg_desc.rx_desc[sc->rg_desc.rx_cur_index];
	}

	bus_dmamap_sync(sc->rg_desc.rx_desc_tag,
		sc->rg_desc.rx_desc_dmamap,
		BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	return;
}

static void rg_intr(void *arg)	/* Interrupt Handler */
{
	struct rg_softc		*sc;

	sc = arg;

	if ((CSR_READ_2(sc, RG_ISR) & RG_INTRS) == 0){
		return;// (FILTER_STRAY);
	}

	/* Disable interrupts. */
	CSR_WRITE_2(sc, RG_IMR, 0x0000);

#if OS_VER<VERSION(7,0)
	rg_int_task(arg, 0);
#else
	taskqueue_enqueue_fast(taskqueue_fast, &sc->rg_inttask);
#endif
//	return (FILTER_HANDLED);
}

static void rg_int_task(void *arg, int npending)
{
	struct rg_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;
	u_int8_t		link_status;

	sc = arg;

	RG_LOCK(sc);

	ifp = RG_GET_IFNET(sc);

	status = CSR_READ_2(sc, RG_ISR);

	if (status){
		CSR_WRITE_2(sc, RG_ISR, status & 0xffbf);
	}

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
	{
		RG_UNLOCK(sc);
		return;
	}

	if ((status & RG_ISR_RX_OK) || (status & RG_ISR_RX_ERR) ||(status & RG_ISR_FIFO_OFLOW) || (status & RG_ISR_RX_OVERRUN)){
		rg_rxeof(sc);
	}

	if (sc->rg_type == MACFG_21){
		if (status & RG_ISR_FIFO_OFLOW){
			sc->rx_fifo_overflow = 1;
			CSR_WRITE_2(sc, 0x00e2, 0x0000);
			CSR_WRITE_4(sc, 0x0048, 0x4000);
			CSR_WRITE_4(sc, 0x0058, 0x4000);
		}else{
			sc->rx_fifo_overflow = 0;
			CSR_WRITE_4(sc,RG_CPCR,0x51512082);
		}

		if (status & RG_ISR_PCS_TIMEOUT){
			if((status & RG_ISR_FIFO_OFLOW) &&
			   (!(status & (RG_ISR_RX_OK | RG_ISR_TX_OK | RG_ISR_RX_OVERRUN)))){
				rg_reset(sc);
				rg_init(sc);
				sc->rx_fifo_overflow = 0;
				CSR_WRITE_2(sc, RG_ISR, RG_ISR_FIFO_OFLOW);
			}
		}
	}

	if(status & RG_ISR_LINKCHG){
		link_status = CSR_READ_1(sc, RG_PHY_STATUS);
		if(link_status & 0x02)
		{
			ifp->if_link_state = LINK_STATE_UP;
		}
		else
		{
			ifp->if_link_state = LINK_STATE_DOWN;
		}
	}

	if ((status & RG_ISR_TX_OK) || (status & RG_ISR_TX_ERR)){
		rg_txeof(sc);
	}

	if (status & RG_ISR_SYSTEM_ERR) {
		rg_reset(sc);
		rg_init(sc);
	}

	switch(sc->rg_type)
	{
		case MACFG_21:
		case MACFG_22:
		case MACFG_23:
		case MACFG_24:
			CSR_WRITE_1(sc, 0x38,0x40);
			break;

		default:
			break;
	}

	RG_UNLOCK(sc);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		rg_start(ifp);

#if OS_VER>=VERSION(7,0)
	if (CSR_READ_2(sc, RG_ISR) & RG_INTRS) {
		taskqueue_enqueue_fast(taskqueue_fast, &sc->rg_inttask);
		return;
	}
#endif

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, RG_IMR, RG_INTRS);
}

/*
 * Program the 64-bit multicast hash filter.
 */
static void rg_setmulti(sc)
	struct rg_softc		*sc;
{
	struct ifnet		*ifp;
	int			h = 0;
	u_int32_t		hashes[2] = { 0, 0 };
	struct ifmultiaddr	*ifma;
	u_int32_t		rxfilt;
	int			mcnt = 0;

	ifp = RG_GET_IFNET(sc);

	rxfilt = CSR_READ_4(sc, RG_RXCFG);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		rxfilt |= RG_RXCFG_RX_MULTI;
		CSR_WRITE_4(sc, RG_RXCFG, rxfilt);
		CSR_WRITE_4(sc, RG_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, RG_MAR4, 0xFFFFFFFF);
		return;
	}

	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, RG_MAR0, 0);
	CSR_WRITE_4(sc, RG_MAR4, 0);

	/* now program new ones */
#if OS_VER > VERSION(6,0)
	IF_ADDR_LOCK(ifp);
#endif
#if OS_VER < VERSION(4,9)
	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
				ifma = ifma->ifma_link.le_next)
#else
	TAILQ_FOREACH(ifma,&ifp->if_multiaddrs,ifma_link)
#endif
	{
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_be(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) >> 26;
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		mcnt++;
	}
#if OS_VER > VERSION(6,0)
	IF_ADDR_UNLOCK(ifp);
#endif

	if (mcnt)
		rxfilt |= RG_RXCFG_RX_MULTI;
	else
		rxfilt &= ~RG_RXCFG_RX_MULTI;

	CSR_WRITE_4(sc, RG_RXCFG, rxfilt);
	CSR_WRITE_4(sc, RG_MAR0, hashes[0]);
	CSR_WRITE_4(sc, RG_MAR4, hashes[1]);

	return;
}

static int rg_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct rg_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	/*int			s;*/
	int			error = 0;

	/*s = splimp();*/

	switch(command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFFLAGS:
		RG_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			rg_init(sc);
		} else if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			rg_stop(sc);
		}
		error = 0;
		RG_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		RG_LOCK(sc);
		rg_setmulti(sc);
		RG_UNLOCK(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->media, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	/*(void)splx(s);*/

	return(error);
}

static void rg_tick(xsc)
	void			*xsc;
{	/*called per second*/
	struct rg_softc		*sc;
	int			s;

	s = splimp();

	sc = xsc;
	/*mii = device_get_softc(sc->rg_miibus);

	mii_tick(mii);*/

	splx(s);

	if (sc->rg_type == MACFG_3 &&
	    sc->rg_8169_MacVersion != 0 &&
	    sc->rg_8169_PhyVersion == 0) {
		static int count = 0;

		if (CSR_READ_1 (sc, 0x6C) & 0x02)
			count = 0;
		else
			count++;
		if (count> 14) {
			MP_WritePhyUshort (sc, 0, 0x9000);
			count = 0;
		}
	}

	sc->rg_stat_ch = timeout (rg_tick, sc, hz);

	return;
}

#if OS_VER<VERSION(7,0)
static void rg_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct rg_softc		*sc;

	sc = ifp->if_softc;

	printf("re%d: watchdog timeout\n", sc->rg_unit);
	ifp->if_oerrors++;

	rg_txeof(sc);
	rg_rxeof(sc);
	rg_init(sc);

	return;
}
#endif

/*
 * Set media options.
 */
static int rg_ifmedia_upd(struct ifnet *ifp)
{
	struct rg_softc	*sc = ifp->if_softc;
	struct ifmedia	*ifm = &sc->media;
	int anar;
	int gbcr;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		anar = ANAR_TX_FD |
		       ANAR_TX |
		       ANAR_10_FD |
		       ANAR_10;
		gbcr = GTCR_ADV_1000TFDX |
		       GTCR_ADV_1000THDX;
		break;
	case IFM_1000_SX:
#if OS_VER < 500000
	case IFM_1000_TX:
#else
	case IFM_1000_T:
#endif
		anar = ANAR_TX_FD |
		       ANAR_TX |
		       ANAR_10_FD |
		       ANAR_10;
		gbcr = GTCR_ADV_1000TFDX |
		       GTCR_ADV_1000THDX;
		break;
	case IFM_100_TX:
		gbcr = MP_ReadPhyUshort(sc, MII_100T2CR) &
		       ~(GTCR_ADV_1000TFDX | GTCR_ADV_1000THDX);
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX) {
			anar = ANAR_TX_FD |
			       ANAR_TX |
			       ANAR_10_FD |
			       ANAR_10;
		} else {
			anar = ANAR_TX |
			       ANAR_10_FD |
			       ANAR_10;
		}
		break;
	case IFM_10_T:
		gbcr = MP_ReadPhyUshort(sc, MII_100T2CR) &
		       ~(GTCR_ADV_1000TFDX | GTCR_ADV_1000THDX);
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX) {
			anar = ANAR_10_FD |
			       ANAR_10;
		} else {
			anar = ANAR_10;
		}

		if (sc->rg_type == MACFG_13) {
			MP_WritePhyUshort(sc, MII_BMCR, 0x8000);
		}

		break;
	default:
		printf("re%d: Unsupported media type\n", sc->rg_unit);
		return(0);
	}

	MP_WritePhyUshort(sc, 0x1F, 0x0000);
	if(sc->rg_device_id==RT_DEVICEID_8169 || sc->rg_device_id==RT_DEVICEID_8169SC || sc->rg_device_id==RT_DEVICEID_8168)
	{
		MP_WritePhyUshort(sc, MII_ANAR, anar | 0x0800 | ANAR_FC);
		MP_WritePhyUshort(sc, MII_100T2CR, gbcr);
		MP_WritePhyUshort(sc, MII_BMCR, BMCR_RESET | BMCR_AUTOEN | BMCR_STARTNEG);
	}
	else if(sc->rg_type==MACFG_36)
	{
		MP_WritePhyUshort(sc, MII_ANAR, anar | 0x0800 | ANAR_FC);
		MP_WritePhyUshort(sc, MII_BMCR, BMCR_RESET | BMCR_AUTOEN | BMCR_STARTNEG);
	}
	else
	{
		MP_WritePhyUshort(sc, MII_ANAR, anar | 1);
		MP_WritePhyUshort(sc, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);
	}
	return(0);
}

/*
 * Report current media status.
 */
static void rg_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct rg_softc		*sc;
	unsigned char msr;

	sc = ifp->if_softc;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	msr = CSR_READ_1(sc, 0x6c);

	if(msr & 0x02)
		ifmr->ifm_status |= IFM_ACTIVE;
	else
		return;

	if(msr & 0x01)
		ifmr->ifm_active |= IFM_FDX;
	else
		ifmr->ifm_active |= IFM_HDX;


	if(msr & 0x04)
		ifmr->ifm_active |= IFM_10_T;
	else if(msr & 0x08)
		ifmr->ifm_active |= IFM_100_TX;
	else if(msr & 0x10)
		ifmr->ifm_active |= IFM_1000_T;

	return;
}

static void rg_8169s_init(struct rg_softc *sc)
{
	u_int16_t Data;
	u_int32_t Data_u32;
	int	i;

	if(sc->rg_type==MACFG_3){
		MP_WritePhyUshort(sc, 0x1f, 0x0001);
		MP_WritePhyUshort(sc, 0x06, 0x006e);
		MP_WritePhyUshort(sc, 0x08, 0x0708);
		MP_WritePhyUshort(sc, 0x15, 0x4000);
		MP_WritePhyUshort(sc, 0x18, 0x65c7);

		MP_WritePhyUshort(sc, 0x1f, 0x0001);
		MP_WritePhyUshort(sc, 0x03, 0x00a1);
		MP_WritePhyUshort(sc, 0x02, 0x0008);
		MP_WritePhyUshort(sc, 0x01, 0x0120);
		MP_WritePhyUshort(sc, 0x00, 0x1000);
		MP_WritePhyUshort(sc, 0x04, 0x0800);
		MP_WritePhyUshort(sc, 0x04, 0x0000);

		MP_WritePhyUshort(sc, 0x03, 0xff41);
		MP_WritePhyUshort(sc, 0x02, 0xdf60);
		MP_WritePhyUshort(sc, 0x01, 0x0140);
		MP_WritePhyUshort(sc, 0x00, 0x0077);
		MP_WritePhyUshort(sc, 0x04, 0x7800);
		MP_WritePhyUshort(sc, 0x04, 0x7000);

		MP_WritePhyUshort(sc, 0x03, 0x802f);
		MP_WritePhyUshort(sc, 0x02, 0x4f02);
		MP_WritePhyUshort(sc, 0x01, 0x0409);
		MP_WritePhyUshort(sc, 0x00, 0xf0f9);
		MP_WritePhyUshort(sc, 0x04, 0x9800);
		MP_WritePhyUshort(sc, 0x04, 0x9000);

		MP_WritePhyUshort(sc, 0x03, 0xdf01);
		MP_WritePhyUshort(sc, 0x02, 0xdf20);
		MP_WritePhyUshort(sc, 0x01, 0xff95);
		MP_WritePhyUshort(sc, 0x00, 0xba00);
		MP_WritePhyUshort(sc, 0x04, 0xa800);
		MP_WritePhyUshort(sc, 0x04, 0xa000);

		MP_WritePhyUshort(sc, 0x03, 0xff41);
		MP_WritePhyUshort(sc, 0x02, 0xdf20);
		MP_WritePhyUshort(sc, 0x01, 0x0140);
		MP_WritePhyUshort(sc, 0x00, 0x00bb);
		MP_WritePhyUshort(sc, 0x04, 0xb800);
		MP_WritePhyUshort(sc, 0x04, 0xb000);

		MP_WritePhyUshort(sc, 0x03, 0xdf41);
		MP_WritePhyUshort(sc, 0x02, 0xdc60);
		MP_WritePhyUshort(sc, 0x01, 0x6340);
		MP_WritePhyUshort(sc, 0x00, 0x007d);
		MP_WritePhyUshort(sc, 0x04, 0xd800);
		MP_WritePhyUshort(sc, 0x04, 0xd000);

		MP_WritePhyUshort(sc, 0x03, 0xdf01);
		MP_WritePhyUshort(sc, 0x02, 0xdf20);
		MP_WritePhyUshort(sc, 0x01, 0x100a);
		MP_WritePhyUshort(sc, 0x00, 0xa0ff);
		MP_WritePhyUshort(sc, 0x04, 0xf800);
		MP_WritePhyUshort(sc, 0x04, 0xf000);

		MP_WritePhyUshort(sc, 0x1f, 0x0000);
		MP_WritePhyUshort(sc, 0x0b, 0x0000);
		MP_WritePhyUshort(sc, 0x00, 0x9200);

		CSR_WRITE_1(sc, 0x82, 0x0d);
	} else if(sc->rg_type==MACFG_4) {
		MP_WritePhyUshort(sc, 0x1f, 0x0002);
		MP_WritePhyUshort(sc, 0x01, 0x90D0);
	} else if(sc->rg_type==MACFG_5) {
		MP_WritePhyUshort(sc, 0x1f, 0x0001);
		MP_WritePhyUshort(sc, 0x04, 0x0000);
		MP_WritePhyUshort(sc, 0x03, 0x00a1);
		MP_WritePhyUshort(sc, 0x02, 0x0008);
		MP_WritePhyUshort(sc, 0x01, 0x0120);
		MP_WritePhyUshort(sc, 0x00, 0x1000);
		MP_WritePhyUshort(sc, 0x04, 0x0800);

		MP_WritePhyUshort(sc, 0x04, 0x9000);
		MP_WritePhyUshort(sc, 0x03, 0x802f);
		MP_WritePhyUshort(sc, 0x02, 0x4f02);
		MP_WritePhyUshort(sc, 0x01, 0x0409);
		MP_WritePhyUshort(sc, 0x00, 0xf099);
		MP_WritePhyUshort(sc, 0x04, 0x9800);

		MP_WritePhyUshort(sc, 0x04, 0xa000);
		MP_WritePhyUshort(sc, 0x03, 0xdf01);
		MP_WritePhyUshort(sc, 0x02, 0xdf20);
		MP_WritePhyUshort(sc, 0x01, 0xff95);
		MP_WritePhyUshort(sc, 0x00, 0xba00);
		MP_WritePhyUshort(sc, 0x04, 0xa800);

		MP_WritePhyUshort(sc, 0x04, 0xf000);
		MP_WritePhyUshort(sc, 0x03, 0xdf01);
		MP_WritePhyUshort(sc, 0x02, 0xdf20);
		MP_WritePhyUshort(sc, 0x01, 0x101a);
		MP_WritePhyUshort(sc, 0x00, 0xa0ff);
		MP_WritePhyUshort(sc, 0x04, 0xf800);
		MP_WritePhyUshort(sc, 0x04, 0x0000);
		MP_WritePhyUshort(sc, 0x1f, 0x0000);

		MP_WritePhyUshort(sc, 0x1f, 0x0001);
		MP_WritePhyUshort(sc, 0x10, 0xf41b);
		MP_WritePhyUshort(sc, 0x14, 0xfb54);
		MP_WritePhyUshort(sc, 0x18, 0xf5c7);
		MP_WritePhyUshort(sc, 0x1f, 0x0000);

		MP_WritePhyUshort(sc, 0x1f, 0x0001);
		MP_WritePhyUshort(sc, 0x17, 0x0CC0);
		MP_WritePhyUshort(sc, 0x1f, 0x0000);

		MP_WritePhyUshort(sc, 0x1f, 0x0001);
		MP_WritePhyUshort(sc, 0x10, 0xf01b);

	} else if(sc->rg_type==MACFG_6) {
		MP_WritePhyUshort(sc, 0x1f, 0x0001);
		MP_WritePhyUshort(sc, 0x04, 0x0000);
		MP_WritePhyUshort(sc, 0x03, 0x00a1);
		MP_WritePhyUshort(sc, 0x02, 0x0008);
		MP_WritePhyUshort(sc, 0x01, 0x0120);
		MP_WritePhyUshort(sc, 0x00, 0x1000);
		MP_WritePhyUshort(sc, 0x04, 0x0800);

		MP_WritePhyUshort(sc, 0x04, 0x9000);
		MP_WritePhyUshort(sc, 0x03, 0x802f);
		MP_WritePhyUshort(sc, 0x02, 0x4f02);
		MP_WritePhyUshort(sc, 0x01, 0x0409);
		MP_WritePhyUshort(sc, 0x00, 0xf099);
		MP_WritePhyUshort(sc, 0x04, 0x9800);

		MP_WritePhyUshort(sc, 0x04, 0xa000);
		MP_WritePhyUshort(sc, 0x03, 0xdf01);
		MP_WritePhyUshort(sc, 0x02, 0xdf20);
		MP_WritePhyUshort(sc, 0x01, 0xff95);
		MP_WritePhyUshort(sc, 0x00, 0xba00);
		MP_WritePhyUshort(sc, 0x04, 0xa800);

		MP_WritePhyUshort(sc, 0x04, 0xf000);
		MP_WritePhyUshort(sc, 0x03, 0xdf01);
		MP_WritePhyUshort(sc, 0x02, 0xdf20);
		MP_WritePhyUshort(sc, 0x01, 0x101a);
		MP_WritePhyUshort(sc, 0x00, 0xa0ff);
		MP_WritePhyUshort(sc, 0x04, 0xf800);
		MP_WritePhyUshort(sc, 0x04, 0x0000);
		MP_WritePhyUshort(sc, 0x1f, 0x0000);

		MP_WritePhyUshort(sc, 0x1f, 0x0001);
		MP_WritePhyUshort(sc, 0x0b, 0x8480);
		MP_WritePhyUshort(sc, 0x1f, 0x0000);

		MP_WritePhyUshort(sc, 0x1f, 0x0001);
		MP_WritePhyUshort(sc, 0x18, 0x67c7);
		MP_WritePhyUshort(sc, 0x04, 0x2000);
		MP_WritePhyUshort(sc, 0x03, 0x002f);
		MP_WritePhyUshort(sc, 0x02, 0x4360);
		MP_WritePhyUshort(sc, 0x01, 0x0109);
		MP_WritePhyUshort(sc, 0x00, 0x3022);
		MP_WritePhyUshort(sc, 0x04, 0x2800);
		MP_WritePhyUshort(sc, 0x1f, 0x0000);

		MP_WritePhyUshort(sc, 0x1f, 0x0001);
		MP_WritePhyUshort(sc, 0x17, 0x0CC0);
	} else if (sc->rg_type == MACFG_14) {
		MP_WritePhyUshort(sc, 0x1f, 0x0000);
		MP_WritePhyUshort(sc, 0x11, 0x15C0);
		MP_WritePhyUshort(sc, 0x19, 0x2080);

		MP_WritePhyUshort(sc, 0x1f, 0x0003);
		MP_WritePhyUshort(sc, 0x08, 0x441D);
		MP_WritePhyUshort(sc, 0x01, 0x9100);

	} else if (sc->rg_type == MACFG_15) {
//		MP_WritePhyUshort(sc, 0x1f, 0x0000);
//		MP_WritePhyUshort(sc, 0x11, 0x15C0);
//		MP_WritePhyUshort(sc, 0x19, 0x2080);

		MP_WritePhyUshort(sc, 0x1f, 0x0003);
		MP_WritePhyUshort(sc, 0x08, 0x441D);
		MP_WritePhyUshort(sc, 0x01, 0x9100);

		MP_WritePhyUshort(sc, 0x1f, 0x0000);
		Data = MP_ReadPhyUshort(sc, 0x10);
		Data |= 0x8000;
		MP_WritePhyUshort(sc, 0x10, Data);

	} else if (sc->rg_type == MACFG_17) {
		MP_WritePhyUshort(sc, 0x1f, 0x0000);
		Data = MP_ReadPhyUshort(sc, 0x10);
		Data |= 0x8000;
		MP_WritePhyUshort(sc, 0x10, Data);

	} else if (sc->rg_type == MACFG_24) {
		MP_WritePhyUshort(sc, 0x1F, 0x0001);
		MP_WritePhyUshort(sc, 0x12, 0x2300);

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		MP_WritePhyUshort(sc, 0x00, 0x88DE);
		MP_WritePhyUshort(sc, 0x01, 0x82B1);
		MP_WritePhyUshort(sc, 0x03, 0x7002);
		MP_WritePhyUshort(sc, 0x08, 0x9E30);
		MP_WritePhyUshort(sc, 0x09, 0x01F0);
		MP_WritePhyUshort(sc, 0x0A, 0x5500);
		MP_WritePhyUshort(sc, 0x0C, 0x00C8);

		MP_WritePhyUshort(sc, 0x1F, 0x0003);
		MP_WritePhyUshort(sc, 0x12, 0xC096);
		MP_WritePhyUshort(sc, 0x16, 0x000A);

		MP_WritePhyUshort(sc, 0x1F, 0x0000);
		MP_WritePhyUshort(sc, 0x14, MP_ReadPhyUshort(sc, 0x14) | (1 << 5));
		MP_WritePhyUshort(sc, 0x0d, MP_ReadPhyUshort(sc, 0x0d) | (1 << 5));

	} else if (sc->rg_type == MACFG_25) {
		MP_WritePhyUshort(sc, 0x1F, 0x0001);
		MP_WritePhyUshort(sc, 0x12, 0x2300);
		MP_WritePhyUshort(sc, 0x03, 0x802f);
		MP_WritePhyUshort(sc, 0x02, 0x4f02);
		MP_WritePhyUshort(sc, 0x01, 0x0409);
		MP_WritePhyUshort(sc, 0x00, 0xf099);
		MP_WritePhyUshort(sc, 0x04, 0x9800);
		MP_WritePhyUshort(sc, 0x04, 0x9000);
		MP_WritePhyUshort(sc, 0x1D, 0x3D98);

		MP_WritePhyUshort(sc, 0x1f, 0x0002);
		MP_WritePhyUshort(sc, 0x00, 0x88DE);
		MP_WritePhyUshort(sc, 0x01, 0x82B1);
		MP_WritePhyUshort(sc, 0x0c, 0x7eb8);
		MP_WritePhyUshort(sc, 0x06, 0x0761);

		MP_WritePhyUshort(sc, 0x1f, 0x0003);
		MP_WritePhyUshort(sc, 0x16, 0x0f0a);

		MP_WritePhyUshort(sc, 0x1F, 0x0000);
		MP_WritePhyUshort(sc, 0x16, MP_ReadPhyUshort(sc, 0x16) | (1 << 0));
		MP_WritePhyUshort(sc, 0x14, MP_ReadPhyUshort(sc, 0x14) | (1 << 5));
		MP_WritePhyUshort(sc, 0x0d, MP_ReadPhyUshort(sc, 0x0d) | (1 << 5));

		MP_WritePhyUshort(sc, 0x1f, 0x0001);
		MP_WritePhyUshort(sc, 0x17, 0x0CC0);

	} else if (sc->rg_type == MACFG_26) {
		MP_WritePhyUshort(sc, 0x1F, 0x0001);
		MP_WritePhyUshort(sc, 0x12, 0x2300);
		MP_WritePhyUshort(sc, 0x17, 0x0CC0);
		MP_WritePhyUshort(sc, 0x1D, 0x3D98);

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		MP_WritePhyUshort(sc, 0x00, 0x88DE);
		MP_WritePhyUshort(sc, 0x01, 0x82B1);
		MP_WritePhyUshort(sc, 0x06, 0x5461);
		MP_WritePhyUshort(sc, 0x0C, 0x7EB8);

		MP_WritePhyUshort(sc, 0x1F, 0x0003);
		MP_WritePhyUshort(sc, 0x16, 0x0F0A);

		MP_WritePhyUshort(sc, 0x1F, 0x0000);
		MP_WritePhyUshort(sc, 0x16, MP_ReadPhyUshort(sc, 0x16) | (1 << 0));
		MP_WritePhyUshort(sc, 0x14, MP_ReadPhyUshort(sc, 0x14) | (1 << 5));
		MP_WritePhyUshort(sc, 0x0d, MP_ReadPhyUshort(sc, 0x0d) | (1 << 5));

	} else if (sc->rg_type == MACFG_27) {
		MP_WritePhyUshort(sc, 0x1F, 0x0000);
		MP_WritePhyUshort(sc, 0x0d, MP_ReadPhyUshort(sc, 0x0d) | (1 << 5));
		MP_WritePhyUshort(sc, 0x1F, 0x0000);

		MP_WritePhyUshort(sc, 0x1F, 0x0001);
		MP_WritePhyUshort(sc, 0x1D, 0x3D98);
		MP_WritePhyUshort(sc, 0x1F, 0x0000);

		MP_WritePhyUshort(sc, 0x1f, 0x0001);
		MP_WritePhyUshort(sc, 0x17, 0x0CC0);

	} else if (sc->rg_type == MACFG_28) {
		MP_WritePhyUshort(sc, 0x1F, 0x0000);
		MP_WritePhyUshort(sc, 0x0d, MP_ReadPhyUshort(sc, 0x0d) | (1 << 5));

		MP_WritePhyUshort(sc, 0x1f, 0x0001);
		MP_WritePhyUshort(sc, 0x17, 0x0CC0);

	} else if (sc->rg_type == MACFG_31) {
		MP_WritePhyUshort(sc, 0x1F, 0x0001);
		MP_WritePhyUshort(sc, 0x06, 0x4064);
		MP_WritePhyUshort(sc, 0x07, 0x2863);
		MP_WritePhyUshort(sc, 0x08, 0x059C);
		MP_WritePhyUshort(sc, 0x09, 0x26B4);
		MP_WritePhyUshort(sc, 0x0A, 0x6A19);
		MP_WritePhyUshort(sc, 0x0B, 0xDCC8);
		MP_WritePhyUshort(sc, 0x10, 0xF06D);
		MP_WritePhyUshort(sc, 0x14, 0x7F68);
		MP_WritePhyUshort(sc, 0x18, 0x7FD9);
		MP_WritePhyUshort(sc, 0x1C, 0xF0FF);
		MP_WritePhyUshort(sc, 0x1D, 0x3D9C);
		MP_WritePhyUshort(sc, 0x1F, 0x0003);
		MP_WritePhyUshort(sc, 0x12, 0xF49F);
		MP_WritePhyUshort(sc, 0x13, 0x070B);
		MP_WritePhyUshort(sc, 0x1A, 0x05AD);
		MP_WritePhyUshort(sc, 0x14, 0x94C0);

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		Data = MP_ReadPhyUshort(sc, 0x0B) & 0xFF00;
		Data |= 0x10;
		MP_WritePhyUshort(sc, 0x0B, Data);
		Data = MP_ReadPhyUshort(sc, 0x0C) & 0x00FF;
		Data |= 0xA200;
		MP_WritePhyUshort(sc, 0x0C, Data);

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		MP_WritePhyUshort(sc, 0x06, 0x5561);
		MP_WritePhyUshort(sc, 0x1F, 0x0005);
		MP_WritePhyUshort(sc, 0x05, 0x8332);
		MP_WritePhyUshort(sc, 0x06, 0x5561);

		if (MP_ReadEfuse(sc, 0x01) == 0xb1) {
			MP_WritePhyUshort(sc, 0x1F, 0x0002);
			MP_WritePhyUshort(sc, 0x05, 0x669A);
			MP_WritePhyUshort(sc, 0x1F, 0x0005);
			MP_WritePhyUshort(sc, 0x05, 0x8330);
			MP_WritePhyUshort(sc, 0x06, 0x669A);

			MP_WritePhyUshort(sc, 0x1F, 0x0002);
			Data = MP_ReadPhyUshort(sc, 0x0D);
			if ((Data & 0x00FF) != 0x006C) {
				Data &= 0xFF00;
				MP_WritePhyUshort(sc, 0x1F, 0x0002);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x0065);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x0066);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x0067);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x0068);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x0069);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x006A);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x006B);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x006C);
			}
		} else {
			MP_WritePhyUshort(sc, 0x1F, 0x0002);
			MP_WritePhyUshort(sc, 0x05, 0x6662);
			MP_WritePhyUshort(sc, 0x1F, 0x0005);
			MP_WritePhyUshort(sc, 0x05, 0x8330);
			MP_WritePhyUshort(sc, 0x06, 0x6662);
		}

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		Data = MP_ReadPhyUshort(sc, 0x0D);
		Data |= 0x300;
		MP_WritePhyUshort(sc, 0x0D, Data);
		Data = MP_ReadPhyUshort(sc, 0x0F);
		Data |= 0x10;
		MP_WritePhyUshort(sc, 0x0F, Data);

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		Data = MP_ReadPhyUshort(sc, 0x02);
		Data &= ~0x600;
		Data |= 0x100;
		MP_WritePhyUshort(sc, 0x02, Data);
		Data = MP_ReadPhyUshort(sc, 0x03);
		Data &= ~0xE000;
		MP_WritePhyUshort(sc, 0x03, Data);

		MP_WritePhyUshort(sc, 0x1F, 0x0001);
		MP_WritePhyUshort(sc, 0x17, 0x0CC0);

		MP_WritePhyUshort(sc, 0x1F, 0x0005);
		MP_WritePhyUshort(sc, 0x05, 0x001B);
		if (MP_ReadPhyUshort(sc, 0x06) == 0xBF00) {
			MP_WritePhyUshort(sc, 0x1f, 0x0005);
			MP_WritePhyUshort(sc, 0x05, 0xfff6);
			MP_WritePhyUshort(sc, 0x06, 0x0080);
			MP_WritePhyUshort(sc, 0x05, 0x8000);
			MP_WritePhyUshort(sc, 0x06, 0xf8f9);
			MP_WritePhyUshort(sc, 0x06, 0xfaef);
			MP_WritePhyUshort(sc, 0x06, 0x59ee);
			MP_WritePhyUshort(sc, 0x06, 0xf8ea);
			MP_WritePhyUshort(sc, 0x06, 0x00ee);
			MP_WritePhyUshort(sc, 0x06, 0xf8eb);
			MP_WritePhyUshort(sc, 0x06, 0x00e0);
			MP_WritePhyUshort(sc, 0x06, 0xf87c);
			MP_WritePhyUshort(sc, 0x06, 0xe1f8);
			MP_WritePhyUshort(sc, 0x06, 0x7d59);
			MP_WritePhyUshort(sc, 0x06, 0x0fef);
			MP_WritePhyUshort(sc, 0x06, 0x0139);
			MP_WritePhyUshort(sc, 0x06, 0x029e);
			MP_WritePhyUshort(sc, 0x06, 0x06ef);
			MP_WritePhyUshort(sc, 0x06, 0x1039);
			MP_WritePhyUshort(sc, 0x06, 0x089f);
			MP_WritePhyUshort(sc, 0x06, 0x2aee);
			MP_WritePhyUshort(sc, 0x06, 0xf8ea);
			MP_WritePhyUshort(sc, 0x06, 0x00ee);
			MP_WritePhyUshort(sc, 0x06, 0xf8eb);
			MP_WritePhyUshort(sc, 0x06, 0x01e0);
			MP_WritePhyUshort(sc, 0x06, 0xf87c);
			MP_WritePhyUshort(sc, 0x06, 0xe1f8);
			MP_WritePhyUshort(sc, 0x06, 0x7d58);
			MP_WritePhyUshort(sc, 0x06, 0x409e);
			MP_WritePhyUshort(sc, 0x06, 0x0f39);
			MP_WritePhyUshort(sc, 0x06, 0x46aa);
			MP_WritePhyUshort(sc, 0x06, 0x0bbf);
			MP_WritePhyUshort(sc, 0x06, 0x8290);
			MP_WritePhyUshort(sc, 0x06, 0xd682);
			MP_WritePhyUshort(sc, 0x06, 0x9802);
			MP_WritePhyUshort(sc, 0x06, 0x014f);
			MP_WritePhyUshort(sc, 0x06, 0xae09);
			MP_WritePhyUshort(sc, 0x06, 0xbf82);
			MP_WritePhyUshort(sc, 0x06, 0x98d6);
			MP_WritePhyUshort(sc, 0x06, 0x82a0);
			MP_WritePhyUshort(sc, 0x06, 0x0201);
			MP_WritePhyUshort(sc, 0x06, 0x4fef);
			MP_WritePhyUshort(sc, 0x06, 0x95fe);
			MP_WritePhyUshort(sc, 0x06, 0xfdfc);
			MP_WritePhyUshort(sc, 0x06, 0x05f8);
			MP_WritePhyUshort(sc, 0x06, 0xf9fa);
			MP_WritePhyUshort(sc, 0x06, 0xeef8);
			MP_WritePhyUshort(sc, 0x06, 0xea00);
			MP_WritePhyUshort(sc, 0x06, 0xeef8);
			MP_WritePhyUshort(sc, 0x06, 0xeb00);
			MP_WritePhyUshort(sc, 0x06, 0xe2f8);
			MP_WritePhyUshort(sc, 0x06, 0x7ce3);
			MP_WritePhyUshort(sc, 0x06, 0xf87d);
			MP_WritePhyUshort(sc, 0x06, 0xa511);
			MP_WritePhyUshort(sc, 0x06, 0x1112);
			MP_WritePhyUshort(sc, 0x06, 0xd240);
			MP_WritePhyUshort(sc, 0x06, 0xd644);
			MP_WritePhyUshort(sc, 0x06, 0x4402);
			MP_WritePhyUshort(sc, 0x06, 0x8217);
			MP_WritePhyUshort(sc, 0x06, 0xd2a0);
			MP_WritePhyUshort(sc, 0x06, 0xd6aa);
			MP_WritePhyUshort(sc, 0x06, 0xaa02);
			MP_WritePhyUshort(sc, 0x06, 0x8217);
			MP_WritePhyUshort(sc, 0x06, 0xae0f);
			MP_WritePhyUshort(sc, 0x06, 0xa544);
			MP_WritePhyUshort(sc, 0x06, 0x4402);
			MP_WritePhyUshort(sc, 0x06, 0xae4d);
			MP_WritePhyUshort(sc, 0x06, 0xa5aa);
			MP_WritePhyUshort(sc, 0x06, 0xaa02);
			MP_WritePhyUshort(sc, 0x06, 0xae47);
			MP_WritePhyUshort(sc, 0x06, 0xaf82);
			MP_WritePhyUshort(sc, 0x06, 0x13ee);
			MP_WritePhyUshort(sc, 0x06, 0x834e);
			MP_WritePhyUshort(sc, 0x06, 0x00ee);
			MP_WritePhyUshort(sc, 0x06, 0x834d);
			MP_WritePhyUshort(sc, 0x06, 0x0fee);
			MP_WritePhyUshort(sc, 0x06, 0x834c);
			MP_WritePhyUshort(sc, 0x06, 0x0fee);
			MP_WritePhyUshort(sc, 0x06, 0x834f);
			MP_WritePhyUshort(sc, 0x06, 0x00ee);
			MP_WritePhyUshort(sc, 0x06, 0x8351);
			MP_WritePhyUshort(sc, 0x06, 0x00ee);
			MP_WritePhyUshort(sc, 0x06, 0x834a);
			MP_WritePhyUshort(sc, 0x06, 0xffee);
			MP_WritePhyUshort(sc, 0x06, 0x834b);
			MP_WritePhyUshort(sc, 0x06, 0xffe0);
			MP_WritePhyUshort(sc, 0x06, 0x8330);
			MP_WritePhyUshort(sc, 0x06, 0xe183);
			MP_WritePhyUshort(sc, 0x06, 0x3158);
			MP_WritePhyUshort(sc, 0x06, 0xfee4);
			MP_WritePhyUshort(sc, 0x06, 0xf88a);
			MP_WritePhyUshort(sc, 0x06, 0xe5f8);
			MP_WritePhyUshort(sc, 0x06, 0x8be0);
			MP_WritePhyUshort(sc, 0x06, 0x8332);
			MP_WritePhyUshort(sc, 0x06, 0xe183);
			MP_WritePhyUshort(sc, 0x06, 0x3359);
			MP_WritePhyUshort(sc, 0x06, 0x0fe2);
			MP_WritePhyUshort(sc, 0x06, 0x834d);
			MP_WritePhyUshort(sc, 0x06, 0x0c24);
			MP_WritePhyUshort(sc, 0x06, 0x5af0);
			MP_WritePhyUshort(sc, 0x06, 0x1e12);
			MP_WritePhyUshort(sc, 0x06, 0xe4f8);
			MP_WritePhyUshort(sc, 0x06, 0x8ce5);
			MP_WritePhyUshort(sc, 0x06, 0xf88d);
			MP_WritePhyUshort(sc, 0x06, 0xaf82);
			MP_WritePhyUshort(sc, 0x06, 0x13e0);
			MP_WritePhyUshort(sc, 0x06, 0x834f);
			MP_WritePhyUshort(sc, 0x06, 0x10e4);
			MP_WritePhyUshort(sc, 0x06, 0x834f);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x4e78);
			MP_WritePhyUshort(sc, 0x06, 0x009f);
			MP_WritePhyUshort(sc, 0x06, 0x0ae0);
			MP_WritePhyUshort(sc, 0x06, 0x834f);
			MP_WritePhyUshort(sc, 0x06, 0xa010);
			MP_WritePhyUshort(sc, 0x06, 0xa5ee);
			MP_WritePhyUshort(sc, 0x06, 0x834e);
			MP_WritePhyUshort(sc, 0x06, 0x01e0);
			MP_WritePhyUshort(sc, 0x06, 0x834e);
			MP_WritePhyUshort(sc, 0x06, 0x7805);
			MP_WritePhyUshort(sc, 0x06, 0x9e9a);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x4e78);
			MP_WritePhyUshort(sc, 0x06, 0x049e);
			MP_WritePhyUshort(sc, 0x06, 0x10e0);
			MP_WritePhyUshort(sc, 0x06, 0x834e);
			MP_WritePhyUshort(sc, 0x06, 0x7803);
			MP_WritePhyUshort(sc, 0x06, 0x9e0f);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x4e78);
			MP_WritePhyUshort(sc, 0x06, 0x019e);
			MP_WritePhyUshort(sc, 0x06, 0x05ae);
			MP_WritePhyUshort(sc, 0x06, 0x0caf);
			MP_WritePhyUshort(sc, 0x06, 0x81f8);
			MP_WritePhyUshort(sc, 0x06, 0xaf81);
			MP_WritePhyUshort(sc, 0x06, 0xa3af);
			MP_WritePhyUshort(sc, 0x06, 0x81dc);
			MP_WritePhyUshort(sc, 0x06, 0xaf82);
			MP_WritePhyUshort(sc, 0x06, 0x13ee);
			MP_WritePhyUshort(sc, 0x06, 0x8348);
			MP_WritePhyUshort(sc, 0x06, 0x00ee);
			MP_WritePhyUshort(sc, 0x06, 0x8349);
			MP_WritePhyUshort(sc, 0x06, 0x00e0);
			MP_WritePhyUshort(sc, 0x06, 0x8351);
			MP_WritePhyUshort(sc, 0x06, 0x10e4);
			MP_WritePhyUshort(sc, 0x06, 0x8351);
			MP_WritePhyUshort(sc, 0x06, 0x5801);
			MP_WritePhyUshort(sc, 0x06, 0x9fea);
			MP_WritePhyUshort(sc, 0x06, 0xd000);
			MP_WritePhyUshort(sc, 0x06, 0xd180);
			MP_WritePhyUshort(sc, 0x06, 0x1f66);
			MP_WritePhyUshort(sc, 0x06, 0xe2f8);
			MP_WritePhyUshort(sc, 0x06, 0xeae3);
			MP_WritePhyUshort(sc, 0x06, 0xf8eb);
			MP_WritePhyUshort(sc, 0x06, 0x5af8);
			MP_WritePhyUshort(sc, 0x06, 0x1e20);
			MP_WritePhyUshort(sc, 0x06, 0xe6f8);
			MP_WritePhyUshort(sc, 0x06, 0xeae5);
			MP_WritePhyUshort(sc, 0x06, 0xf8eb);
			MP_WritePhyUshort(sc, 0x06, 0xd302);
			MP_WritePhyUshort(sc, 0x06, 0xb3fe);
			MP_WritePhyUshort(sc, 0x06, 0xe2f8);
			MP_WritePhyUshort(sc, 0x06, 0x7cef);
			MP_WritePhyUshort(sc, 0x06, 0x325b);
			MP_WritePhyUshort(sc, 0x06, 0x80e3);
			MP_WritePhyUshort(sc, 0x06, 0xf87d);
			MP_WritePhyUshort(sc, 0x06, 0x9e03);
			MP_WritePhyUshort(sc, 0x06, 0x7dff);
			MP_WritePhyUshort(sc, 0x06, 0xff0d);
			MP_WritePhyUshort(sc, 0x06, 0x581c);
			MP_WritePhyUshort(sc, 0x06, 0x551a);
			MP_WritePhyUshort(sc, 0x06, 0x6511);
			MP_WritePhyUshort(sc, 0x06, 0xa190);
			MP_WritePhyUshort(sc, 0x06, 0xd3e2);
			MP_WritePhyUshort(sc, 0x06, 0x8348);
			MP_WritePhyUshort(sc, 0x06, 0xe383);
			MP_WritePhyUshort(sc, 0x06, 0x491b);
			MP_WritePhyUshort(sc, 0x06, 0x56ab);
			MP_WritePhyUshort(sc, 0x06, 0x08ef);
			MP_WritePhyUshort(sc, 0x06, 0x56e6);
			MP_WritePhyUshort(sc, 0x06, 0x8348);
			MP_WritePhyUshort(sc, 0x06, 0xe783);
			MP_WritePhyUshort(sc, 0x06, 0x4910);
			MP_WritePhyUshort(sc, 0x06, 0xd180);
			MP_WritePhyUshort(sc, 0x06, 0x1f66);
			MP_WritePhyUshort(sc, 0x06, 0xa004);
			MP_WritePhyUshort(sc, 0x06, 0xb9e2);
			MP_WritePhyUshort(sc, 0x06, 0x8348);
			MP_WritePhyUshort(sc, 0x06, 0xe383);
			MP_WritePhyUshort(sc, 0x06, 0x49ef);
			MP_WritePhyUshort(sc, 0x06, 0x65e2);
			MP_WritePhyUshort(sc, 0x06, 0x834a);
			MP_WritePhyUshort(sc, 0x06, 0xe383);
			MP_WritePhyUshort(sc, 0x06, 0x4b1b);
			MP_WritePhyUshort(sc, 0x06, 0x56aa);
			MP_WritePhyUshort(sc, 0x06, 0x0eef);
			MP_WritePhyUshort(sc, 0x06, 0x56e6);
			MP_WritePhyUshort(sc, 0x06, 0x834a);
			MP_WritePhyUshort(sc, 0x06, 0xe783);
			MP_WritePhyUshort(sc, 0x06, 0x4be2);
			MP_WritePhyUshort(sc, 0x06, 0x834d);
			MP_WritePhyUshort(sc, 0x06, 0xe683);
			MP_WritePhyUshort(sc, 0x06, 0x4ce0);
			MP_WritePhyUshort(sc, 0x06, 0x834d);
			MP_WritePhyUshort(sc, 0x06, 0xa000);
			MP_WritePhyUshort(sc, 0x06, 0x0caf);
			MP_WritePhyUshort(sc, 0x06, 0x81dc);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x4d10);
			MP_WritePhyUshort(sc, 0x06, 0xe483);
			MP_WritePhyUshort(sc, 0x06, 0x4dae);
			MP_WritePhyUshort(sc, 0x06, 0x0480);
			MP_WritePhyUshort(sc, 0x06, 0xe483);
			MP_WritePhyUshort(sc, 0x06, 0x4de0);
			MP_WritePhyUshort(sc, 0x06, 0x834e);
			MP_WritePhyUshort(sc, 0x06, 0x7803);
			MP_WritePhyUshort(sc, 0x06, 0x9e0b);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x4e78);
			MP_WritePhyUshort(sc, 0x06, 0x049e);
			MP_WritePhyUshort(sc, 0x06, 0x04ee);
			MP_WritePhyUshort(sc, 0x06, 0x834e);
			MP_WritePhyUshort(sc, 0x06, 0x02e0);
			MP_WritePhyUshort(sc, 0x06, 0x8332);
			MP_WritePhyUshort(sc, 0x06, 0xe183);
			MP_WritePhyUshort(sc, 0x06, 0x3359);
			MP_WritePhyUshort(sc, 0x06, 0x0fe2);
			MP_WritePhyUshort(sc, 0x06, 0x834d);
			MP_WritePhyUshort(sc, 0x06, 0x0c24);
			MP_WritePhyUshort(sc, 0x06, 0x5af0);
			MP_WritePhyUshort(sc, 0x06, 0x1e12);
			MP_WritePhyUshort(sc, 0x06, 0xe4f8);
			MP_WritePhyUshort(sc, 0x06, 0x8ce5);
			MP_WritePhyUshort(sc, 0x06, 0xf88d);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x30e1);
			MP_WritePhyUshort(sc, 0x06, 0x8331);
			MP_WritePhyUshort(sc, 0x06, 0x6801);
			MP_WritePhyUshort(sc, 0x06, 0xe4f8);
			MP_WritePhyUshort(sc, 0x06, 0x8ae5);
			MP_WritePhyUshort(sc, 0x06, 0xf88b);
			MP_WritePhyUshort(sc, 0x06, 0xae37);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4e03);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x4ce1);
			MP_WritePhyUshort(sc, 0x06, 0x834d);
			MP_WritePhyUshort(sc, 0x06, 0x1b01);
			MP_WritePhyUshort(sc, 0x06, 0x9e04);
			MP_WritePhyUshort(sc, 0x06, 0xaaa1);
			MP_WritePhyUshort(sc, 0x06, 0xaea8);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4e04);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4f00);
			MP_WritePhyUshort(sc, 0x06, 0xaeab);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x4f78);
			MP_WritePhyUshort(sc, 0x06, 0x039f);
			MP_WritePhyUshort(sc, 0x06, 0x14ee);
			MP_WritePhyUshort(sc, 0x06, 0x834e);
			MP_WritePhyUshort(sc, 0x06, 0x05d2);
			MP_WritePhyUshort(sc, 0x06, 0x40d6);
			MP_WritePhyUshort(sc, 0x06, 0x5554);
			MP_WritePhyUshort(sc, 0x06, 0x0282);
			MP_WritePhyUshort(sc, 0x06, 0x17d2);
			MP_WritePhyUshort(sc, 0x06, 0xa0d6);
			MP_WritePhyUshort(sc, 0x06, 0xba00);
			MP_WritePhyUshort(sc, 0x06, 0x0282);
			MP_WritePhyUshort(sc, 0x06, 0x17fe);
			MP_WritePhyUshort(sc, 0x06, 0xfdfc);
			MP_WritePhyUshort(sc, 0x06, 0x05f8);
			MP_WritePhyUshort(sc, 0x06, 0xe0f8);
			MP_WritePhyUshort(sc, 0x06, 0x60e1);
			MP_WritePhyUshort(sc, 0x06, 0xf861);
			MP_WritePhyUshort(sc, 0x06, 0x6802);
			MP_WritePhyUshort(sc, 0x06, 0xe4f8);
			MP_WritePhyUshort(sc, 0x06, 0x60e5);
			MP_WritePhyUshort(sc, 0x06, 0xf861);
			MP_WritePhyUshort(sc, 0x06, 0xe0f8);
			MP_WritePhyUshort(sc, 0x06, 0x48e1);
			MP_WritePhyUshort(sc, 0x06, 0xf849);
			MP_WritePhyUshort(sc, 0x06, 0x580f);
			MP_WritePhyUshort(sc, 0x06, 0x1e02);
			MP_WritePhyUshort(sc, 0x06, 0xe4f8);
			MP_WritePhyUshort(sc, 0x06, 0x48e5);
			MP_WritePhyUshort(sc, 0x06, 0xf849);
			MP_WritePhyUshort(sc, 0x06, 0xd000);
			MP_WritePhyUshort(sc, 0x06, 0x0282);
			MP_WritePhyUshort(sc, 0x06, 0x5bbf);
			MP_WritePhyUshort(sc, 0x06, 0x8350);
			MP_WritePhyUshort(sc, 0x06, 0xef46);
			MP_WritePhyUshort(sc, 0x06, 0xdc19);
			MP_WritePhyUshort(sc, 0x06, 0xddd0);
			MP_WritePhyUshort(sc, 0x06, 0x0102);
			MP_WritePhyUshort(sc, 0x06, 0x825b);
			MP_WritePhyUshort(sc, 0x06, 0x0282);
			MP_WritePhyUshort(sc, 0x06, 0x77e0);
			MP_WritePhyUshort(sc, 0x06, 0xf860);
			MP_WritePhyUshort(sc, 0x06, 0xe1f8);
			MP_WritePhyUshort(sc, 0x06, 0x6158);
			MP_WritePhyUshort(sc, 0x06, 0xfde4);
			MP_WritePhyUshort(sc, 0x06, 0xf860);
			MP_WritePhyUshort(sc, 0x06, 0xe5f8);
			MP_WritePhyUshort(sc, 0x06, 0x61fc);
			MP_WritePhyUshort(sc, 0x06, 0x04f9);
			MP_WritePhyUshort(sc, 0x06, 0xfafb);
			MP_WritePhyUshort(sc, 0x06, 0xc6bf);
			MP_WritePhyUshort(sc, 0x06, 0xf840);
			MP_WritePhyUshort(sc, 0x06, 0xbe83);
			MP_WritePhyUshort(sc, 0x06, 0x50a0);
			MP_WritePhyUshort(sc, 0x06, 0x0101);
			MP_WritePhyUshort(sc, 0x06, 0x071b);
			MP_WritePhyUshort(sc, 0x06, 0x89cf);
			MP_WritePhyUshort(sc, 0x06, 0xd208);
			MP_WritePhyUshort(sc, 0x06, 0xebdb);
			MP_WritePhyUshort(sc, 0x06, 0x19b2);
			MP_WritePhyUshort(sc, 0x06, 0xfbff);
			MP_WritePhyUshort(sc, 0x06, 0xfefd);
			MP_WritePhyUshort(sc, 0x06, 0x04f8);
			MP_WritePhyUshort(sc, 0x06, 0xe0f8);
			MP_WritePhyUshort(sc, 0x06, 0x48e1);
			MP_WritePhyUshort(sc, 0x06, 0xf849);
			MP_WritePhyUshort(sc, 0x06, 0x6808);
			MP_WritePhyUshort(sc, 0x06, 0xe4f8);
			MP_WritePhyUshort(sc, 0x06, 0x48e5);
			MP_WritePhyUshort(sc, 0x06, 0xf849);
			MP_WritePhyUshort(sc, 0x06, 0x58f7);
			MP_WritePhyUshort(sc, 0x06, 0xe4f8);
			MP_WritePhyUshort(sc, 0x06, 0x48e5);
			MP_WritePhyUshort(sc, 0x06, 0xf849);
			MP_WritePhyUshort(sc, 0x06, 0xfc04);
			MP_WritePhyUshort(sc, 0x06, 0x4d20);
			MP_WritePhyUshort(sc, 0x06, 0x0002);
			MP_WritePhyUshort(sc, 0x06, 0x4e22);
			MP_WritePhyUshort(sc, 0x06, 0x0002);
			MP_WritePhyUshort(sc, 0x06, 0x4ddf);
			MP_WritePhyUshort(sc, 0x06, 0xff01);
			MP_WritePhyUshort(sc, 0x06, 0x4edd);
			MP_WritePhyUshort(sc, 0x06, 0xff01);
			MP_WritePhyUshort(sc, 0x06, 0xf8fa);
			MP_WritePhyUshort(sc, 0x06, 0xfbef);
			MP_WritePhyUshort(sc, 0x06, 0x79bf);
			MP_WritePhyUshort(sc, 0x06, 0xf822);
			MP_WritePhyUshort(sc, 0x06, 0xd819);
			MP_WritePhyUshort(sc, 0x06, 0xd958);
			MP_WritePhyUshort(sc, 0x06, 0x849f);
			MP_WritePhyUshort(sc, 0x06, 0x09bf);
			MP_WritePhyUshort(sc, 0x06, 0x82be);
			MP_WritePhyUshort(sc, 0x06, 0xd682);
			MP_WritePhyUshort(sc, 0x06, 0xc602);
			MP_WritePhyUshort(sc, 0x06, 0x014f);
			MP_WritePhyUshort(sc, 0x06, 0xef97);
			MP_WritePhyUshort(sc, 0x06, 0xfffe);
			MP_WritePhyUshort(sc, 0x06, 0xfc05);
			MP_WritePhyUshort(sc, 0x06, 0x17ff);
			MP_WritePhyUshort(sc, 0x06, 0xfe01);
			MP_WritePhyUshort(sc, 0x06, 0x1700);
			MP_WritePhyUshort(sc, 0x06, 0x0102);
			MP_WritePhyUshort(sc, 0x05, 0x83d8);
			MP_WritePhyUshort(sc, 0x06, 0x8051);
			MP_WritePhyUshort(sc, 0x05, 0x83d6);
			MP_WritePhyUshort(sc, 0x06, 0x82a0);
			MP_WritePhyUshort(sc, 0x05, 0x83d4);
			MP_WritePhyUshort(sc, 0x06, 0x8000);
			MP_WritePhyUshort(sc, 0x02, 0x2010);
			MP_WritePhyUshort(sc, 0x03, 0xdc00);
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
			MP_WritePhyUshort(sc, 0x0b, 0x0600);
			MP_WritePhyUshort(sc, 0x1f, 0x0005);
			MP_WritePhyUshort(sc, 0x05, 0xfff6);
			MP_WritePhyUshort(sc, 0x06, 0x00fc);
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
		}

		MP_WritePhyUshort(sc, 0x1F, 0x0000);
		MP_WritePhyUshort(sc, 0x0D, 0xF880);
		MP_WritePhyUshort(sc, 0x1F, 0x0000);
	} else if (sc->rg_type == MACFG_32) {
		MP_WritePhyUshort(sc, 0x1F, 0x0001);
		MP_WritePhyUshort(sc, 0x06, 0x4064);
		MP_WritePhyUshort(sc, 0x07, 0x2863);
		MP_WritePhyUshort(sc, 0x08, 0x059C);
		MP_WritePhyUshort(sc, 0x09, 0x26B4);
		MP_WritePhyUshort(sc, 0x0A, 0x6A19);
		MP_WritePhyUshort(sc, 0x0B, 0xBCC0);
		MP_WritePhyUshort(sc, 0x10, 0xF06D);
		MP_WritePhyUshort(sc, 0x14, 0x7F68);
		MP_WritePhyUshort(sc, 0x18, 0x7FD9);
		MP_WritePhyUshort(sc, 0x1C, 0xF0FF);
		MP_WritePhyUshort(sc, 0x1D, 0x3D9C);
		MP_WritePhyUshort(sc, 0x1F, 0x0003);
		MP_WritePhyUshort(sc, 0x12, 0xF49F);
		MP_WritePhyUshort(sc, 0x13, 0x070B);
		MP_WritePhyUshort(sc, 0x1A, 0x05AD);
		MP_WritePhyUshort(sc, 0x14, 0x94C0);

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		MP_WritePhyUshort(sc, 0x06, 0x5571);

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		MP_WritePhyUshort(sc, 0x05, 0x2642);

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		MP_WritePhyUshort(sc, 0x02, 0xC107);
		MP_WritePhyUshort(sc, 0x03, 0x1002);

		MP_WritePhyUshort(sc, 0x1F, 0x0001);
		MP_WritePhyUshort(sc, 0x16, 0x0CC0);

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		MP_WritePhyUshort(sc, 0x0F, 0x0017);

		MP_WritePhyUshort(sc, 0x1F, 0x0005);
		MP_WritePhyUshort(sc, 0x05, 0x8200);
		MP_WritePhyUshort(sc, 0x06, 0xF8F9);
		MP_WritePhyUshort(sc, 0x06, 0xFAEF);
		MP_WritePhyUshort(sc, 0x06, 0x59EE);
		MP_WritePhyUshort(sc, 0x06, 0xF8EA);
		MP_WritePhyUshort(sc, 0x06, 0x00EE);
		MP_WritePhyUshort(sc, 0x06, 0xF8EB);
		MP_WritePhyUshort(sc, 0x06, 0x00E0);
		MP_WritePhyUshort(sc, 0x06, 0xF87C);
		MP_WritePhyUshort(sc, 0x06, 0xE1F8);
		MP_WritePhyUshort(sc, 0x06, 0x7D59);
		MP_WritePhyUshort(sc, 0x06, 0x0FEF);
		MP_WritePhyUshort(sc, 0x06, 0x0139);
		MP_WritePhyUshort(sc, 0x06, 0x029E);
		MP_WritePhyUshort(sc, 0x06, 0x06EF);
		MP_WritePhyUshort(sc, 0x06, 0x1039);
		MP_WritePhyUshort(sc, 0x06, 0x089F);
		MP_WritePhyUshort(sc, 0x06, 0x2AEE);
		MP_WritePhyUshort(sc, 0x06, 0xF8EA);
		MP_WritePhyUshort(sc, 0x06, 0x00EE);
		MP_WritePhyUshort(sc, 0x06, 0xF8EB);
		MP_WritePhyUshort(sc, 0x06, 0x01E0);
		MP_WritePhyUshort(sc, 0x06, 0xF87C);
		MP_WritePhyUshort(sc, 0x06, 0xE1F8);
		MP_WritePhyUshort(sc, 0x06, 0x7D58);
		MP_WritePhyUshort(sc, 0x06, 0x409E);
		MP_WritePhyUshort(sc, 0x06, 0x0F39);
		MP_WritePhyUshort(sc, 0x06, 0x46AA);
		MP_WritePhyUshort(sc, 0x06, 0x0BBF);
		MP_WritePhyUshort(sc, 0x06, 0x8251);
		MP_WritePhyUshort(sc, 0x06, 0xD682);
		MP_WritePhyUshort(sc, 0x06, 0x5902);
		MP_WritePhyUshort(sc, 0x06, 0x014F);
		MP_WritePhyUshort(sc, 0x06, 0xAE09);
		MP_WritePhyUshort(sc, 0x06, 0xBF82);
		MP_WritePhyUshort(sc, 0x06, 0x59D6);
		MP_WritePhyUshort(sc, 0x06, 0x8261);
		MP_WritePhyUshort(sc, 0x06, 0x0201);
		MP_WritePhyUshort(sc, 0x06, 0x4FEF);
		MP_WritePhyUshort(sc, 0x06, 0x95FE);
		MP_WritePhyUshort(sc, 0x06, 0xFDFC);
		MP_WritePhyUshort(sc, 0x06, 0x054D);
		MP_WritePhyUshort(sc, 0x06, 0x2000);
		MP_WritePhyUshort(sc, 0x06, 0x024E);
		MP_WritePhyUshort(sc, 0x06, 0x2200);
		MP_WritePhyUshort(sc, 0x06, 0x024D);
		MP_WritePhyUshort(sc, 0x06, 0xDFFF);
		MP_WritePhyUshort(sc, 0x06, 0x014E);
		MP_WritePhyUshort(sc, 0x06, 0xDDFF);
		MP_WritePhyUshort(sc, 0x06, 0x0100);
		MP_WritePhyUshort(sc, 0x02, 0x6010);
		MP_WritePhyUshort(sc, 0x05, 0xFFF6);
		MP_WritePhyUshort(sc, 0x06, 0x00EC);
		MP_WritePhyUshort(sc, 0x05, 0x83D4);
		MP_WritePhyUshort(sc, 0x06, 0x8200);

	} else if (sc->rg_type == MACFG_33) {
		MP_WritePhyUshort(sc, 0x1F, 0x0001);
		MP_WritePhyUshort(sc, 0x06, 0x4064);
		MP_WritePhyUshort(sc, 0x07, 0x2863);
		MP_WritePhyUshort(sc, 0x08, 0x059C);
		MP_WritePhyUshort(sc, 0x09, 0x26B4);
		MP_WritePhyUshort(sc, 0x0A, 0x6A19);
		MP_WritePhyUshort(sc, 0x0B, 0xDCC8);
		MP_WritePhyUshort(sc, 0x10, 0xF06D);
		MP_WritePhyUshort(sc, 0x14, 0x7F68);
		MP_WritePhyUshort(sc, 0x18, 0x7FD9);
		MP_WritePhyUshort(sc, 0x1C, 0xF0FF);
		MP_WritePhyUshort(sc, 0x1D, 0x3D9C);
		MP_WritePhyUshort(sc, 0x1F, 0x0003);
		MP_WritePhyUshort(sc, 0x12, 0xF49F);
		MP_WritePhyUshort(sc, 0x13, 0x070B);
		MP_WritePhyUshort(sc, 0x1A, 0x05AD);
		MP_WritePhyUshort(sc, 0x14, 0x94C0);

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		MP_WritePhyUshort(sc, 0x06, 0x5561);
		MP_WritePhyUshort(sc, 0x1F, 0x0005);
		MP_WritePhyUshort(sc, 0x05, 0x8332);
		MP_WritePhyUshort(sc, 0x06, 0x5561);

		if (MP_ReadEfuse(sc, 0x01) == 0xb1) {
			MP_WritePhyUshort(sc, 0x1F, 0x0002);
			MP_WritePhyUshort(sc, 0x05, 0x669A);
			MP_WritePhyUshort(sc, 0x1F, 0x0005);
			MP_WritePhyUshort(sc, 0x05, 0x8330);
			MP_WritePhyUshort(sc, 0x06, 0x669A);

			MP_WritePhyUshort(sc, 0x1F, 0x0002);
			Data = MP_ReadPhyUshort(sc, 0x0D);
			if ((Data & 0x00FF) != 0x006C) {
				Data &= 0xFF00;
				MP_WritePhyUshort(sc, 0x1F, 0x0002);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x0065);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x0066);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x0067);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x0068);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x0069);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x006A);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x006B);
				MP_WritePhyUshort(sc, 0x0D, Data | 0x006C);
			}
		} else {
			MP_WritePhyUshort(sc, 0x1F, 0x0002);
			MP_WritePhyUshort(sc, 0x05, 0x2642);
			MP_WritePhyUshort(sc, 0x1F, 0x0005);
			MP_WritePhyUshort(sc, 0x05, 0x8330);
			MP_WritePhyUshort(sc, 0x06, 0x2642);
		}

		if (MP_ReadEfuse(sc, 0x30) == 0x98) {
			MP_WritePhyUshort(sc, 0x1F, 0x0000);
			MP_WritePhyUshort(sc, 0x11, MP_ReadPhyUshort(sc, 0x11) & ~0x02);
			MP_WritePhyUshort(sc, 0x1F, 0x0005);
			MP_WritePhyUshort(sc, 0x01, MP_ReadPhyUshort(sc, 0x01) | 0x200);
		} else if (MP_ReadEfuse(sc, 0x30) == 0x90) {
			MP_WritePhyUshort(sc, 0x1F, 0x0005);
			MP_WritePhyUshort(sc, 0x01, MP_ReadPhyUshort(sc, 0x01) & ~0x200);
			MP_WritePhyUshort(sc, 0x1F, 0x0000);
			MP_WritePhyUshort(sc, 0x16, 0x5101);
		}

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		Data = MP_ReadPhyUshort(sc, 0x02);
		Data &= ~0x600;
		Data |= 0x100;
		MP_WritePhyUshort(sc, 0x02, Data);
		Data = MP_ReadPhyUshort(sc, 0x03);
		Data &= ~0xE000;
		MP_WritePhyUshort(sc, 0x03, Data);

		MP_WritePhyUshort(sc, 0x1F, 0x0001);
		MP_WritePhyUshort(sc, 0x17, 0x0CC0);

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		Data = MP_ReadPhyUshort(sc, 0x0F);
		Data |= 0x17;
		MP_WritePhyUshort(sc, 0x0F, Data);

		MP_WritePhyUshort(sc, 0x1F, 0x0005);
		MP_WritePhyUshort(sc, 0x05, 0x001B);
		if (MP_ReadPhyUshort(sc, 0x06) == 0xB300) {
			MP_WritePhyUshort(sc, 0x1f, 0x0005);
			MP_WritePhyUshort(sc, 0x05, 0xfff6);
			MP_WritePhyUshort(sc, 0x06, 0x0080);
			MP_WritePhyUshort(sc, 0x05, 0x8000);
			MP_WritePhyUshort(sc, 0x06, 0xf8f9);
			MP_WritePhyUshort(sc, 0x06, 0xfaee);
			MP_WritePhyUshort(sc, 0x06, 0xf8ea);
			MP_WritePhyUshort(sc, 0x06, 0x00ee);
			MP_WritePhyUshort(sc, 0x06, 0xf8eb);
			MP_WritePhyUshort(sc, 0x06, 0x00e2);
			MP_WritePhyUshort(sc, 0x06, 0xf87c);
			MP_WritePhyUshort(sc, 0x06, 0xe3f8);
			MP_WritePhyUshort(sc, 0x06, 0x7da5);
			MP_WritePhyUshort(sc, 0x06, 0x1111);
			MP_WritePhyUshort(sc, 0x06, 0x12d2);
			MP_WritePhyUshort(sc, 0x06, 0x40d6);
			MP_WritePhyUshort(sc, 0x06, 0x4444);
			MP_WritePhyUshort(sc, 0x06, 0x0281);
			MP_WritePhyUshort(sc, 0x06, 0xc6d2);
			MP_WritePhyUshort(sc, 0x06, 0xa0d6);
			MP_WritePhyUshort(sc, 0x06, 0xaaaa);
			MP_WritePhyUshort(sc, 0x06, 0x0281);
			MP_WritePhyUshort(sc, 0x06, 0xc6ae);
			MP_WritePhyUshort(sc, 0x06, 0x0fa5);
			MP_WritePhyUshort(sc, 0x06, 0x4444);
			MP_WritePhyUshort(sc, 0x06, 0x02ae);
			MP_WritePhyUshort(sc, 0x06, 0x4da5);
			MP_WritePhyUshort(sc, 0x06, 0xaaaa);
			MP_WritePhyUshort(sc, 0x06, 0x02ae);
			MP_WritePhyUshort(sc, 0x06, 0x47af);
			MP_WritePhyUshort(sc, 0x06, 0x81c2);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4e00);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4d0f);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4c0f);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4f00);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x5100);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4aff);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4bff);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x30e1);
			MP_WritePhyUshort(sc, 0x06, 0x8331);
			MP_WritePhyUshort(sc, 0x06, 0x58fe);
			MP_WritePhyUshort(sc, 0x06, 0xe4f8);
			MP_WritePhyUshort(sc, 0x06, 0x8ae5);
			MP_WritePhyUshort(sc, 0x06, 0xf88b);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x32e1);
			MP_WritePhyUshort(sc, 0x06, 0x8333);
			MP_WritePhyUshort(sc, 0x06, 0x590f);
			MP_WritePhyUshort(sc, 0x06, 0xe283);
			MP_WritePhyUshort(sc, 0x06, 0x4d0c);
			MP_WritePhyUshort(sc, 0x06, 0x245a);
			MP_WritePhyUshort(sc, 0x06, 0xf01e);
			MP_WritePhyUshort(sc, 0x06, 0x12e4);
			MP_WritePhyUshort(sc, 0x06, 0xf88c);
			MP_WritePhyUshort(sc, 0x06, 0xe5f8);
			MP_WritePhyUshort(sc, 0x06, 0x8daf);
			MP_WritePhyUshort(sc, 0x06, 0x81c2);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x4f10);
			MP_WritePhyUshort(sc, 0x06, 0xe483);
			MP_WritePhyUshort(sc, 0x06, 0x4fe0);
			MP_WritePhyUshort(sc, 0x06, 0x834e);
			MP_WritePhyUshort(sc, 0x06, 0x7800);
			MP_WritePhyUshort(sc, 0x06, 0x9f0a);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x4fa0);
			MP_WritePhyUshort(sc, 0x06, 0x10a5);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4e01);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x4e78);
			MP_WritePhyUshort(sc, 0x06, 0x059e);
			MP_WritePhyUshort(sc, 0x06, 0x9ae0);
			MP_WritePhyUshort(sc, 0x06, 0x834e);
			MP_WritePhyUshort(sc, 0x06, 0x7804);
			MP_WritePhyUshort(sc, 0x06, 0x9e10);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x4e78);
			MP_WritePhyUshort(sc, 0x06, 0x039e);
			MP_WritePhyUshort(sc, 0x06, 0x0fe0);
			MP_WritePhyUshort(sc, 0x06, 0x834e);
			MP_WritePhyUshort(sc, 0x06, 0x7801);
			MP_WritePhyUshort(sc, 0x06, 0x9e05);
			MP_WritePhyUshort(sc, 0x06, 0xae0c);
			MP_WritePhyUshort(sc, 0x06, 0xaf81);
			MP_WritePhyUshort(sc, 0x06, 0xa7af);
			MP_WritePhyUshort(sc, 0x06, 0x8152);
			MP_WritePhyUshort(sc, 0x06, 0xaf81);
			MP_WritePhyUshort(sc, 0x06, 0x8baf);
			MP_WritePhyUshort(sc, 0x06, 0x81c2);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4800);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4900);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x5110);
			MP_WritePhyUshort(sc, 0x06, 0xe483);
			MP_WritePhyUshort(sc, 0x06, 0x5158);
			MP_WritePhyUshort(sc, 0x06, 0x019f);
			MP_WritePhyUshort(sc, 0x06, 0xead0);
			MP_WritePhyUshort(sc, 0x06, 0x00d1);
			MP_WritePhyUshort(sc, 0x06, 0x801f);
			MP_WritePhyUshort(sc, 0x06, 0x66e2);
			MP_WritePhyUshort(sc, 0x06, 0xf8ea);
			MP_WritePhyUshort(sc, 0x06, 0xe3f8);
			MP_WritePhyUshort(sc, 0x06, 0xeb5a);
			MP_WritePhyUshort(sc, 0x06, 0xf81e);
			MP_WritePhyUshort(sc, 0x06, 0x20e6);
			MP_WritePhyUshort(sc, 0x06, 0xf8ea);
			MP_WritePhyUshort(sc, 0x06, 0xe5f8);
			MP_WritePhyUshort(sc, 0x06, 0xebd3);
			MP_WritePhyUshort(sc, 0x06, 0x02b3);
			MP_WritePhyUshort(sc, 0x06, 0xfee2);
			MP_WritePhyUshort(sc, 0x06, 0xf87c);
			MP_WritePhyUshort(sc, 0x06, 0xef32);
			MP_WritePhyUshort(sc, 0x06, 0x5b80);
			MP_WritePhyUshort(sc, 0x06, 0xe3f8);
			MP_WritePhyUshort(sc, 0x06, 0x7d9e);
			MP_WritePhyUshort(sc, 0x06, 0x037d);
			MP_WritePhyUshort(sc, 0x06, 0xffff);
			MP_WritePhyUshort(sc, 0x06, 0x0d58);
			MP_WritePhyUshort(sc, 0x06, 0x1c55);
			MP_WritePhyUshort(sc, 0x06, 0x1a65);
			MP_WritePhyUshort(sc, 0x06, 0x11a1);
			MP_WritePhyUshort(sc, 0x06, 0x90d3);
			MP_WritePhyUshort(sc, 0x06, 0xe283);
			MP_WritePhyUshort(sc, 0x06, 0x48e3);
			MP_WritePhyUshort(sc, 0x06, 0x8349);
			MP_WritePhyUshort(sc, 0x06, 0x1b56);
			MP_WritePhyUshort(sc, 0x06, 0xab08);
			MP_WritePhyUshort(sc, 0x06, 0xef56);
			MP_WritePhyUshort(sc, 0x06, 0xe683);
			MP_WritePhyUshort(sc, 0x06, 0x48e7);
			MP_WritePhyUshort(sc, 0x06, 0x8349);
			MP_WritePhyUshort(sc, 0x06, 0x10d1);
			MP_WritePhyUshort(sc, 0x06, 0x801f);
			MP_WritePhyUshort(sc, 0x06, 0x66a0);
			MP_WritePhyUshort(sc, 0x06, 0x04b9);
			MP_WritePhyUshort(sc, 0x06, 0xe283);
			MP_WritePhyUshort(sc, 0x06, 0x48e3);
			MP_WritePhyUshort(sc, 0x06, 0x8349);
			MP_WritePhyUshort(sc, 0x06, 0xef65);
			MP_WritePhyUshort(sc, 0x06, 0xe283);
			MP_WritePhyUshort(sc, 0x06, 0x4ae3);
			MP_WritePhyUshort(sc, 0x06, 0x834b);
			MP_WritePhyUshort(sc, 0x06, 0x1b56);
			MP_WritePhyUshort(sc, 0x06, 0xaa0e);
			MP_WritePhyUshort(sc, 0x06, 0xef56);
			MP_WritePhyUshort(sc, 0x06, 0xe683);
			MP_WritePhyUshort(sc, 0x06, 0x4ae7);
			MP_WritePhyUshort(sc, 0x06, 0x834b);
			MP_WritePhyUshort(sc, 0x06, 0xe283);
			MP_WritePhyUshort(sc, 0x06, 0x4de6);
			MP_WritePhyUshort(sc, 0x06, 0x834c);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x4da0);
			MP_WritePhyUshort(sc, 0x06, 0x000c);
			MP_WritePhyUshort(sc, 0x06, 0xaf81);
			MP_WritePhyUshort(sc, 0x06, 0x8be0);
			MP_WritePhyUshort(sc, 0x06, 0x834d);
			MP_WritePhyUshort(sc, 0x06, 0x10e4);
			MP_WritePhyUshort(sc, 0x06, 0x834d);
			MP_WritePhyUshort(sc, 0x06, 0xae04);
			MP_WritePhyUshort(sc, 0x06, 0x80e4);
			MP_WritePhyUshort(sc, 0x06, 0x834d);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x4e78);
			MP_WritePhyUshort(sc, 0x06, 0x039e);
			MP_WritePhyUshort(sc, 0x06, 0x0be0);
			MP_WritePhyUshort(sc, 0x06, 0x834e);
			MP_WritePhyUshort(sc, 0x06, 0x7804);
			MP_WritePhyUshort(sc, 0x06, 0x9e04);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4e02);
			MP_WritePhyUshort(sc, 0x06, 0xe083);
			MP_WritePhyUshort(sc, 0x06, 0x32e1);
			MP_WritePhyUshort(sc, 0x06, 0x8333);
			MP_WritePhyUshort(sc, 0x06, 0x590f);
			MP_WritePhyUshort(sc, 0x06, 0xe283);
			MP_WritePhyUshort(sc, 0x06, 0x4d0c);
			MP_WritePhyUshort(sc, 0x06, 0x245a);
			MP_WritePhyUshort(sc, 0x06, 0xf01e);
			MP_WritePhyUshort(sc, 0x06, 0x12e4);
			MP_WritePhyUshort(sc, 0x06, 0xf88c);
			MP_WritePhyUshort(sc, 0x06, 0xe5f8);
			MP_WritePhyUshort(sc, 0x06, 0x8de0);
			MP_WritePhyUshort(sc, 0x06, 0x8330);
			MP_WritePhyUshort(sc, 0x06, 0xe183);
			MP_WritePhyUshort(sc, 0x06, 0x3168);
			MP_WritePhyUshort(sc, 0x06, 0x01e4);
			MP_WritePhyUshort(sc, 0x06, 0xf88a);
			MP_WritePhyUshort(sc, 0x06, 0xe5f8);
			MP_WritePhyUshort(sc, 0x06, 0x8bae);
			MP_WritePhyUshort(sc, 0x06, 0x37ee);
			MP_WritePhyUshort(sc, 0x06, 0x834e);
			MP_WritePhyUshort(sc, 0x06, 0x03e0);
			MP_WritePhyUshort(sc, 0x06, 0x834c);
			MP_WritePhyUshort(sc, 0x06, 0xe183);
			MP_WritePhyUshort(sc, 0x06, 0x4d1b);
			MP_WritePhyUshort(sc, 0x06, 0x019e);
			MP_WritePhyUshort(sc, 0x06, 0x04aa);
			MP_WritePhyUshort(sc, 0x06, 0xa1ae);
			MP_WritePhyUshort(sc, 0x06, 0xa8ee);
			MP_WritePhyUshort(sc, 0x06, 0x834e);
			MP_WritePhyUshort(sc, 0x06, 0x04ee);
			MP_WritePhyUshort(sc, 0x06, 0x834f);
			MP_WritePhyUshort(sc, 0x06, 0x00ae);
			MP_WritePhyUshort(sc, 0x06, 0xabe0);
			MP_WritePhyUshort(sc, 0x06, 0x834f);
			MP_WritePhyUshort(sc, 0x06, 0x7803);
			MP_WritePhyUshort(sc, 0x06, 0x9f14);
			MP_WritePhyUshort(sc, 0x06, 0xee83);
			MP_WritePhyUshort(sc, 0x06, 0x4e05);
			MP_WritePhyUshort(sc, 0x06, 0xd240);
			MP_WritePhyUshort(sc, 0x06, 0xd655);
			MP_WritePhyUshort(sc, 0x06, 0x5402);
			MP_WritePhyUshort(sc, 0x06, 0x81c6);
			MP_WritePhyUshort(sc, 0x06, 0xd2a0);
			MP_WritePhyUshort(sc, 0x06, 0xd6ba);
			MP_WritePhyUshort(sc, 0x06, 0x0002);
			MP_WritePhyUshort(sc, 0x06, 0x81c6);
			MP_WritePhyUshort(sc, 0x06, 0xfefd);
			MP_WritePhyUshort(sc, 0x06, 0xfc05);
			MP_WritePhyUshort(sc, 0x06, 0xf8e0);
			MP_WritePhyUshort(sc, 0x06, 0xf860);
			MP_WritePhyUshort(sc, 0x06, 0xe1f8);
			MP_WritePhyUshort(sc, 0x06, 0x6168);
			MP_WritePhyUshort(sc, 0x06, 0x02e4);
			MP_WritePhyUshort(sc, 0x06, 0xf860);
			MP_WritePhyUshort(sc, 0x06, 0xe5f8);
			MP_WritePhyUshort(sc, 0x06, 0x61e0);
			MP_WritePhyUshort(sc, 0x06, 0xf848);
			MP_WritePhyUshort(sc, 0x06, 0xe1f8);
			MP_WritePhyUshort(sc, 0x06, 0x4958);
			MP_WritePhyUshort(sc, 0x06, 0x0f1e);
			MP_WritePhyUshort(sc, 0x06, 0x02e4);
			MP_WritePhyUshort(sc, 0x06, 0xf848);
			MP_WritePhyUshort(sc, 0x06, 0xe5f8);
			MP_WritePhyUshort(sc, 0x06, 0x49d0);
			MP_WritePhyUshort(sc, 0x06, 0x0002);
			MP_WritePhyUshort(sc, 0x06, 0x820a);
			MP_WritePhyUshort(sc, 0x06, 0xbf83);
			MP_WritePhyUshort(sc, 0x06, 0x50ef);
			MP_WritePhyUshort(sc, 0x06, 0x46dc);
			MP_WritePhyUshort(sc, 0x06, 0x19dd);
			MP_WritePhyUshort(sc, 0x06, 0xd001);
			MP_WritePhyUshort(sc, 0x06, 0x0282);
			MP_WritePhyUshort(sc, 0x06, 0x0a02);
			MP_WritePhyUshort(sc, 0x06, 0x8226);
			MP_WritePhyUshort(sc, 0x06, 0xe0f8);
			MP_WritePhyUshort(sc, 0x06, 0x60e1);
			MP_WritePhyUshort(sc, 0x06, 0xf861);
			MP_WritePhyUshort(sc, 0x06, 0x58fd);
			MP_WritePhyUshort(sc, 0x06, 0xe4f8);
			MP_WritePhyUshort(sc, 0x06, 0x60e5);
			MP_WritePhyUshort(sc, 0x06, 0xf861);
			MP_WritePhyUshort(sc, 0x06, 0xfc04);
			MP_WritePhyUshort(sc, 0x06, 0xf9fa);
			MP_WritePhyUshort(sc, 0x06, 0xfbc6);
			MP_WritePhyUshort(sc, 0x06, 0xbff8);
			MP_WritePhyUshort(sc, 0x06, 0x40be);
			MP_WritePhyUshort(sc, 0x06, 0x8350);
			MP_WritePhyUshort(sc, 0x06, 0xa001);
			MP_WritePhyUshort(sc, 0x06, 0x0107);
			MP_WritePhyUshort(sc, 0x06, 0x1b89);
			MP_WritePhyUshort(sc, 0x06, 0xcfd2);
			MP_WritePhyUshort(sc, 0x06, 0x08eb);
			MP_WritePhyUshort(sc, 0x06, 0xdb19);
			MP_WritePhyUshort(sc, 0x06, 0xb2fb);
			MP_WritePhyUshort(sc, 0x06, 0xfffe);
			MP_WritePhyUshort(sc, 0x06, 0xfd04);
			MP_WritePhyUshort(sc, 0x06, 0xf8e0);
			MP_WritePhyUshort(sc, 0x06, 0xf848);
			MP_WritePhyUshort(sc, 0x06, 0xe1f8);
			MP_WritePhyUshort(sc, 0x06, 0x4968);
			MP_WritePhyUshort(sc, 0x06, 0x08e4);
			MP_WritePhyUshort(sc, 0x06, 0xf848);
			MP_WritePhyUshort(sc, 0x06, 0xe5f8);
			MP_WritePhyUshort(sc, 0x06, 0x4958);
			MP_WritePhyUshort(sc, 0x06, 0xf7e4);
			MP_WritePhyUshort(sc, 0x06, 0xf848);
			MP_WritePhyUshort(sc, 0x06, 0xe5f8);
			MP_WritePhyUshort(sc, 0x06, 0x49fc);
			MP_WritePhyUshort(sc, 0x06, 0x044d);
			MP_WritePhyUshort(sc, 0x06, 0x2000);
			MP_WritePhyUshort(sc, 0x06, 0x024e);
			MP_WritePhyUshort(sc, 0x06, 0x2200);
			MP_WritePhyUshort(sc, 0x06, 0x024d);
			MP_WritePhyUshort(sc, 0x06, 0xdfff);
			MP_WritePhyUshort(sc, 0x06, 0x014e);
			MP_WritePhyUshort(sc, 0x06, 0xddff);
			MP_WritePhyUshort(sc, 0x06, 0x01f8);
			MP_WritePhyUshort(sc, 0x06, 0xfafb);
			MP_WritePhyUshort(sc, 0x06, 0xef79);
			MP_WritePhyUshort(sc, 0x06, 0xbff8);
			MP_WritePhyUshort(sc, 0x06, 0x22d8);
			MP_WritePhyUshort(sc, 0x06, 0x19d9);
			MP_WritePhyUshort(sc, 0x06, 0x5884);
			MP_WritePhyUshort(sc, 0x06, 0x9f09);
			MP_WritePhyUshort(sc, 0x06, 0xbf82);
			MP_WritePhyUshort(sc, 0x06, 0x6dd6);
			MP_WritePhyUshort(sc, 0x06, 0x8275);
			MP_WritePhyUshort(sc, 0x06, 0x0201);
			MP_WritePhyUshort(sc, 0x06, 0x4fef);
			MP_WritePhyUshort(sc, 0x06, 0x97ff);
			MP_WritePhyUshort(sc, 0x06, 0xfefc);
			MP_WritePhyUshort(sc, 0x06, 0x0517);
			MP_WritePhyUshort(sc, 0x06, 0xfffe);
			MP_WritePhyUshort(sc, 0x06, 0x0117);
			MP_WritePhyUshort(sc, 0x06, 0x0001);
			MP_WritePhyUshort(sc, 0x06, 0x0200);
			MP_WritePhyUshort(sc, 0x05, 0x83d8);
			MP_WritePhyUshort(sc, 0x06, 0x8000);
			MP_WritePhyUshort(sc, 0x05, 0x83d6);
			MP_WritePhyUshort(sc, 0x06, 0x824f);
			MP_WritePhyUshort(sc, 0x02, 0x2010);
			MP_WritePhyUshort(sc, 0x03, 0xdc00);
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
			MP_WritePhyUshort(sc, 0x0b, 0x0600);
			MP_WritePhyUshort(sc, 0x1f, 0x0005);
			MP_WritePhyUshort(sc, 0x05, 0xfff6);
			MP_WritePhyUshort(sc, 0x06, 0x00fc);
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
		}

		MP_WritePhyUshort(sc, 0x1F, 0x0000);
		MP_WritePhyUshort(sc, 0x0D, 0xF880);
		MP_WritePhyUshort(sc, 0x1F, 0x0000);
	} else if (sc->rg_type == MACFG_36 || sc->rg_type == MACFG_37) {
		CSR_WRITE_1(sc, 0xF3, CSR_READ_1(sc, 0xF3)|0x04);

		if(sc->rg_type == MACFG_36)
		{
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
			MP_WritePhyUshort(sc, 0x00, 0x1800);
			MP_WritePhyUshort(sc, 0x1f, 0x0007);
			MP_WritePhyUshort(sc, 0x1e, 0x0023);
			MP_WritePhyUshort(sc, 0x17, 0x0117);
			MP_WritePhyUshort(sc, 0x1f, 0x0007);
			MP_WritePhyUshort(sc, 0x1E, 0x002C);
			MP_WritePhyUshort(sc, 0x1B, 0x5000);
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
			MP_WritePhyUshort(sc, 0x16, 0x4104);
			for(i=0;i<200;i++)
			{
				DELAY(100);
				Data = MP_ReadPhyUshort(sc, 0x1E);
				Data &= 0x03FF;
				if(Data==0x000C)
					break;
			}
			MP_WritePhyUshort(sc, 0x1f, 0x0005);
			for(i=0;i<200;i++)
			{
				DELAY(100);
				Data = MP_ReadPhyUshort(sc, 0x07);
				if((Data & 0x0020)==0)
					break;
			}
			Data = MP_ReadPhyUshort(sc, 0x07);
			if(Data & 0x0020)
			{
				MP_WritePhyUshort(sc, 0x1f, 0x0007);
				MP_WritePhyUshort(sc, 0x1e, 0x00a1);
				MP_WritePhyUshort(sc, 0x17, 0x1000);
				MP_WritePhyUshort(sc, 0x17, 0x0000);
				MP_WritePhyUshort(sc, 0x17, 0x2000);
				MP_WritePhyUshort(sc, 0x1e, 0x002f);
				MP_WritePhyUshort(sc, 0x18, 0x9bfb);
				MP_WritePhyUshort(sc, 0x1f, 0x0005);
				MP_WritePhyUshort(sc, 0x07, 0x0000);
				MP_WritePhyUshort(sc, 0x1f, 0x0000);
			}
			MP_WritePhyUshort(sc, 0x1f, 0x0005);
			MP_WritePhyUshort(sc, 0x05, 0xfff6);
			MP_WritePhyUshort(sc, 0x06, 0x0080);
			Data = MP_ReadPhyUshort(sc, 0x00);
			Data &= ~(0x0080);
			MP_WritePhyUshort(sc, 0x00, Data);
			MP_WritePhyUshort(sc, 0x1f, 0x0002);
			Data = MP_ReadPhyUshort(sc, 0x08);
			Data &= ~(0x0080);
			MP_WritePhyUshort(sc, 0x08, Data);
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
			MP_WritePhyUshort(sc, 0x1f, 0x0007);
			MP_WritePhyUshort(sc, 0x1e, 0x0023);
			MP_WritePhyUshort(sc, 0x16, 0x0306);
			MP_WritePhyUshort(sc, 0x16, 0x0307);
			MP_WritePhyUshort(sc, 0x15, 0x000e);
			MP_WritePhyUshort(sc, 0x19, 0x000a);
			MP_WritePhyUshort(sc, 0x15, 0x0010);
			MP_WritePhyUshort(sc, 0x19, 0x0008);
			MP_WritePhyUshort(sc, 0x15, 0x0018);
			MP_WritePhyUshort(sc, 0x19, 0x4801);
			MP_WritePhyUshort(sc, 0x15, 0x0019);
			MP_WritePhyUshort(sc, 0x19, 0x6801);
			MP_WritePhyUshort(sc, 0x15, 0x001a);
			MP_WritePhyUshort(sc, 0x19, 0x66a1);
			MP_WritePhyUshort(sc, 0x15, 0x001f);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0020);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0021);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0022);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0023);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0024);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0025);
			MP_WritePhyUshort(sc, 0x19, 0x64a1);
			MP_WritePhyUshort(sc, 0x15, 0x0026);
			MP_WritePhyUshort(sc, 0x19, 0x40ea);
			MP_WritePhyUshort(sc, 0x15, 0x0027);
			MP_WritePhyUshort(sc, 0x19, 0x4503);
			MP_WritePhyUshort(sc, 0x15, 0x0028);
			MP_WritePhyUshort(sc, 0x19, 0x9f00);
			MP_WritePhyUshort(sc, 0x15, 0x0029);
			MP_WritePhyUshort(sc, 0x19, 0xa631);
			MP_WritePhyUshort(sc, 0x15, 0x002a);
			MP_WritePhyUshort(sc, 0x19, 0x9717);
			MP_WritePhyUshort(sc, 0x15, 0x002b);
			MP_WritePhyUshort(sc, 0x19, 0x302c);
			MP_WritePhyUshort(sc, 0x15, 0x002c);
			MP_WritePhyUshort(sc, 0x19, 0x4802);
			MP_WritePhyUshort(sc, 0x15, 0x002d);
			MP_WritePhyUshort(sc, 0x19, 0x58da);
			MP_WritePhyUshort(sc, 0x15, 0x002e);
			MP_WritePhyUshort(sc, 0x19, 0x400d);
			MP_WritePhyUshort(sc, 0x15, 0x002f);
			MP_WritePhyUshort(sc, 0x19, 0x4488);
			MP_WritePhyUshort(sc, 0x15, 0x0030);
			MP_WritePhyUshort(sc, 0x19, 0x9e00);
			MP_WritePhyUshort(sc, 0x15, 0x0031);
			MP_WritePhyUshort(sc, 0x19, 0x63c8);
			MP_WritePhyUshort(sc, 0x15, 0x0032);
			MP_WritePhyUshort(sc, 0x19, 0x6481);
			MP_WritePhyUshort(sc, 0x15, 0x0033);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0034);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0035);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0036);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0037);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0038);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0039);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x003a);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x003b);
			MP_WritePhyUshort(sc, 0x19, 0x63e8);
			MP_WritePhyUshort(sc, 0x15, 0x003c);
			MP_WritePhyUshort(sc, 0x19, 0x7d00);
			MP_WritePhyUshort(sc, 0x15, 0x003d);
			MP_WritePhyUshort(sc, 0x19, 0x59d4);
			MP_WritePhyUshort(sc, 0x15, 0x003e);
			MP_WritePhyUshort(sc, 0x19, 0x63f8);
			MP_WritePhyUshort(sc, 0x15, 0x0040);
			MP_WritePhyUshort(sc, 0x19, 0x64a1);
			MP_WritePhyUshort(sc, 0x15, 0x0041);
			MP_WritePhyUshort(sc, 0x19, 0x30de);
			MP_WritePhyUshort(sc, 0x15, 0x0044);
			MP_WritePhyUshort(sc, 0x19, 0x480f);
			MP_WritePhyUshort(sc, 0x15, 0x0045);
			MP_WritePhyUshort(sc, 0x19, 0x6800);
			MP_WritePhyUshort(sc, 0x15, 0x0046);
			MP_WritePhyUshort(sc, 0x19, 0x6680);
			MP_WritePhyUshort(sc, 0x15, 0x0047);
			MP_WritePhyUshort(sc, 0x19, 0x7c10);
			MP_WritePhyUshort(sc, 0x15, 0x0048);
			MP_WritePhyUshort(sc, 0x19, 0x63c8);
			MP_WritePhyUshort(sc, 0x15, 0x0049);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x004a);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x004b);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x004c);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x004d);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x004e);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x004f);
			MP_WritePhyUshort(sc, 0x19, 0x40ea);
			MP_WritePhyUshort(sc, 0x15, 0x0050);
			MP_WritePhyUshort(sc, 0x19, 0x4503);
			MP_WritePhyUshort(sc, 0x15, 0x0051);
			MP_WritePhyUshort(sc, 0x19, 0x58ca);
			MP_WritePhyUshort(sc, 0x15, 0x0052);
			MP_WritePhyUshort(sc, 0x19, 0x63c8);
			MP_WritePhyUshort(sc, 0x15, 0x0053);
			MP_WritePhyUshort(sc, 0x19, 0x63d8);
			MP_WritePhyUshort(sc, 0x15, 0x0054);
			MP_WritePhyUshort(sc, 0x19, 0x66a0);
			MP_WritePhyUshort(sc, 0x15, 0x0055);
			MP_WritePhyUshort(sc, 0x19, 0x9f00);
			MP_WritePhyUshort(sc, 0x15, 0x0056);
			MP_WritePhyUshort(sc, 0x19, 0x3000);
			MP_WritePhyUshort(sc, 0x15, 0x006E);
			MP_WritePhyUshort(sc, 0x19, 0x9afa);
			MP_WritePhyUshort(sc, 0x15, 0x00a1);
			MP_WritePhyUshort(sc, 0x19, 0x3044);
			MP_WritePhyUshort(sc, 0x15, 0x00ab);
			MP_WritePhyUshort(sc, 0x19, 0x5820);
			MP_WritePhyUshort(sc, 0x15, 0x00ac);
			MP_WritePhyUshort(sc, 0x19, 0x5e04);
			MP_WritePhyUshort(sc, 0x15, 0x00ad);
			MP_WritePhyUshort(sc, 0x19, 0xb60c);
			MP_WritePhyUshort(sc, 0x15, 0x00af);
			MP_WritePhyUshort(sc, 0x19, 0x000a);
			MP_WritePhyUshort(sc, 0x15, 0x00b2);
			MP_WritePhyUshort(sc, 0x19, 0x30b9);
			MP_WritePhyUshort(sc, 0x15, 0x00b9);
			MP_WritePhyUshort(sc, 0x19, 0x4408);
			MP_WritePhyUshort(sc, 0x15, 0x00ba);
			MP_WritePhyUshort(sc, 0x19, 0x480b);
			MP_WritePhyUshort(sc, 0x15, 0x00bb);
			MP_WritePhyUshort(sc, 0x19, 0x5e00);
			MP_WritePhyUshort(sc, 0x15, 0x00bc);
			MP_WritePhyUshort(sc, 0x19, 0x405f);
			MP_WritePhyUshort(sc, 0x15, 0x00bd);
			MP_WritePhyUshort(sc, 0x19, 0x4448);
			MP_WritePhyUshort(sc, 0x15, 0x00be);
			MP_WritePhyUshort(sc, 0x19, 0x4020);
			MP_WritePhyUshort(sc, 0x15, 0x00bf);
			MP_WritePhyUshort(sc, 0x19, 0x4468);
			MP_WritePhyUshort(sc, 0x15, 0x00c0);
			MP_WritePhyUshort(sc, 0x19, 0x9c02);
			MP_WritePhyUshort(sc, 0x15, 0x00c1);
			MP_WritePhyUshort(sc, 0x19, 0x58a0);
			MP_WritePhyUshort(sc, 0x15, 0x00c2);
			MP_WritePhyUshort(sc, 0x19, 0xb605);
			MP_WritePhyUshort(sc, 0x15, 0x00c3);
			MP_WritePhyUshort(sc, 0x19, 0xc0d3);
			MP_WritePhyUshort(sc, 0x15, 0x00c4);
			MP_WritePhyUshort(sc, 0x19, 0x00e6);
			MP_WritePhyUshort(sc, 0x15, 0x00c5);
			MP_WritePhyUshort(sc, 0x19, 0xdaec);
			MP_WritePhyUshort(sc, 0x15, 0x00c6);
			MP_WritePhyUshort(sc, 0x19, 0x00fa);
			MP_WritePhyUshort(sc, 0x15, 0x00c7);
			MP_WritePhyUshort(sc, 0x19, 0x9df9);
			MP_WritePhyUshort(sc, 0x15, 0x00c8);
			MP_WritePhyUshort(sc, 0x19, 0x307a);
			MP_WritePhyUshort(sc, 0x15, 0x0112);
			MP_WritePhyUshort(sc, 0x19, 0x6421);
			MP_WritePhyUshort(sc, 0x15, 0x0113);
			MP_WritePhyUshort(sc, 0x19, 0x7c08);
			MP_WritePhyUshort(sc, 0x15, 0x0114);
			MP_WritePhyUshort(sc, 0x19, 0x63f0);
			MP_WritePhyUshort(sc, 0x15, 0x0115);
			MP_WritePhyUshort(sc, 0x19, 0x4003);
			MP_WritePhyUshort(sc, 0x15, 0x0116);
			MP_WritePhyUshort(sc, 0x19, 0x4418);
			MP_WritePhyUshort(sc, 0x15, 0x0117);
			MP_WritePhyUshort(sc, 0x19, 0x9b00);
			MP_WritePhyUshort(sc, 0x15, 0x0118);
			MP_WritePhyUshort(sc, 0x19, 0x6461);
			MP_WritePhyUshort(sc, 0x15, 0x0119);
			MP_WritePhyUshort(sc, 0x19, 0x64e1);
			MP_WritePhyUshort(sc, 0x15, 0x011a);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0150);
			MP_WritePhyUshort(sc, 0x19, 0x6461);
			MP_WritePhyUshort(sc, 0x15, 0x0151);
			MP_WritePhyUshort(sc, 0x19, 0x4003);
			MP_WritePhyUshort(sc, 0x15, 0x0152);
			MP_WritePhyUshort(sc, 0x19, 0x4540);
			MP_WritePhyUshort(sc, 0x15, 0x0153);
			MP_WritePhyUshort(sc, 0x19, 0x9f00);
			MP_WritePhyUshort(sc, 0x15, 0x0155);
			MP_WritePhyUshort(sc, 0x19, 0x6421);
			MP_WritePhyUshort(sc, 0x15, 0x0156);
			MP_WritePhyUshort(sc, 0x19, 0x64a1);
			MP_WritePhyUshort(sc, 0x15, 0x021e);
			MP_WritePhyUshort(sc, 0x19, 0x5410);
			MP_WritePhyUshort(sc, 0x15, 0x0225);
			MP_WritePhyUshort(sc, 0x19, 0x5400);
			MP_WritePhyUshort(sc, 0x15, 0x023D);
			MP_WritePhyUshort(sc, 0x19, 0x4050);
			MP_WritePhyUshort(sc, 0x15, 0x0295);
			MP_WritePhyUshort(sc, 0x19, 0x6c08);
			MP_WritePhyUshort(sc, 0x15, 0x02bd);
			MP_WritePhyUshort(sc, 0x19, 0xa523);
			MP_WritePhyUshort(sc, 0x15, 0x02be);
			MP_WritePhyUshort(sc, 0x19, 0x32ca);
			MP_WritePhyUshort(sc, 0x15, 0x02ca);
			MP_WritePhyUshort(sc, 0x19, 0x48b3);
			MP_WritePhyUshort(sc, 0x15, 0x02cb);
			MP_WritePhyUshort(sc, 0x19, 0x4020);
			MP_WritePhyUshort(sc, 0x15, 0x02cc);
			MP_WritePhyUshort(sc, 0x19, 0x4823);
			MP_WritePhyUshort(sc, 0x15, 0x02cd);
			MP_WritePhyUshort(sc, 0x19, 0x4510);
			MP_WritePhyUshort(sc, 0x15, 0x02ce);
			MP_WritePhyUshort(sc, 0x19, 0xb63a);
			MP_WritePhyUshort(sc, 0x15, 0x02cf);
			MP_WritePhyUshort(sc, 0x19, 0x7dc8);
			MP_WritePhyUshort(sc, 0x15, 0x02d6);
			MP_WritePhyUshort(sc, 0x19, 0x9bf8);
			MP_WritePhyUshort(sc, 0x15, 0x02d8);
			MP_WritePhyUshort(sc, 0x19, 0x85f6);
			MP_WritePhyUshort(sc, 0x15, 0x02d9);
			MP_WritePhyUshort(sc, 0x19, 0x32e0);
			MP_WritePhyUshort(sc, 0x15, 0x02e0);
			MP_WritePhyUshort(sc, 0x19, 0x4834);
			MP_WritePhyUshort(sc, 0x15, 0x02e1);
			MP_WritePhyUshort(sc, 0x19, 0x6c08);
			MP_WritePhyUshort(sc, 0x15, 0x02e2);
			MP_WritePhyUshort(sc, 0x19, 0x4020);
			MP_WritePhyUshort(sc, 0x15, 0x02e3);
			MP_WritePhyUshort(sc, 0x19, 0x4824);
			MP_WritePhyUshort(sc, 0x15, 0x02e4);
			MP_WritePhyUshort(sc, 0x19, 0x4520);
			MP_WritePhyUshort(sc, 0x15, 0x02e5);
			MP_WritePhyUshort(sc, 0x19, 0x4008);
			MP_WritePhyUshort(sc, 0x15, 0x02e6);
			MP_WritePhyUshort(sc, 0x19, 0x4560);
			MP_WritePhyUshort(sc, 0x15, 0x02e7);
			MP_WritePhyUshort(sc, 0x19, 0x9d04);
			MP_WritePhyUshort(sc, 0x15, 0x02e8);
			MP_WritePhyUshort(sc, 0x19, 0x48c4);
			MP_WritePhyUshort(sc, 0x15, 0x02e9);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x02ea);
			MP_WritePhyUshort(sc, 0x19, 0x4844);
			MP_WritePhyUshort(sc, 0x15, 0x02eb);
			MP_WritePhyUshort(sc, 0x19, 0x7dc8);
			MP_WritePhyUshort(sc, 0x15, 0x02f0);
			MP_WritePhyUshort(sc, 0x19, 0x9cf7);
			MP_WritePhyUshort(sc, 0x15, 0x02f1);
			MP_WritePhyUshort(sc, 0x19, 0xdf94);
			MP_WritePhyUshort(sc, 0x15, 0x02f2);
			MP_WritePhyUshort(sc, 0x19, 0x0002);
			MP_WritePhyUshort(sc, 0x15, 0x02f3);
			MP_WritePhyUshort(sc, 0x19, 0x6810);
			MP_WritePhyUshort(sc, 0x15, 0x02f4);
			MP_WritePhyUshort(sc, 0x19, 0xb614);
			MP_WritePhyUshort(sc, 0x15, 0x02f5);
			MP_WritePhyUshort(sc, 0x19, 0xc42b);
			MP_WritePhyUshort(sc, 0x15, 0x02f6);
			MP_WritePhyUshort(sc, 0x19, 0x00d4);
			MP_WritePhyUshort(sc, 0x15, 0x02f7);
			MP_WritePhyUshort(sc, 0x19, 0xc455);
			MP_WritePhyUshort(sc, 0x15, 0x02f8);
			MP_WritePhyUshort(sc, 0x19, 0x0093);
			MP_WritePhyUshort(sc, 0x15, 0x02f9);
			MP_WritePhyUshort(sc, 0x19, 0x92ee);
			MP_WritePhyUshort(sc, 0x15, 0x02fa);
			MP_WritePhyUshort(sc, 0x19, 0xefed);
			MP_WritePhyUshort(sc, 0x15, 0x02fb);
			MP_WritePhyUshort(sc, 0x19, 0x3312);
			MP_WritePhyUshort(sc, 0x15, 0x0312);
			MP_WritePhyUshort(sc, 0x19, 0x49b5);
			MP_WritePhyUshort(sc, 0x15, 0x0313);
			MP_WritePhyUshort(sc, 0x19, 0x7d00);
			MP_WritePhyUshort(sc, 0x15, 0x0314);
			MP_WritePhyUshort(sc, 0x19, 0x4d00);
			MP_WritePhyUshort(sc, 0x15, 0x0315);
			MP_WritePhyUshort(sc, 0x19, 0x6810);
			MP_WritePhyUshort(sc, 0x15, 0x031e);
			MP_WritePhyUshort(sc, 0x19, 0x404f);
			MP_WritePhyUshort(sc, 0x15, 0x031f);
			MP_WritePhyUshort(sc, 0x19, 0x44c8);
			MP_WritePhyUshort(sc, 0x15, 0x0320);
			MP_WritePhyUshort(sc, 0x19, 0xd64f);
			MP_WritePhyUshort(sc, 0x15, 0x0321);
			MP_WritePhyUshort(sc, 0x19, 0x00e7);
			MP_WritePhyUshort(sc, 0x15, 0x0322);
			MP_WritePhyUshort(sc, 0x19, 0x7c08);
			MP_WritePhyUshort(sc, 0x15, 0x0323);
			MP_WritePhyUshort(sc, 0x19, 0x8203);
			MP_WritePhyUshort(sc, 0x15, 0x0324);
			MP_WritePhyUshort(sc, 0x19, 0x4d48);
			MP_WritePhyUshort(sc, 0x15, 0x0325);
			MP_WritePhyUshort(sc, 0x19, 0x3327);
			MP_WritePhyUshort(sc, 0x15, 0x0326);
			MP_WritePhyUshort(sc, 0x19, 0x4d40);
			MP_WritePhyUshort(sc, 0x15, 0x0327);
			MP_WritePhyUshort(sc, 0x19, 0xc8d7);
			MP_WritePhyUshort(sc, 0x15, 0x0328);
			MP_WritePhyUshort(sc, 0x19, 0x0003);
			MP_WritePhyUshort(sc, 0x15, 0x0329);
			MP_WritePhyUshort(sc, 0x19, 0x7c20);
			MP_WritePhyUshort(sc, 0x15, 0x032a);
			MP_WritePhyUshort(sc, 0x19, 0x4c20);
			MP_WritePhyUshort(sc, 0x15, 0x032b);
			MP_WritePhyUshort(sc, 0x19, 0xc8ed);
			MP_WritePhyUshort(sc, 0x15, 0x032c);
			MP_WritePhyUshort(sc, 0x19, 0x00f4);
			MP_WritePhyUshort(sc, 0x15, 0x032d);
			MP_WritePhyUshort(sc, 0x19, 0x82b3);
			MP_WritePhyUshort(sc, 0x15, 0x032e);
			MP_WritePhyUshort(sc, 0x19, 0xd11d);
			MP_WritePhyUshort(sc, 0x15, 0x032f);
			MP_WritePhyUshort(sc, 0x19, 0x00b1);
			MP_WritePhyUshort(sc, 0x15, 0x0330);
			MP_WritePhyUshort(sc, 0x19, 0xde18);
			MP_WritePhyUshort(sc, 0x15, 0x0331);
			MP_WritePhyUshort(sc, 0x19, 0x0008);
			MP_WritePhyUshort(sc, 0x15, 0x0332);
			MP_WritePhyUshort(sc, 0x19, 0x91ee);
			MP_WritePhyUshort(sc, 0x15, 0x0333);
			MP_WritePhyUshort(sc, 0x19, 0x3339);
			MP_WritePhyUshort(sc, 0x15, 0x033a);
			MP_WritePhyUshort(sc, 0x19, 0x4064);
			MP_WritePhyUshort(sc, 0x15, 0x0340);
			MP_WritePhyUshort(sc, 0x19, 0x9e06);
			MP_WritePhyUshort(sc, 0x15, 0x0341);
			MP_WritePhyUshort(sc, 0x19, 0x7c08);
			MP_WritePhyUshort(sc, 0x15, 0x0342);
			MP_WritePhyUshort(sc, 0x19, 0x8203);
			MP_WritePhyUshort(sc, 0x15, 0x0343);
			MP_WritePhyUshort(sc, 0x19, 0x4d48);
			MP_WritePhyUshort(sc, 0x15, 0x0344);
			MP_WritePhyUshort(sc, 0x19, 0x3346);
			MP_WritePhyUshort(sc, 0x15, 0x0345);
			MP_WritePhyUshort(sc, 0x19, 0x4d40);
			MP_WritePhyUshort(sc, 0x15, 0x0346);
			MP_WritePhyUshort(sc, 0x19, 0xd11d);
			MP_WritePhyUshort(sc, 0x15, 0x0347);
			MP_WritePhyUshort(sc, 0x19, 0x0099);
			MP_WritePhyUshort(sc, 0x15, 0x0348);
			MP_WritePhyUshort(sc, 0x19, 0xbb17);
			MP_WritePhyUshort(sc, 0x15, 0x0349);
			MP_WritePhyUshort(sc, 0x19, 0x8102);
			MP_WritePhyUshort(sc, 0x15, 0x034a);
			MP_WritePhyUshort(sc, 0x19, 0x334d);
			MP_WritePhyUshort(sc, 0x15, 0x034b);
			MP_WritePhyUshort(sc, 0x19, 0xa22c);
			MP_WritePhyUshort(sc, 0x15, 0x034c);
			MP_WritePhyUshort(sc, 0x19, 0x3397);
			MP_WritePhyUshort(sc, 0x15, 0x034d);
			MP_WritePhyUshort(sc, 0x19, 0x91f2);
			MP_WritePhyUshort(sc, 0x15, 0x034e);
			MP_WritePhyUshort(sc, 0x19, 0xc218);
			MP_WritePhyUshort(sc, 0x15, 0x034f);
			MP_WritePhyUshort(sc, 0x19, 0x00f0);
			MP_WritePhyUshort(sc, 0x15, 0x0350);
			MP_WritePhyUshort(sc, 0x19, 0x3397);
			MP_WritePhyUshort(sc, 0x15, 0x0351);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0364);
			MP_WritePhyUshort(sc, 0x19, 0xbc05);
			MP_WritePhyUshort(sc, 0x15, 0x0367);
			MP_WritePhyUshort(sc, 0x19, 0xa1fc);
			MP_WritePhyUshort(sc, 0x15, 0x0368);
			MP_WritePhyUshort(sc, 0x19, 0x3377);
			MP_WritePhyUshort(sc, 0x15, 0x0369);
			MP_WritePhyUshort(sc, 0x19, 0x328b);
			MP_WritePhyUshort(sc, 0x15, 0x036a);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0377);
			MP_WritePhyUshort(sc, 0x19, 0x4b97);
			MP_WritePhyUshort(sc, 0x15, 0x0378);
			MP_WritePhyUshort(sc, 0x19, 0x6818);
			MP_WritePhyUshort(sc, 0x15, 0x0379);
			MP_WritePhyUshort(sc, 0x19, 0x4b07);
			MP_WritePhyUshort(sc, 0x15, 0x037a);
			MP_WritePhyUshort(sc, 0x19, 0x40ac);
			MP_WritePhyUshort(sc, 0x15, 0x037b);
			MP_WritePhyUshort(sc, 0x19, 0x4445);
			MP_WritePhyUshort(sc, 0x15, 0x037c);
			MP_WritePhyUshort(sc, 0x19, 0x404e);
			MP_WritePhyUshort(sc, 0x15, 0x037d);
			MP_WritePhyUshort(sc, 0x19, 0x4461);
			MP_WritePhyUshort(sc, 0x15, 0x037e);
			MP_WritePhyUshort(sc, 0x19, 0x9c09);
			MP_WritePhyUshort(sc, 0x15, 0x037f);
			MP_WritePhyUshort(sc, 0x19, 0x63da);
			MP_WritePhyUshort(sc, 0x15, 0x0380);
			MP_WritePhyUshort(sc, 0x19, 0x5440);
			MP_WritePhyUshort(sc, 0x15, 0x0381);
			MP_WritePhyUshort(sc, 0x19, 0x4b98);
			MP_WritePhyUshort(sc, 0x15, 0x0382);
			MP_WritePhyUshort(sc, 0x19, 0x7c60);
			MP_WritePhyUshort(sc, 0x15, 0x0383);
			MP_WritePhyUshort(sc, 0x19, 0x4c00);
			MP_WritePhyUshort(sc, 0x15, 0x0384);
			MP_WritePhyUshort(sc, 0x19, 0x4b08);
			MP_WritePhyUshort(sc, 0x15, 0x0385);
			MP_WritePhyUshort(sc, 0x19, 0x63d8);
			MP_WritePhyUshort(sc, 0x15, 0x0386);
			MP_WritePhyUshort(sc, 0x19, 0x338d);
			MP_WritePhyUshort(sc, 0x15, 0x0387);
			MP_WritePhyUshort(sc, 0x19, 0xd64f);
			MP_WritePhyUshort(sc, 0x15, 0x0388);
			MP_WritePhyUshort(sc, 0x19, 0x0080);
			MP_WritePhyUshort(sc, 0x15, 0x0389);
			MP_WritePhyUshort(sc, 0x19, 0x820c);
			MP_WritePhyUshort(sc, 0x15, 0x038a);
			MP_WritePhyUshort(sc, 0x19, 0xa10b);
			MP_WritePhyUshort(sc, 0x15, 0x038b);
			MP_WritePhyUshort(sc, 0x19, 0x9df3);
			MP_WritePhyUshort(sc, 0x15, 0x038c);
			MP_WritePhyUshort(sc, 0x19, 0x3395);
			MP_WritePhyUshort(sc, 0x15, 0x038d);
			MP_WritePhyUshort(sc, 0x19, 0xd64f);
			MP_WritePhyUshort(sc, 0x15, 0x038e);
			MP_WritePhyUshort(sc, 0x19, 0x00f9);
			MP_WritePhyUshort(sc, 0x15, 0x038f);
			MP_WritePhyUshort(sc, 0x19, 0xc017);
			MP_WritePhyUshort(sc, 0x15, 0x0390);
			MP_WritePhyUshort(sc, 0x19, 0x0005);
			MP_WritePhyUshort(sc, 0x15, 0x0391);
			MP_WritePhyUshort(sc, 0x19, 0x6c0b);
			MP_WritePhyUshort(sc, 0x15, 0x0392);
			MP_WritePhyUshort(sc, 0x19, 0xa103);
			MP_WritePhyUshort(sc, 0x15, 0x0393);
			MP_WritePhyUshort(sc, 0x19, 0x6c08);
			MP_WritePhyUshort(sc, 0x15, 0x0394);
			MP_WritePhyUshort(sc, 0x19, 0x9df9);
			MP_WritePhyUshort(sc, 0x15, 0x0395);
			MP_WritePhyUshort(sc, 0x19, 0x6c08);
			MP_WritePhyUshort(sc, 0x15, 0x0396);
			MP_WritePhyUshort(sc, 0x19, 0x3397);
			MP_WritePhyUshort(sc, 0x15, 0x0399);
			MP_WritePhyUshort(sc, 0x19, 0x6810);
			MP_WritePhyUshort(sc, 0x15, 0x03a4);
			MP_WritePhyUshort(sc, 0x19, 0x7c08);
			MP_WritePhyUshort(sc, 0x15, 0x03a5);
			MP_WritePhyUshort(sc, 0x19, 0x8203);
			MP_WritePhyUshort(sc, 0x15, 0x03a6);
			MP_WritePhyUshort(sc, 0x19, 0x4d08);
			MP_WritePhyUshort(sc, 0x15, 0x03a7);
			MP_WritePhyUshort(sc, 0x19, 0x33a9);
			MP_WritePhyUshort(sc, 0x15, 0x03a8);
			MP_WritePhyUshort(sc, 0x19, 0x4d00);
			MP_WritePhyUshort(sc, 0x15, 0x03a9);
			MP_WritePhyUshort(sc, 0x19, 0x9bfa);
			MP_WritePhyUshort(sc, 0x15, 0x03aa);
			MP_WritePhyUshort(sc, 0x19, 0x33b6);
			MP_WritePhyUshort(sc, 0x15, 0x03bb);
			MP_WritePhyUshort(sc, 0x19, 0x4056);
			MP_WritePhyUshort(sc, 0x15, 0x03bc);
			MP_WritePhyUshort(sc, 0x19, 0x44e9);
			MP_WritePhyUshort(sc, 0x15, 0x03bd);
			MP_WritePhyUshort(sc, 0x19, 0x4054);
			MP_WritePhyUshort(sc, 0x15, 0x03be);
			MP_WritePhyUshort(sc, 0x19, 0x44f8);
			MP_WritePhyUshort(sc, 0x15, 0x03bf);
			MP_WritePhyUshort(sc, 0x19, 0xd64f);
			MP_WritePhyUshort(sc, 0x15, 0x03c0);
			MP_WritePhyUshort(sc, 0x19, 0x0037);
			MP_WritePhyUshort(sc, 0x15, 0x03c1);
			MP_WritePhyUshort(sc, 0x19, 0xbd37);
			MP_WritePhyUshort(sc, 0x15, 0x03c2);
			MP_WritePhyUshort(sc, 0x19, 0x9cfd);
			MP_WritePhyUshort(sc, 0x15, 0x03c3);
			MP_WritePhyUshort(sc, 0x19, 0xc639);
			MP_WritePhyUshort(sc, 0x15, 0x03c4);
			MP_WritePhyUshort(sc, 0x19, 0x0011);
			MP_WritePhyUshort(sc, 0x15, 0x03c5);
			MP_WritePhyUshort(sc, 0x19, 0x9b03);
			MP_WritePhyUshort(sc, 0x15, 0x03c6);
			MP_WritePhyUshort(sc, 0x19, 0x7c01);
			MP_WritePhyUshort(sc, 0x15, 0x03c7);
			MP_WritePhyUshort(sc, 0x19, 0x4c01);
			MP_WritePhyUshort(sc, 0x15, 0x03c8);
			MP_WritePhyUshort(sc, 0x19, 0x9e03);
			MP_WritePhyUshort(sc, 0x15, 0x03c9);
			MP_WritePhyUshort(sc, 0x19, 0x7c20);
			MP_WritePhyUshort(sc, 0x15, 0x03ca);
			MP_WritePhyUshort(sc, 0x19, 0x4c20);
			MP_WritePhyUshort(sc, 0x15, 0x03cb);
			MP_WritePhyUshort(sc, 0x19, 0x9af4);
			MP_WritePhyUshort(sc, 0x15, 0x03cc);
			MP_WritePhyUshort(sc, 0x19, 0x7c12);
			MP_WritePhyUshort(sc, 0x15, 0x03cd);
			MP_WritePhyUshort(sc, 0x19, 0x4c52);
			MP_WritePhyUshort(sc, 0x15, 0x03ce);
			MP_WritePhyUshort(sc, 0x19, 0x4470);
			MP_WritePhyUshort(sc, 0x15, 0x03cf);
			MP_WritePhyUshort(sc, 0x19, 0x7c12);
			MP_WritePhyUshort(sc, 0x15, 0x03d0);
			MP_WritePhyUshort(sc, 0x19, 0x4c40);
			MP_WritePhyUshort(sc, 0x15, 0x03d1);
			MP_WritePhyUshort(sc, 0x19, 0x33bf);
			MP_WritePhyUshort(sc, 0x15, 0x03d6);
			MP_WritePhyUshort(sc, 0x19, 0x4047);
			MP_WritePhyUshort(sc, 0x15, 0x03d7);
			MP_WritePhyUshort(sc, 0x19, 0x4469);
			MP_WritePhyUshort(sc, 0x15, 0x03d8);
			MP_WritePhyUshort(sc, 0x19, 0x492b);
			MP_WritePhyUshort(sc, 0x15, 0x03d9);
			MP_WritePhyUshort(sc, 0x19, 0x4479);
			MP_WritePhyUshort(sc, 0x15, 0x03da);
			MP_WritePhyUshort(sc, 0x19, 0x7c09);
			MP_WritePhyUshort(sc, 0x15, 0x03db);
			MP_WritePhyUshort(sc, 0x19, 0x8203);
			MP_WritePhyUshort(sc, 0x15, 0x03dc);
			MP_WritePhyUshort(sc, 0x19, 0x4d48);
			MP_WritePhyUshort(sc, 0x15, 0x03dd);
			MP_WritePhyUshort(sc, 0x19, 0x33df);
			MP_WritePhyUshort(sc, 0x15, 0x03de);
			MP_WritePhyUshort(sc, 0x19, 0x4d40);
			MP_WritePhyUshort(sc, 0x15, 0x03df);
			MP_WritePhyUshort(sc, 0x19, 0xd64f);
			MP_WritePhyUshort(sc, 0x15, 0x03e0);
			MP_WritePhyUshort(sc, 0x19, 0x0017);
			MP_WritePhyUshort(sc, 0x15, 0x03e1);
			MP_WritePhyUshort(sc, 0x19, 0xbd17);
			MP_WritePhyUshort(sc, 0x15, 0x03e2);
			MP_WritePhyUshort(sc, 0x19, 0x9b03);
			MP_WritePhyUshort(sc, 0x15, 0x03e3);
			MP_WritePhyUshort(sc, 0x19, 0x7c20);
			MP_WritePhyUshort(sc, 0x15, 0x03e4);
			MP_WritePhyUshort(sc, 0x19, 0x4c20);
			MP_WritePhyUshort(sc, 0x15, 0x03e5);
			MP_WritePhyUshort(sc, 0x19, 0x88f5);
			MP_WritePhyUshort(sc, 0x15, 0x03e6);
			MP_WritePhyUshort(sc, 0x19, 0xc428);
			MP_WritePhyUshort(sc, 0x15, 0x03e7);
			MP_WritePhyUshort(sc, 0x19, 0x0008);
			MP_WritePhyUshort(sc, 0x15, 0x03e8);
			MP_WritePhyUshort(sc, 0x19, 0x9af2);
			MP_WritePhyUshort(sc, 0x15, 0x03e9);
			MP_WritePhyUshort(sc, 0x19, 0x7c12);
			MP_WritePhyUshort(sc, 0x15, 0x03ea);
			MP_WritePhyUshort(sc, 0x19, 0x4c52);
			MP_WritePhyUshort(sc, 0x15, 0x03eb);
			MP_WritePhyUshort(sc, 0x19, 0x4470);
			MP_WritePhyUshort(sc, 0x15, 0x03ec);
			MP_WritePhyUshort(sc, 0x19, 0x7c12);
			MP_WritePhyUshort(sc, 0x15, 0x03ed);
			MP_WritePhyUshort(sc, 0x19, 0x4c40);
			MP_WritePhyUshort(sc, 0x15, 0x03ee);
			MP_WritePhyUshort(sc, 0x19, 0x33da);
			MP_WritePhyUshort(sc, 0x15, 0x03ef);
			MP_WritePhyUshort(sc, 0x19, 0x3312);
			MP_WritePhyUshort(sc, 0x16, 0x0306);
			MP_WritePhyUshort(sc, 0x16, 0x0300);
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
			MP_WritePhyUshort(sc, 0x17, 0x2179);
			MP_WritePhyUshort(sc, 0x1f, 0x0007);
			MP_WritePhyUshort(sc, 0x1e, 0x0040);
			MP_WritePhyUshort(sc, 0x18, 0x0645);
			MP_WritePhyUshort(sc, 0x19, 0xe200);
			MP_WritePhyUshort(sc, 0x18, 0x0655);
			MP_WritePhyUshort(sc, 0x19, 0x9000);
			MP_WritePhyUshort(sc, 0x18, 0x0d05);
			MP_WritePhyUshort(sc, 0x19, 0xbe00);
			MP_WritePhyUshort(sc, 0x18, 0x0d15);
			MP_WritePhyUshort(sc, 0x19, 0xd300);
			MP_WritePhyUshort(sc, 0x18, 0x0d25);
			MP_WritePhyUshort(sc, 0x19, 0xfe00);
			MP_WritePhyUshort(sc, 0x18, 0x0d35);
			MP_WritePhyUshort(sc, 0x19, 0x4000);
			MP_WritePhyUshort(sc, 0x18, 0x0d45);
			MP_WritePhyUshort(sc, 0x19, 0x7f00);
			MP_WritePhyUshort(sc, 0x18, 0x0d55);
			MP_WritePhyUshort(sc, 0x19, 0x1000);
			MP_WritePhyUshort(sc, 0x18, 0x0d65);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x18, 0x0d75);
			MP_WritePhyUshort(sc, 0x19, 0x8200);
			MP_WritePhyUshort(sc, 0x18, 0x0d85);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x18, 0x0d95);
			MP_WritePhyUshort(sc, 0x19, 0x7000);
			MP_WritePhyUshort(sc, 0x18, 0x0da5);
			MP_WritePhyUshort(sc, 0x19, 0x0f00);
			MP_WritePhyUshort(sc, 0x18, 0x0db5);
			MP_WritePhyUshort(sc, 0x19, 0x0100);
			MP_WritePhyUshort(sc, 0x18, 0x0dc5);
			MP_WritePhyUshort(sc, 0x19, 0x9b00);
			MP_WritePhyUshort(sc, 0x18, 0x0dd5);
			MP_WritePhyUshort(sc, 0x19, 0x7f00);
			MP_WritePhyUshort(sc, 0x18, 0x0de5);
			MP_WritePhyUshort(sc, 0x19, 0xe000);
			MP_WritePhyUshort(sc, 0x18, 0x0df5);
			MP_WritePhyUshort(sc, 0x19, 0xef00);
			MP_WritePhyUshort(sc, 0x18, 0x16d5);
			MP_WritePhyUshort(sc, 0x19, 0xe200);
			MP_WritePhyUshort(sc, 0x18, 0x16e5);
			MP_WritePhyUshort(sc, 0x19, 0xab00);
			MP_WritePhyUshort(sc, 0x18, 0x2904);
			MP_WritePhyUshort(sc, 0x19, 0x4000);
			MP_WritePhyUshort(sc, 0x18, 0x2914);
			MP_WritePhyUshort(sc, 0x19, 0x7f00);
			MP_WritePhyUshort(sc, 0x18, 0x2924);
			MP_WritePhyUshort(sc, 0x19, 0x0100);
			MP_WritePhyUshort(sc, 0x18, 0x2934);
			MP_WritePhyUshort(sc, 0x19, 0x2000);
			MP_WritePhyUshort(sc, 0x18, 0x2944);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x18, 0x2954);
			MP_WritePhyUshort(sc, 0x19, 0x4600);
			MP_WritePhyUshort(sc, 0x18, 0x2964);
			MP_WritePhyUshort(sc, 0x19, 0xfc00);
			MP_WritePhyUshort(sc, 0x18, 0x2974);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x18, 0x2984);
			MP_WritePhyUshort(sc, 0x19, 0x5000);
			MP_WritePhyUshort(sc, 0x18, 0x2994);
			MP_WritePhyUshort(sc, 0x19, 0x9d00);
			MP_WritePhyUshort(sc, 0x18, 0x29a4);
			MP_WritePhyUshort(sc, 0x19, 0xff00);
			MP_WritePhyUshort(sc, 0x18, 0x29b4);
			MP_WritePhyUshort(sc, 0x19, 0x4000);
			MP_WritePhyUshort(sc, 0x18, 0x29c4);
			MP_WritePhyUshort(sc, 0x19, 0x7f00);
			MP_WritePhyUshort(sc, 0x18, 0x29d4);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x18, 0x29e4);
			MP_WritePhyUshort(sc, 0x19, 0x2000);
			MP_WritePhyUshort(sc, 0x18, 0x29f4);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x18, 0x2a04);
			MP_WritePhyUshort(sc, 0x19, 0xe600);
			MP_WritePhyUshort(sc, 0x18, 0x2a14);
			MP_WritePhyUshort(sc, 0x19, 0xff00);
			MP_WritePhyUshort(sc, 0x18, 0x2a24);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x18, 0x2a34);
			MP_WritePhyUshort(sc, 0x19, 0x5000);
			MP_WritePhyUshort(sc, 0x18, 0x2a44);
			MP_WritePhyUshort(sc, 0x19, 0x8500);
			MP_WritePhyUshort(sc, 0x18, 0x2a54);
			MP_WritePhyUshort(sc, 0x19, 0x7f00);
			MP_WritePhyUshort(sc, 0x18, 0x2a64);
			MP_WritePhyUshort(sc, 0x19, 0xac00);
			MP_WritePhyUshort(sc, 0x18, 0x2a74);
			MP_WritePhyUshort(sc, 0x19, 0x0800);
			MP_WritePhyUshort(sc, 0x18, 0x2a84);
			MP_WritePhyUshort(sc, 0x19, 0xfc00);
			MP_WritePhyUshort(sc, 0x18, 0x2a94);
			MP_WritePhyUshort(sc, 0x19, 0xe000);
			MP_WritePhyUshort(sc, 0x18, 0x2aa4);
			MP_WritePhyUshort(sc, 0x19, 0x7400);
			MP_WritePhyUshort(sc, 0x18, 0x2ab4);
			MP_WritePhyUshort(sc, 0x19, 0x4000);
			MP_WritePhyUshort(sc, 0x18, 0x2ac4);
			MP_WritePhyUshort(sc, 0x19, 0x7f00);
			MP_WritePhyUshort(sc, 0x18, 0x2ad4);
			MP_WritePhyUshort(sc, 0x19, 0x0100);
			MP_WritePhyUshort(sc, 0x18, 0x2ae4);
			MP_WritePhyUshort(sc, 0x19, 0xff00);
			MP_WritePhyUshort(sc, 0x18, 0x2af4);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x18, 0x2b04);
			MP_WritePhyUshort(sc, 0x19, 0x4400);
			MP_WritePhyUshort(sc, 0x18, 0x2b14);
			MP_WritePhyUshort(sc, 0x19, 0xfc00);
			MP_WritePhyUshort(sc, 0x18, 0x2b24);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x18, 0x2b34);
			MP_WritePhyUshort(sc, 0x19, 0x4000);
			MP_WritePhyUshort(sc, 0x18, 0x2b44);
			MP_WritePhyUshort(sc, 0x19, 0x9d00);
			MP_WritePhyUshort(sc, 0x18, 0x2b54);
			MP_WritePhyUshort(sc, 0x19, 0xff00);
			MP_WritePhyUshort(sc, 0x18, 0x2b64);
			MP_WritePhyUshort(sc, 0x19, 0x4000);
			MP_WritePhyUshort(sc, 0x18, 0x2b74);
			MP_WritePhyUshort(sc, 0x19, 0x7f00);
			MP_WritePhyUshort(sc, 0x18, 0x2b84);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x18, 0x2b94);
			MP_WritePhyUshort(sc, 0x19, 0xff00);
			MP_WritePhyUshort(sc, 0x18, 0x2ba4);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x18, 0x2bb4);
			MP_WritePhyUshort(sc, 0x19, 0xfc00);
			MP_WritePhyUshort(sc, 0x18, 0x2bc4);
			MP_WritePhyUshort(sc, 0x19, 0xff00);
			MP_WritePhyUshort(sc, 0x18, 0x2bd4);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x18, 0x2be4);
			MP_WritePhyUshort(sc, 0x19, 0x4000);
			MP_WritePhyUshort(sc, 0x18, 0x2bf4);
			MP_WritePhyUshort(sc, 0x19, 0x8900);
			MP_WritePhyUshort(sc, 0x18, 0x2c04);
			MP_WritePhyUshort(sc, 0x19, 0x8300);
			MP_WritePhyUshort(sc, 0x18, 0x2c14);
			MP_WritePhyUshort(sc, 0x19, 0xe000);
			MP_WritePhyUshort(sc, 0x18, 0x2c24);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x18, 0x2c34);
			MP_WritePhyUshort(sc, 0x19, 0xac00);
			MP_WritePhyUshort(sc, 0x18, 0x2c44);
			MP_WritePhyUshort(sc, 0x19, 0x0800);
			MP_WritePhyUshort(sc, 0x18, 0x2c54);
			MP_WritePhyUshort(sc, 0x19, 0xfa00);
			MP_WritePhyUshort(sc, 0x18, 0x2c64);
			MP_WritePhyUshort(sc, 0x19, 0xe100);
			MP_WritePhyUshort(sc, 0x18, 0x2c74);
			MP_WritePhyUshort(sc, 0x19, 0x7f00);
			MP_WritePhyUshort(sc, 0x18, 0x0001);
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
			MP_WritePhyUshort(sc, 0x17, 0x2100);
			MP_WritePhyUshort(sc, 0x1f, 0x0005);
			MP_WritePhyUshort(sc, 0x05, 0xfff6);
			MP_WritePhyUshort(sc, 0x06, 0x0080);
			MP_WritePhyUshort(sc, 0x05, 0x8000);
			MP_WritePhyUshort(sc, 0x06, 0xd480);
			MP_WritePhyUshort(sc, 0x06, 0xc1e4);
			MP_WritePhyUshort(sc, 0x06, 0x8b9a);
			MP_WritePhyUshort(sc, 0x06, 0xe58b);
			MP_WritePhyUshort(sc, 0x06, 0x9bee);
			MP_WritePhyUshort(sc, 0x06, 0x8b83);
			MP_WritePhyUshort(sc, 0x06, 0x41bf);
			MP_WritePhyUshort(sc, 0x06, 0x8b88);
			MP_WritePhyUshort(sc, 0x06, 0xec00);
			MP_WritePhyUshort(sc, 0x06, 0x19a9);
			MP_WritePhyUshort(sc, 0x06, 0x8b90);
			MP_WritePhyUshort(sc, 0x06, 0xf9ee);
			MP_WritePhyUshort(sc, 0x06, 0xfff6);
			MP_WritePhyUshort(sc, 0x06, 0x00ee);
			MP_WritePhyUshort(sc, 0x06, 0xfff7);
			MP_WritePhyUshort(sc, 0x06, 0xffe0);
			MP_WritePhyUshort(sc, 0x06, 0xe140);
			MP_WritePhyUshort(sc, 0x06, 0xe1e1);
			MP_WritePhyUshort(sc, 0x06, 0x41f7);
			MP_WritePhyUshort(sc, 0x06, 0x2ff6);
			MP_WritePhyUshort(sc, 0x06, 0x28e4);
			MP_WritePhyUshort(sc, 0x06, 0xe140);
			MP_WritePhyUshort(sc, 0x06, 0xe5e1);
			MP_WritePhyUshort(sc, 0x06, 0x41f7);
			MP_WritePhyUshort(sc, 0x06, 0x0002);
			MP_WritePhyUshort(sc, 0x06, 0x020c);
			MP_WritePhyUshort(sc, 0x06, 0x0202);
			MP_WritePhyUshort(sc, 0x06, 0x1d02);
			MP_WritePhyUshort(sc, 0x06, 0x0230);
			MP_WritePhyUshort(sc, 0x06, 0x0202);
			MP_WritePhyUshort(sc, 0x06, 0x4002);
			MP_WritePhyUshort(sc, 0x06, 0x028b);
			MP_WritePhyUshort(sc, 0x06, 0x0280);
			MP_WritePhyUshort(sc, 0x06, 0x6c02);
			MP_WritePhyUshort(sc, 0x06, 0x8085);
			MP_WritePhyUshort(sc, 0x06, 0xe08b);
			MP_WritePhyUshort(sc, 0x06, 0x88e1);
			MP_WritePhyUshort(sc, 0x06, 0x8b89);
			MP_WritePhyUshort(sc, 0x06, 0x1e01);
			MP_WritePhyUshort(sc, 0x06, 0xe18b);
			MP_WritePhyUshort(sc, 0x06, 0x8a1e);
			MP_WritePhyUshort(sc, 0x06, 0x01e1);
			MP_WritePhyUshort(sc, 0x06, 0x8b8b);
			MP_WritePhyUshort(sc, 0x06, 0x1e01);
			MP_WritePhyUshort(sc, 0x06, 0xe18b);
			MP_WritePhyUshort(sc, 0x06, 0x8c1e);
			MP_WritePhyUshort(sc, 0x06, 0x01e1);
			MP_WritePhyUshort(sc, 0x06, 0x8b8d);
			MP_WritePhyUshort(sc, 0x06, 0x1e01);
			MP_WritePhyUshort(sc, 0x06, 0xe18b);
			MP_WritePhyUshort(sc, 0x06, 0x8e1e);
			MP_WritePhyUshort(sc, 0x06, 0x01a0);
			MP_WritePhyUshort(sc, 0x06, 0x00c7);
			MP_WritePhyUshort(sc, 0x06, 0xaec3);
			MP_WritePhyUshort(sc, 0x06, 0xf8e0);
			MP_WritePhyUshort(sc, 0x06, 0x8b8d);
			MP_WritePhyUshort(sc, 0x06, 0xad20);
			MP_WritePhyUshort(sc, 0x06, 0x10ee);
			MP_WritePhyUshort(sc, 0x06, 0x8b8d);
			MP_WritePhyUshort(sc, 0x06, 0x0002);
			MP_WritePhyUshort(sc, 0x06, 0x1310);
			MP_WritePhyUshort(sc, 0x06, 0x0280);
			MP_WritePhyUshort(sc, 0x06, 0xc602);
			MP_WritePhyUshort(sc, 0x06, 0x1f0c);
			MP_WritePhyUshort(sc, 0x06, 0x0227);
			MP_WritePhyUshort(sc, 0x06, 0x49fc);
			MP_WritePhyUshort(sc, 0x06, 0x04f8);
			MP_WritePhyUshort(sc, 0x06, 0xe08b);
			MP_WritePhyUshort(sc, 0x06, 0x8ead);
			MP_WritePhyUshort(sc, 0x06, 0x200b);
			MP_WritePhyUshort(sc, 0x06, 0xf620);
			MP_WritePhyUshort(sc, 0x06, 0xe48b);
			MP_WritePhyUshort(sc, 0x06, 0x8e02);
			MP_WritePhyUshort(sc, 0x06, 0x852d);
			MP_WritePhyUshort(sc, 0x06, 0x021b);
			MP_WritePhyUshort(sc, 0x06, 0x67ad);
			MP_WritePhyUshort(sc, 0x06, 0x2211);
			MP_WritePhyUshort(sc, 0x06, 0xf622);
			MP_WritePhyUshort(sc, 0x06, 0xe48b);
			MP_WritePhyUshort(sc, 0x06, 0x8e02);
			MP_WritePhyUshort(sc, 0x06, 0x2ba5);
			MP_WritePhyUshort(sc, 0x06, 0x022a);
			MP_WritePhyUshort(sc, 0x06, 0x2402);
			MP_WritePhyUshort(sc, 0x06, 0x82e5);
			MP_WritePhyUshort(sc, 0x06, 0x022a);
			MP_WritePhyUshort(sc, 0x06, 0xf0ad);
			MP_WritePhyUshort(sc, 0x06, 0x2511);
			MP_WritePhyUshort(sc, 0x06, 0xf625);
			MP_WritePhyUshort(sc, 0x06, 0xe48b);
			MP_WritePhyUshort(sc, 0x06, 0x8e02);
			MP_WritePhyUshort(sc, 0x06, 0x8445);
			MP_WritePhyUshort(sc, 0x06, 0x0204);
			MP_WritePhyUshort(sc, 0x06, 0x0302);
			MP_WritePhyUshort(sc, 0x06, 0x19cc);
			MP_WritePhyUshort(sc, 0x06, 0x022b);
			MP_WritePhyUshort(sc, 0x06, 0x5bfc);
			MP_WritePhyUshort(sc, 0x06, 0x04ee);
			MP_WritePhyUshort(sc, 0x06, 0x8b8d);
			MP_WritePhyUshort(sc, 0x06, 0x0105);
			MP_WritePhyUshort(sc, 0x06, 0xf8f9);
			MP_WritePhyUshort(sc, 0x06, 0xfae0);
			MP_WritePhyUshort(sc, 0x06, 0x8b81);
			MP_WritePhyUshort(sc, 0x06, 0xac26);
			MP_WritePhyUshort(sc, 0x06, 0x08e0);
			MP_WritePhyUshort(sc, 0x06, 0x8b81);
			MP_WritePhyUshort(sc, 0x06, 0xac21);
			MP_WritePhyUshort(sc, 0x06, 0x02ae);
			MP_WritePhyUshort(sc, 0x06, 0x6bee);
			MP_WritePhyUshort(sc, 0x06, 0xe0ea);
			MP_WritePhyUshort(sc, 0x06, 0x00ee);
			MP_WritePhyUshort(sc, 0x06, 0xe0eb);
			MP_WritePhyUshort(sc, 0x06, 0x00e2);
			MP_WritePhyUshort(sc, 0x06, 0xe07c);
			MP_WritePhyUshort(sc, 0x06, 0xe3e0);
			MP_WritePhyUshort(sc, 0x06, 0x7da5);
			MP_WritePhyUshort(sc, 0x06, 0x1111);
			MP_WritePhyUshort(sc, 0x06, 0x15d2);
			MP_WritePhyUshort(sc, 0x06, 0x60d6);
			MP_WritePhyUshort(sc, 0x06, 0x6666);
			MP_WritePhyUshort(sc, 0x06, 0x0207);
			MP_WritePhyUshort(sc, 0x06, 0x6cd2);
			MP_WritePhyUshort(sc, 0x06, 0xa0d6);
			MP_WritePhyUshort(sc, 0x06, 0xaaaa);
			MP_WritePhyUshort(sc, 0x06, 0x0207);
			MP_WritePhyUshort(sc, 0x06, 0x6c02);
			MP_WritePhyUshort(sc, 0x06, 0x201d);
			MP_WritePhyUshort(sc, 0x06, 0xae44);
			MP_WritePhyUshort(sc, 0x06, 0xa566);
			MP_WritePhyUshort(sc, 0x06, 0x6602);
			MP_WritePhyUshort(sc, 0x06, 0xae38);
			MP_WritePhyUshort(sc, 0x06, 0xa5aa);
			MP_WritePhyUshort(sc, 0x06, 0xaa02);
			MP_WritePhyUshort(sc, 0x06, 0xae32);
			MP_WritePhyUshort(sc, 0x06, 0xeee0);
			MP_WritePhyUshort(sc, 0x06, 0xea04);
			MP_WritePhyUshort(sc, 0x06, 0xeee0);
			MP_WritePhyUshort(sc, 0x06, 0xeb06);
			MP_WritePhyUshort(sc, 0x06, 0xe2e0);
			MP_WritePhyUshort(sc, 0x06, 0x7ce3);
			MP_WritePhyUshort(sc, 0x06, 0xe07d);
			MP_WritePhyUshort(sc, 0x06, 0xe0e0);
			MP_WritePhyUshort(sc, 0x06, 0x38e1);
			MP_WritePhyUshort(sc, 0x06, 0xe039);
			MP_WritePhyUshort(sc, 0x06, 0xad2e);
			MP_WritePhyUshort(sc, 0x06, 0x21ad);
			MP_WritePhyUshort(sc, 0x06, 0x3f13);
			MP_WritePhyUshort(sc, 0x06, 0xe0e4);
			MP_WritePhyUshort(sc, 0x06, 0x14e1);
			MP_WritePhyUshort(sc, 0x06, 0xe415);
			MP_WritePhyUshort(sc, 0x06, 0x6880);
			MP_WritePhyUshort(sc, 0x06, 0xe4e4);
			MP_WritePhyUshort(sc, 0x06, 0x14e5);
			MP_WritePhyUshort(sc, 0x06, 0xe415);
			MP_WritePhyUshort(sc, 0x06, 0x0220);
			MP_WritePhyUshort(sc, 0x06, 0x1dae);
			MP_WritePhyUshort(sc, 0x06, 0x0bac);
			MP_WritePhyUshort(sc, 0x06, 0x3e02);
			MP_WritePhyUshort(sc, 0x06, 0xae06);
			MP_WritePhyUshort(sc, 0x06, 0x0281);
			MP_WritePhyUshort(sc, 0x06, 0x4602);
			MP_WritePhyUshort(sc, 0x06, 0x2057);
			MP_WritePhyUshort(sc, 0x06, 0xfefd);
			MP_WritePhyUshort(sc, 0x06, 0xfc04);
			MP_WritePhyUshort(sc, 0x06, 0xf8e0);
			MP_WritePhyUshort(sc, 0x06, 0x8b81);
			MP_WritePhyUshort(sc, 0x06, 0xad26);
			MP_WritePhyUshort(sc, 0x06, 0x0302);
			MP_WritePhyUshort(sc, 0x06, 0x20a7);
			MP_WritePhyUshort(sc, 0x06, 0xe08b);
			MP_WritePhyUshort(sc, 0x06, 0x81ad);
			MP_WritePhyUshort(sc, 0x06, 0x2109);
			MP_WritePhyUshort(sc, 0x06, 0xe08b);
			MP_WritePhyUshort(sc, 0x06, 0x2eac);
			MP_WritePhyUshort(sc, 0x06, 0x2003);
			MP_WritePhyUshort(sc, 0x06, 0x0281);
			MP_WritePhyUshort(sc, 0x06, 0x61fc);
			MP_WritePhyUshort(sc, 0x06, 0x04f8);
			MP_WritePhyUshort(sc, 0x06, 0xe08b);
			MP_WritePhyUshort(sc, 0x06, 0x81ac);
			MP_WritePhyUshort(sc, 0x06, 0x2505);
			MP_WritePhyUshort(sc, 0x06, 0x0222);
			MP_WritePhyUshort(sc, 0x06, 0xaeae);
			MP_WritePhyUshort(sc, 0x06, 0x0302);
			MP_WritePhyUshort(sc, 0x06, 0x8172);
			MP_WritePhyUshort(sc, 0x06, 0xfc04);
			MP_WritePhyUshort(sc, 0x06, 0xf8f9);
			MP_WritePhyUshort(sc, 0x06, 0xfaef);
			MP_WritePhyUshort(sc, 0x06, 0x69fa);
			MP_WritePhyUshort(sc, 0x06, 0xe086);
			MP_WritePhyUshort(sc, 0x06, 0x20a0);
			MP_WritePhyUshort(sc, 0x06, 0x8016);
			MP_WritePhyUshort(sc, 0x06, 0xe086);
			MP_WritePhyUshort(sc, 0x06, 0x21e1);
			MP_WritePhyUshort(sc, 0x06, 0x8b33);
			MP_WritePhyUshort(sc, 0x06, 0x1b10);
			MP_WritePhyUshort(sc, 0x06, 0x9e06);
			MP_WritePhyUshort(sc, 0x06, 0x0223);
			MP_WritePhyUshort(sc, 0x06, 0x91af);
			MP_WritePhyUshort(sc, 0x06, 0x8252);
			MP_WritePhyUshort(sc, 0x06, 0xee86);
			MP_WritePhyUshort(sc, 0x06, 0x2081);
			MP_WritePhyUshort(sc, 0x06, 0xaee4);
			MP_WritePhyUshort(sc, 0x06, 0xa081);
			MP_WritePhyUshort(sc, 0x06, 0x1402);
			MP_WritePhyUshort(sc, 0x06, 0x2399);
			MP_WritePhyUshort(sc, 0x06, 0xbf25);
			MP_WritePhyUshort(sc, 0x06, 0xcc02);
			MP_WritePhyUshort(sc, 0x06, 0x2d21);
			MP_WritePhyUshort(sc, 0x06, 0xee86);
			MP_WritePhyUshort(sc, 0x06, 0x2100);
			MP_WritePhyUshort(sc, 0x06, 0xee86);
			MP_WritePhyUshort(sc, 0x06, 0x2082);
			MP_WritePhyUshort(sc, 0x06, 0xaf82);
			MP_WritePhyUshort(sc, 0x06, 0x52a0);
			MP_WritePhyUshort(sc, 0x06, 0x8232);
			MP_WritePhyUshort(sc, 0x06, 0xe086);
			MP_WritePhyUshort(sc, 0x06, 0x21e1);
			MP_WritePhyUshort(sc, 0x06, 0x8b32);
			MP_WritePhyUshort(sc, 0x06, 0x1b10);
			MP_WritePhyUshort(sc, 0x06, 0x9e06);
			MP_WritePhyUshort(sc, 0x06, 0x0223);
			MP_WritePhyUshort(sc, 0x06, 0x91af);
			MP_WritePhyUshort(sc, 0x06, 0x8252);
			MP_WritePhyUshort(sc, 0x06, 0xee86);
			MP_WritePhyUshort(sc, 0x06, 0x2100);
			MP_WritePhyUshort(sc, 0x06, 0xd000);
			MP_WritePhyUshort(sc, 0x06, 0x0282);
			MP_WritePhyUshort(sc, 0x06, 0x5910);
			MP_WritePhyUshort(sc, 0x06, 0xa004);
			MP_WritePhyUshort(sc, 0x06, 0xf9e0);
			MP_WritePhyUshort(sc, 0x06, 0x861f);
			MP_WritePhyUshort(sc, 0x06, 0xa000);
			MP_WritePhyUshort(sc, 0x06, 0x07ee);
			MP_WritePhyUshort(sc, 0x06, 0x8620);
			MP_WritePhyUshort(sc, 0x06, 0x83af);
			MP_WritePhyUshort(sc, 0x06, 0x8178);
			MP_WritePhyUshort(sc, 0x06, 0x0224);
			MP_WritePhyUshort(sc, 0x06, 0x0102);
			MP_WritePhyUshort(sc, 0x06, 0x2399);
			MP_WritePhyUshort(sc, 0x06, 0xae72);
			MP_WritePhyUshort(sc, 0x06, 0xa083);
			MP_WritePhyUshort(sc, 0x06, 0x4b1f);
			MP_WritePhyUshort(sc, 0x06, 0x55d0);
			MP_WritePhyUshort(sc, 0x06, 0x04bf);
			MP_WritePhyUshort(sc, 0x06, 0x8615);
			MP_WritePhyUshort(sc, 0x06, 0x1a90);
			MP_WritePhyUshort(sc, 0x06, 0x0c54);
			MP_WritePhyUshort(sc, 0x06, 0xd91e);
			MP_WritePhyUshort(sc, 0x06, 0x31b0);
			MP_WritePhyUshort(sc, 0x06, 0xf4e0);
			MP_WritePhyUshort(sc, 0x06, 0xe022);
			MP_WritePhyUshort(sc, 0x06, 0xe1e0);
			MP_WritePhyUshort(sc, 0x06, 0x23ad);
			MP_WritePhyUshort(sc, 0x06, 0x2e0c);
			MP_WritePhyUshort(sc, 0x06, 0xef02);
			MP_WritePhyUshort(sc, 0x06, 0xef12);
			MP_WritePhyUshort(sc, 0x06, 0x0e44);
			MP_WritePhyUshort(sc, 0x06, 0xef23);
			MP_WritePhyUshort(sc, 0x06, 0x0e54);
			MP_WritePhyUshort(sc, 0x06, 0xef21);
			MP_WritePhyUshort(sc, 0x06, 0xe6e4);
			MP_WritePhyUshort(sc, 0x06, 0x2ae7);
			MP_WritePhyUshort(sc, 0x06, 0xe42b);
			MP_WritePhyUshort(sc, 0x06, 0xe2e4);
			MP_WritePhyUshort(sc, 0x06, 0x28e3);
			MP_WritePhyUshort(sc, 0x06, 0xe429);
			MP_WritePhyUshort(sc, 0x06, 0x6d20);
			MP_WritePhyUshort(sc, 0x06, 0x00e6);
			MP_WritePhyUshort(sc, 0x06, 0xe428);
			MP_WritePhyUshort(sc, 0x06, 0xe7e4);
			MP_WritePhyUshort(sc, 0x06, 0x29bf);
			MP_WritePhyUshort(sc, 0x06, 0x25ca);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0x21ee);
			MP_WritePhyUshort(sc, 0x06, 0x8620);
			MP_WritePhyUshort(sc, 0x06, 0x84ee);
			MP_WritePhyUshort(sc, 0x06, 0x8621);
			MP_WritePhyUshort(sc, 0x06, 0x00af);
			MP_WritePhyUshort(sc, 0x06, 0x8178);
			MP_WritePhyUshort(sc, 0x06, 0xa084);
			MP_WritePhyUshort(sc, 0x06, 0x19e0);
			MP_WritePhyUshort(sc, 0x06, 0x8621);
			MP_WritePhyUshort(sc, 0x06, 0xe18b);
			MP_WritePhyUshort(sc, 0x06, 0x341b);
			MP_WritePhyUshort(sc, 0x06, 0x109e);
			MP_WritePhyUshort(sc, 0x06, 0x0602);
			MP_WritePhyUshort(sc, 0x06, 0x2391);
			MP_WritePhyUshort(sc, 0x06, 0xaf82);
			MP_WritePhyUshort(sc, 0x06, 0x5202);
			MP_WritePhyUshort(sc, 0x06, 0x241f);
			MP_WritePhyUshort(sc, 0x06, 0xee86);
			MP_WritePhyUshort(sc, 0x06, 0x2085);
			MP_WritePhyUshort(sc, 0x06, 0xae08);
			MP_WritePhyUshort(sc, 0x06, 0xa085);
			MP_WritePhyUshort(sc, 0x06, 0x02ae);
			MP_WritePhyUshort(sc, 0x06, 0x0302);
			MP_WritePhyUshort(sc, 0x06, 0x2442);
			MP_WritePhyUshort(sc, 0x06, 0xfeef);
			MP_WritePhyUshort(sc, 0x06, 0x96fe);
			MP_WritePhyUshort(sc, 0x06, 0xfdfc);
			MP_WritePhyUshort(sc, 0x06, 0x04f8);
			MP_WritePhyUshort(sc, 0x06, 0xf9fa);
			MP_WritePhyUshort(sc, 0x06, 0xef69);
			MP_WritePhyUshort(sc, 0x06, 0xfad1);
			MP_WritePhyUshort(sc, 0x06, 0x801f);
			MP_WritePhyUshort(sc, 0x06, 0x66e2);
			MP_WritePhyUshort(sc, 0x06, 0xe0ea);
			MP_WritePhyUshort(sc, 0x06, 0xe3e0);
			MP_WritePhyUshort(sc, 0x06, 0xeb5a);
			MP_WritePhyUshort(sc, 0x06, 0xf81e);
			MP_WritePhyUshort(sc, 0x06, 0x20e6);
			MP_WritePhyUshort(sc, 0x06, 0xe0ea);
			MP_WritePhyUshort(sc, 0x06, 0xe5e0);
			MP_WritePhyUshort(sc, 0x06, 0xebd3);
			MP_WritePhyUshort(sc, 0x06, 0x05b3);
			MP_WritePhyUshort(sc, 0x06, 0xfee2);
			MP_WritePhyUshort(sc, 0x06, 0xe07c);
			MP_WritePhyUshort(sc, 0x06, 0xe3e0);
			MP_WritePhyUshort(sc, 0x06, 0x7dad);
			MP_WritePhyUshort(sc, 0x06, 0x3703);
			MP_WritePhyUshort(sc, 0x06, 0x7dff);
			MP_WritePhyUshort(sc, 0x06, 0xff0d);
			MP_WritePhyUshort(sc, 0x06, 0x581c);
			MP_WritePhyUshort(sc, 0x06, 0x55f8);
			MP_WritePhyUshort(sc, 0x06, 0xef46);
			MP_WritePhyUshort(sc, 0x06, 0x0282);
			MP_WritePhyUshort(sc, 0x06, 0xc7ef);
			MP_WritePhyUshort(sc, 0x06, 0x65ef);
			MP_WritePhyUshort(sc, 0x06, 0x54fc);
			MP_WritePhyUshort(sc, 0x06, 0xac30);
			MP_WritePhyUshort(sc, 0x06, 0x2b11);
			MP_WritePhyUshort(sc, 0x06, 0xa188);
			MP_WritePhyUshort(sc, 0x06, 0xcabf);
			MP_WritePhyUshort(sc, 0x06, 0x860e);
			MP_WritePhyUshort(sc, 0x06, 0xef10);
			MP_WritePhyUshort(sc, 0x06, 0x0c11);
			MP_WritePhyUshort(sc, 0x06, 0x1a91);
			MP_WritePhyUshort(sc, 0x06, 0xda19);
			MP_WritePhyUshort(sc, 0x06, 0xdbf8);
			MP_WritePhyUshort(sc, 0x06, 0xef46);
			MP_WritePhyUshort(sc, 0x06, 0x021e);
			MP_WritePhyUshort(sc, 0x06, 0x17ef);
			MP_WritePhyUshort(sc, 0x06, 0x54fc);
			MP_WritePhyUshort(sc, 0x06, 0xad30);
			MP_WritePhyUshort(sc, 0x06, 0x0fef);
			MP_WritePhyUshort(sc, 0x06, 0x5689);
			MP_WritePhyUshort(sc, 0x06, 0xde19);
			MP_WritePhyUshort(sc, 0x06, 0xdfe2);
			MP_WritePhyUshort(sc, 0x06, 0x861f);
			MP_WritePhyUshort(sc, 0x06, 0xbf86);
			MP_WritePhyUshort(sc, 0x06, 0x161a);
			MP_WritePhyUshort(sc, 0x06, 0x90de);
			MP_WritePhyUshort(sc, 0x06, 0xfeef);
			MP_WritePhyUshort(sc, 0x06, 0x96fe);
			MP_WritePhyUshort(sc, 0x06, 0xfdfc);
			MP_WritePhyUshort(sc, 0x06, 0x04ac);
			MP_WritePhyUshort(sc, 0x06, 0x2707);
			MP_WritePhyUshort(sc, 0x06, 0xac37);
			MP_WritePhyUshort(sc, 0x06, 0x071a);
			MP_WritePhyUshort(sc, 0x06, 0x54ae);
			MP_WritePhyUshort(sc, 0x06, 0x11ac);
			MP_WritePhyUshort(sc, 0x06, 0x3707);
			MP_WritePhyUshort(sc, 0x06, 0xae00);
			MP_WritePhyUshort(sc, 0x06, 0x1a54);
			MP_WritePhyUshort(sc, 0x06, 0xac37);
			MP_WritePhyUshort(sc, 0x06, 0x07d0);
			MP_WritePhyUshort(sc, 0x06, 0x01d5);
			MP_WritePhyUshort(sc, 0x06, 0xffff);
			MP_WritePhyUshort(sc, 0x06, 0xae02);
			MP_WritePhyUshort(sc, 0x06, 0xd000);
			MP_WritePhyUshort(sc, 0x06, 0x04f8);
			MP_WritePhyUshort(sc, 0x06, 0xe08b);
			MP_WritePhyUshort(sc, 0x06, 0x83ad);
			MP_WritePhyUshort(sc, 0x06, 0x2444);
			MP_WritePhyUshort(sc, 0x06, 0xe0e0);
			MP_WritePhyUshort(sc, 0x06, 0x22e1);
			MP_WritePhyUshort(sc, 0x06, 0xe023);
			MP_WritePhyUshort(sc, 0x06, 0xad22);
			MP_WritePhyUshort(sc, 0x06, 0x3be0);
			MP_WritePhyUshort(sc, 0x06, 0x8abe);
			MP_WritePhyUshort(sc, 0x06, 0xa000);
			MP_WritePhyUshort(sc, 0x06, 0x0502);
			MP_WritePhyUshort(sc, 0x06, 0x28de);
			MP_WritePhyUshort(sc, 0x06, 0xae42);
			MP_WritePhyUshort(sc, 0x06, 0xa001);
			MP_WritePhyUshort(sc, 0x06, 0x0502);
			MP_WritePhyUshort(sc, 0x06, 0x28f1);
			MP_WritePhyUshort(sc, 0x06, 0xae3a);
			MP_WritePhyUshort(sc, 0x06, 0xa002);
			MP_WritePhyUshort(sc, 0x06, 0x0502);
			MP_WritePhyUshort(sc, 0x06, 0x8344);
			MP_WritePhyUshort(sc, 0x06, 0xae32);
			MP_WritePhyUshort(sc, 0x06, 0xa003);
			MP_WritePhyUshort(sc, 0x06, 0x0502);
			MP_WritePhyUshort(sc, 0x06, 0x299a);
			MP_WritePhyUshort(sc, 0x06, 0xae2a);
			MP_WritePhyUshort(sc, 0x06, 0xa004);
			MP_WritePhyUshort(sc, 0x06, 0x0502);
			MP_WritePhyUshort(sc, 0x06, 0x29ae);
			MP_WritePhyUshort(sc, 0x06, 0xae22);
			MP_WritePhyUshort(sc, 0x06, 0xa005);
			MP_WritePhyUshort(sc, 0x06, 0x0502);
			MP_WritePhyUshort(sc, 0x06, 0x29d7);
			MP_WritePhyUshort(sc, 0x06, 0xae1a);
			MP_WritePhyUshort(sc, 0x06, 0xa006);
			MP_WritePhyUshort(sc, 0x06, 0x0502);
			MP_WritePhyUshort(sc, 0x06, 0x29fe);
			MP_WritePhyUshort(sc, 0x06, 0xae12);
			MP_WritePhyUshort(sc, 0x06, 0xee8a);
			MP_WritePhyUshort(sc, 0x06, 0xc000);
			MP_WritePhyUshort(sc, 0x06, 0xee8a);
			MP_WritePhyUshort(sc, 0x06, 0xc100);
			MP_WritePhyUshort(sc, 0x06, 0xee8a);
			MP_WritePhyUshort(sc, 0x06, 0xc600);
			MP_WritePhyUshort(sc, 0x06, 0xee8a);
			MP_WritePhyUshort(sc, 0x06, 0xbe00);
			MP_WritePhyUshort(sc, 0x06, 0xae00);
			MP_WritePhyUshort(sc, 0x06, 0xfc04);
			MP_WritePhyUshort(sc, 0x06, 0xf802);
			MP_WritePhyUshort(sc, 0x06, 0x2a67);
			MP_WritePhyUshort(sc, 0x06, 0xe0e0);
			MP_WritePhyUshort(sc, 0x06, 0x22e1);
			MP_WritePhyUshort(sc, 0x06, 0xe023);
			MP_WritePhyUshort(sc, 0x06, 0x0d06);
			MP_WritePhyUshort(sc, 0x06, 0x5803);
			MP_WritePhyUshort(sc, 0x06, 0xa002);
			MP_WritePhyUshort(sc, 0x06, 0x02ae);
			MP_WritePhyUshort(sc, 0x06, 0x2da0);
			MP_WritePhyUshort(sc, 0x06, 0x0102);
			MP_WritePhyUshort(sc, 0x06, 0xae2d);
			MP_WritePhyUshort(sc, 0x06, 0xa000);
			MP_WritePhyUshort(sc, 0x06, 0x4de0);
			MP_WritePhyUshort(sc, 0x06, 0xe200);
			MP_WritePhyUshort(sc, 0x06, 0xe1e2);
			MP_WritePhyUshort(sc, 0x06, 0x01ad);
			MP_WritePhyUshort(sc, 0x06, 0x2444);
			MP_WritePhyUshort(sc, 0x06, 0xe08a);
			MP_WritePhyUshort(sc, 0x06, 0xc2e4);
			MP_WritePhyUshort(sc, 0x06, 0x8ac4);
			MP_WritePhyUshort(sc, 0x06, 0xe08a);
			MP_WritePhyUshort(sc, 0x06, 0xc3e4);
			MP_WritePhyUshort(sc, 0x06, 0x8ac5);
			MP_WritePhyUshort(sc, 0x06, 0xee8a);
			MP_WritePhyUshort(sc, 0x06, 0xbe03);
			MP_WritePhyUshort(sc, 0x06, 0xe08b);
			MP_WritePhyUshort(sc, 0x06, 0x83ad);
			MP_WritePhyUshort(sc, 0x06, 0x253a);
			MP_WritePhyUshort(sc, 0x06, 0xee8a);
			MP_WritePhyUshort(sc, 0x06, 0xbe05);
			MP_WritePhyUshort(sc, 0x06, 0xae34);
			MP_WritePhyUshort(sc, 0x06, 0xe08a);
			MP_WritePhyUshort(sc, 0x06, 0xceae);
			MP_WritePhyUshort(sc, 0x06, 0x03e0);
			MP_WritePhyUshort(sc, 0x06, 0x8acf);
			MP_WritePhyUshort(sc, 0x06, 0xe18a);
			MP_WritePhyUshort(sc, 0x06, 0xc249);
			MP_WritePhyUshort(sc, 0x06, 0x05e5);
			MP_WritePhyUshort(sc, 0x06, 0x8ac4);
			MP_WritePhyUshort(sc, 0x06, 0xe18a);
			MP_WritePhyUshort(sc, 0x06, 0xc349);
			MP_WritePhyUshort(sc, 0x06, 0x05e5);
			MP_WritePhyUshort(sc, 0x06, 0x8ac5);
			MP_WritePhyUshort(sc, 0x06, 0xee8a);
			MP_WritePhyUshort(sc, 0x06, 0xbe05);
			MP_WritePhyUshort(sc, 0x06, 0x022a);
			MP_WritePhyUshort(sc, 0x06, 0xb6ac);
			MP_WritePhyUshort(sc, 0x06, 0x2012);
			MP_WritePhyUshort(sc, 0x06, 0x0283);
			MP_WritePhyUshort(sc, 0x06, 0xbaac);
			MP_WritePhyUshort(sc, 0x06, 0x200c);
			MP_WritePhyUshort(sc, 0x06, 0xee8a);
			MP_WritePhyUshort(sc, 0x06, 0xc100);
			MP_WritePhyUshort(sc, 0x06, 0xee8a);
			MP_WritePhyUshort(sc, 0x06, 0xc600);
			MP_WritePhyUshort(sc, 0x06, 0xee8a);
			MP_WritePhyUshort(sc, 0x06, 0xbe02);
			MP_WritePhyUshort(sc, 0x06, 0xfc04);
			MP_WritePhyUshort(sc, 0x06, 0xd000);
			MP_WritePhyUshort(sc, 0x06, 0x0283);
			MP_WritePhyUshort(sc, 0x06, 0xcc59);
			MP_WritePhyUshort(sc, 0x06, 0x0f39);
			MP_WritePhyUshort(sc, 0x06, 0x02aa);
			MP_WritePhyUshort(sc, 0x06, 0x04d0);
			MP_WritePhyUshort(sc, 0x06, 0x01ae);
			MP_WritePhyUshort(sc, 0x06, 0x02d0);
			MP_WritePhyUshort(sc, 0x06, 0x0004);
			MP_WritePhyUshort(sc, 0x06, 0xf9fa);
			MP_WritePhyUshort(sc, 0x06, 0xe2e2);
			MP_WritePhyUshort(sc, 0x06, 0xd2e3);
			MP_WritePhyUshort(sc, 0x06, 0xe2d3);
			MP_WritePhyUshort(sc, 0x06, 0xf95a);
			MP_WritePhyUshort(sc, 0x06, 0xf7e6);
			MP_WritePhyUshort(sc, 0x06, 0xe2d2);
			MP_WritePhyUshort(sc, 0x06, 0xe7e2);
			MP_WritePhyUshort(sc, 0x06, 0xd3e2);
			MP_WritePhyUshort(sc, 0x06, 0xe02c);
			MP_WritePhyUshort(sc, 0x06, 0xe3e0);
			MP_WritePhyUshort(sc, 0x06, 0x2df9);
			MP_WritePhyUshort(sc, 0x06, 0x5be0);
			MP_WritePhyUshort(sc, 0x06, 0x1e30);
			MP_WritePhyUshort(sc, 0x06, 0xe6e0);
			MP_WritePhyUshort(sc, 0x06, 0x2ce7);
			MP_WritePhyUshort(sc, 0x06, 0xe02d);
			MP_WritePhyUshort(sc, 0x06, 0xe2e2);
			MP_WritePhyUshort(sc, 0x06, 0xcce3);
			MP_WritePhyUshort(sc, 0x06, 0xe2cd);
			MP_WritePhyUshort(sc, 0x06, 0xf95a);
			MP_WritePhyUshort(sc, 0x06, 0x0f6a);
			MP_WritePhyUshort(sc, 0x06, 0x50e6);
			MP_WritePhyUshort(sc, 0x06, 0xe2cc);
			MP_WritePhyUshort(sc, 0x06, 0xe7e2);
			MP_WritePhyUshort(sc, 0x06, 0xcde0);
			MP_WritePhyUshort(sc, 0x06, 0xe03c);
			MP_WritePhyUshort(sc, 0x06, 0xe1e0);
			MP_WritePhyUshort(sc, 0x06, 0x3def);
			MP_WritePhyUshort(sc, 0x06, 0x64fd);
			MP_WritePhyUshort(sc, 0x06, 0xe0e2);
			MP_WritePhyUshort(sc, 0x06, 0xcce1);
			MP_WritePhyUshort(sc, 0x06, 0xe2cd);
			MP_WritePhyUshort(sc, 0x06, 0x580f);
			MP_WritePhyUshort(sc, 0x06, 0x5af0);
			MP_WritePhyUshort(sc, 0x06, 0x1e02);
			MP_WritePhyUshort(sc, 0x06, 0xe4e2);
			MP_WritePhyUshort(sc, 0x06, 0xcce5);
			MP_WritePhyUshort(sc, 0x06, 0xe2cd);
			MP_WritePhyUshort(sc, 0x06, 0xfde0);
			MP_WritePhyUshort(sc, 0x06, 0xe02c);
			MP_WritePhyUshort(sc, 0x06, 0xe1e0);
			MP_WritePhyUshort(sc, 0x06, 0x2d59);
			MP_WritePhyUshort(sc, 0x06, 0xe05b);
			MP_WritePhyUshort(sc, 0x06, 0x1f1e);
			MP_WritePhyUshort(sc, 0x06, 0x13e4);
			MP_WritePhyUshort(sc, 0x06, 0xe02c);
			MP_WritePhyUshort(sc, 0x06, 0xe5e0);
			MP_WritePhyUshort(sc, 0x06, 0x2dfd);
			MP_WritePhyUshort(sc, 0x06, 0xe0e2);
			MP_WritePhyUshort(sc, 0x06, 0xd2e1);
			MP_WritePhyUshort(sc, 0x06, 0xe2d3);
			MP_WritePhyUshort(sc, 0x06, 0x58f7);
			MP_WritePhyUshort(sc, 0x06, 0x5a08);
			MP_WritePhyUshort(sc, 0x06, 0x1e02);
			MP_WritePhyUshort(sc, 0x06, 0xe4e2);
			MP_WritePhyUshort(sc, 0x06, 0xd2e5);
			MP_WritePhyUshort(sc, 0x06, 0xe2d3);
			MP_WritePhyUshort(sc, 0x06, 0xef46);
			MP_WritePhyUshort(sc, 0x06, 0xfefd);
			MP_WritePhyUshort(sc, 0x06, 0x04f8);
			MP_WritePhyUshort(sc, 0x06, 0xf9fa);
			MP_WritePhyUshort(sc, 0x06, 0xef69);
			MP_WritePhyUshort(sc, 0x06, 0xe0e0);
			MP_WritePhyUshort(sc, 0x06, 0x22e1);
			MP_WritePhyUshort(sc, 0x06, 0xe023);
			MP_WritePhyUshort(sc, 0x06, 0x58c4);
			MP_WritePhyUshort(sc, 0x06, 0xe18b);
			MP_WritePhyUshort(sc, 0x06, 0x6e1f);
			MP_WritePhyUshort(sc, 0x06, 0x109e);
			MP_WritePhyUshort(sc, 0x06, 0x58e4);
			MP_WritePhyUshort(sc, 0x06, 0x8b6e);
			MP_WritePhyUshort(sc, 0x06, 0xad22);
			MP_WritePhyUshort(sc, 0x06, 0x22ac);
			MP_WritePhyUshort(sc, 0x06, 0x2755);
			MP_WritePhyUshort(sc, 0x06, 0xac26);
			MP_WritePhyUshort(sc, 0x06, 0x02ae);
			MP_WritePhyUshort(sc, 0x06, 0x1ad1);
			MP_WritePhyUshort(sc, 0x06, 0x06bf);
			MP_WritePhyUshort(sc, 0x06, 0x3bba);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0xc1d1);
			MP_WritePhyUshort(sc, 0x06, 0x07bf);
			MP_WritePhyUshort(sc, 0x06, 0x3bbd);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0xc1d1);
			MP_WritePhyUshort(sc, 0x06, 0x07bf);
			MP_WritePhyUshort(sc, 0x06, 0x3bc0);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0xc1ae);
			MP_WritePhyUshort(sc, 0x06, 0x30d1);
			MP_WritePhyUshort(sc, 0x06, 0x03bf);
			MP_WritePhyUshort(sc, 0x06, 0x3bc3);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0xc1d1);
			MP_WritePhyUshort(sc, 0x06, 0x00bf);
			MP_WritePhyUshort(sc, 0x06, 0x3bc6);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0xc1d1);
			MP_WritePhyUshort(sc, 0x06, 0x00bf);
			MP_WritePhyUshort(sc, 0x06, 0x84e9);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0xc1d1);
			MP_WritePhyUshort(sc, 0x06, 0x0fbf);
			MP_WritePhyUshort(sc, 0x06, 0x3bba);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0xc1d1);
			MP_WritePhyUshort(sc, 0x06, 0x01bf);
			MP_WritePhyUshort(sc, 0x06, 0x3bbd);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0xc1d1);
			MP_WritePhyUshort(sc, 0x06, 0x01bf);
			MP_WritePhyUshort(sc, 0x06, 0x3bc0);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0xc1ef);
			MP_WritePhyUshort(sc, 0x06, 0x96fe);
			MP_WritePhyUshort(sc, 0x06, 0xfdfc);
			MP_WritePhyUshort(sc, 0x06, 0x04d1);
			MP_WritePhyUshort(sc, 0x06, 0x00bf);
			MP_WritePhyUshort(sc, 0x06, 0x3bc3);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0xc1d0);
			MP_WritePhyUshort(sc, 0x06, 0x1102);
			MP_WritePhyUshort(sc, 0x06, 0x2bfb);
			MP_WritePhyUshort(sc, 0x06, 0x5903);
			MP_WritePhyUshort(sc, 0x06, 0xef01);
			MP_WritePhyUshort(sc, 0x06, 0xd100);
			MP_WritePhyUshort(sc, 0x06, 0xa000);
			MP_WritePhyUshort(sc, 0x06, 0x02d1);
			MP_WritePhyUshort(sc, 0x06, 0x01bf);
			MP_WritePhyUshort(sc, 0x06, 0x3bc6);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0xc1d1);
			MP_WritePhyUshort(sc, 0x06, 0x11ad);
			MP_WritePhyUshort(sc, 0x06, 0x2002);
			MP_WritePhyUshort(sc, 0x06, 0x0c11);
			MP_WritePhyUshort(sc, 0x06, 0xad21);
			MP_WritePhyUshort(sc, 0x06, 0x020c);
			MP_WritePhyUshort(sc, 0x06, 0x12bf);
			MP_WritePhyUshort(sc, 0x06, 0x84e9);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0xc1ae);
			MP_WritePhyUshort(sc, 0x06, 0xc870);
			MP_WritePhyUshort(sc, 0x06, 0xe426);
			MP_WritePhyUshort(sc, 0x06, 0x0284);
			MP_WritePhyUshort(sc, 0x06, 0xf005);
			MP_WritePhyUshort(sc, 0x06, 0xf8fa);
			MP_WritePhyUshort(sc, 0x06, 0xef69);
			MP_WritePhyUshort(sc, 0x06, 0xe0e2);
			MP_WritePhyUshort(sc, 0x06, 0xfee1);
			MP_WritePhyUshort(sc, 0x06, 0xe2ff);
			MP_WritePhyUshort(sc, 0x06, 0xad2d);
			MP_WritePhyUshort(sc, 0x06, 0x1ae0);
			MP_WritePhyUshort(sc, 0x06, 0xe14e);
			MP_WritePhyUshort(sc, 0x06, 0xe1e1);
			MP_WritePhyUshort(sc, 0x06, 0x4fac);
			MP_WritePhyUshort(sc, 0x06, 0x2d22);
			MP_WritePhyUshort(sc, 0x06, 0xf603);
			MP_WritePhyUshort(sc, 0x06, 0x0203);
			MP_WritePhyUshort(sc, 0x06, 0x3bf7);
			MP_WritePhyUshort(sc, 0x06, 0x03f7);
			MP_WritePhyUshort(sc, 0x06, 0x06bf);
			MP_WritePhyUshort(sc, 0x06, 0x8561);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0x21ae);
			MP_WritePhyUshort(sc, 0x06, 0x11e0);
			MP_WritePhyUshort(sc, 0x06, 0xe14e);
			MP_WritePhyUshort(sc, 0x06, 0xe1e1);
			MP_WritePhyUshort(sc, 0x06, 0x4fad);
			MP_WritePhyUshort(sc, 0x06, 0x2d08);
			MP_WritePhyUshort(sc, 0x06, 0xbf85);
			MP_WritePhyUshort(sc, 0x06, 0x6c02);
			MP_WritePhyUshort(sc, 0x06, 0x2d21);
			MP_WritePhyUshort(sc, 0x06, 0xf606);
			MP_WritePhyUshort(sc, 0x06, 0xef96);
			MP_WritePhyUshort(sc, 0x06, 0xfefc);
			MP_WritePhyUshort(sc, 0x06, 0x04f8);
			MP_WritePhyUshort(sc, 0x06, 0xfaef);
			MP_WritePhyUshort(sc, 0x06, 0x69e0);
			MP_WritePhyUshort(sc, 0x06, 0xe000);
			MP_WritePhyUshort(sc, 0x06, 0xe1e0);
			MP_WritePhyUshort(sc, 0x06, 0x01ad);
			MP_WritePhyUshort(sc, 0x06, 0x271f);
			MP_WritePhyUshort(sc, 0x06, 0xd101);
			MP_WritePhyUshort(sc, 0x06, 0xbf85);
			MP_WritePhyUshort(sc, 0x06, 0x5e02);
			MP_WritePhyUshort(sc, 0x06, 0x2dc1);
			MP_WritePhyUshort(sc, 0x06, 0xe0e0);
			MP_WritePhyUshort(sc, 0x06, 0x20e1);
			MP_WritePhyUshort(sc, 0x06, 0xe021);
			MP_WritePhyUshort(sc, 0x06, 0xad20);
			MP_WritePhyUshort(sc, 0x06, 0x0ed1);
			MP_WritePhyUshort(sc, 0x06, 0x00bf);
			MP_WritePhyUshort(sc, 0x06, 0x855e);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0xc1bf);
			MP_WritePhyUshort(sc, 0x06, 0x3b96);
			MP_WritePhyUshort(sc, 0x06, 0x022d);
			MP_WritePhyUshort(sc, 0x06, 0x21ef);
			MP_WritePhyUshort(sc, 0x06, 0x96fe);
			MP_WritePhyUshort(sc, 0x06, 0xfc04);
			MP_WritePhyUshort(sc, 0x06, 0x00e2);
			MP_WritePhyUshort(sc, 0x06, 0x34a7);
			MP_WritePhyUshort(sc, 0x06, 0x25e5);
			MP_WritePhyUshort(sc, 0x06, 0x0a1d);
			MP_WritePhyUshort(sc, 0x06, 0xe50a);
			MP_WritePhyUshort(sc, 0x06, 0x2ce5);
			MP_WritePhyUshort(sc, 0x06, 0x0a6d);
			MP_WritePhyUshort(sc, 0x06, 0xe50a);
			MP_WritePhyUshort(sc, 0x06, 0x1de5);
			MP_WritePhyUshort(sc, 0x06, 0x0a1c);
			MP_WritePhyUshort(sc, 0x06, 0xe50a);
			MP_WritePhyUshort(sc, 0x06, 0x2da7);
			MP_WritePhyUshort(sc, 0x06, 0x5500);
			MP_WritePhyUshort(sc, 0x05, 0x8b94);
			MP_WritePhyUshort(sc, 0x06, 0x84ec);
			Data = MP_ReadPhyUshort(sc, 0x01);
			Data |= 0x0001;
			MP_WritePhyUshort(sc, 0x01, Data);
			MP_WritePhyUshort(sc, 0x00, 0x0005);
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
			MP_WritePhyUshort(sc, 0x1f, 0x0005);
			for(i=0;i<200;i++)
			{
				DELAY(100);
				Data = MP_ReadPhyUshort(sc, 0x00);
				if(Data & 0x0080)
					break;
			}
			MP_WritePhyUshort(sc, 0x1f, 0x0007);
			MP_WritePhyUshort(sc, 0x1e, 0x0023);
			MP_WritePhyUshort(sc, 0x17, 0x0116);
			MP_WritePhyUshort(sc, 0x1f, 0x0007);
			MP_WritePhyUshort(sc, 0x1e, 0x0028);
			MP_WritePhyUshort(sc, 0x15, 0x0010);
			MP_WritePhyUshort(sc, 0x1f, 0x0007);
			MP_WritePhyUshort(sc, 0x1e, 0x0020);
			MP_WritePhyUshort(sc, 0x15, 0x0100);
			MP_WritePhyUshort(sc, 0x1f, 0x0007);
			MP_WritePhyUshort(sc, 0x1e, 0x0041);
			MP_WritePhyUshort(sc, 0x15, 0x0802);
			MP_WritePhyUshort(sc, 0x16, 0x2185);
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
		}
		else if(sc->rg_type == MACFG_37)
		{
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
			MP_WritePhyUshort(sc, 0x00, 0x1800);
			MP_WritePhyUshort(sc, 0x1f, 0x0007);
			MP_WritePhyUshort(sc, 0x1e, 0x0023);
			MP_WritePhyUshort(sc, 0x17, 0x0117);
			MP_WritePhyUshort(sc, 0x1f, 0x0007);
			MP_WritePhyUshort(sc, 0x1E, 0x002C);
			MP_WritePhyUshort(sc, 0x1B, 0x5000);
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
			MP_WritePhyUshort(sc, 0x16, 0x4104);
			for(i=0;i<200;i++)
			{
				DELAY(100);
				Data = MP_ReadPhyUshort(sc, 0x1E);
				Data &= 0x03FF;
				if(Data == 0x000C)
					break;
			}
			MP_WritePhyUshort(sc, 0x1f, 0x0005);
			for(i=0;i<200;i++)
			{
				DELAY(100);
				Data = MP_ReadPhyUshort(sc, 0x07);
				if((Data & 0x0020)==0)
					break;
			}
			Data = MP_ReadPhyUshort(sc, 0x07);
			if(Data & 0x0020)
			{
				MP_WritePhyUshort(sc, 0x1f, 0x0007);
				MP_WritePhyUshort(sc, 0x1e, 0x00a1);
				MP_WritePhyUshort(sc, 0x17, 0x1000);
				MP_WritePhyUshort(sc, 0x17, 0x0000);
				MP_WritePhyUshort(sc, 0x17, 0x2000);
				MP_WritePhyUshort(sc, 0x1e, 0x002f);
				MP_WritePhyUshort(sc, 0x18, 0x9bfb);
				MP_WritePhyUshort(sc, 0x1f, 0x0005);
				MP_WritePhyUshort(sc, 0x07, 0x0000);
				MP_WritePhyUshort(sc, 0x1f, 0x0000);
			}
			MP_WritePhyUshort(sc, 0x1f, 0x0005);
			MP_WritePhyUshort(sc, 0x05, 0xfff6);
			MP_WritePhyUshort(sc, 0x06, 0x0080);
			Data = MP_ReadPhyUshort(sc, 0x00);
			Data &= ~(0x0080);
			MP_WritePhyUshort(sc, 0x00, Data);
			MP_WritePhyUshort(sc, 0x1f, 0x0002);
			Data = MP_ReadPhyUshort(sc, 0x08);
			Data &= ~(0x0080);
			MP_WritePhyUshort(sc, 0x08, Data);
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
			MP_WritePhyUshort(sc, 0x1f, 0x0007);
			MP_WritePhyUshort(sc, 0x1e, 0x0023);
			MP_WritePhyUshort(sc, 0x16, 0x0306);
			MP_WritePhyUshort(sc, 0x16, 0x0307);
			MP_WritePhyUshort(sc, 0x15, 0x000e);
			MP_WritePhyUshort(sc, 0x19, 0x000a);
			MP_WritePhyUshort(sc, 0x15, 0x0010);
			MP_WritePhyUshort(sc, 0x19, 0x0008);
			MP_WritePhyUshort(sc, 0x15, 0x0018);
			MP_WritePhyUshort(sc, 0x19, 0x4801);
			MP_WritePhyUshort(sc, 0x15, 0x0019);
			MP_WritePhyUshort(sc, 0x19, 0x6801);
			MP_WritePhyUshort(sc, 0x15, 0x001a);
			MP_WritePhyUshort(sc, 0x19, 0x66a1);
			MP_WritePhyUshort(sc, 0x15, 0x001f);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0020);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0021);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0022);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0023);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0024);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0025);
			MP_WritePhyUshort(sc, 0x19, 0x64a1);
			MP_WritePhyUshort(sc, 0x15, 0x0026);
			MP_WritePhyUshort(sc, 0x19, 0x40ea);
			MP_WritePhyUshort(sc, 0x15, 0x0027);
			MP_WritePhyUshort(sc, 0x19, 0x4503);
			MP_WritePhyUshort(sc, 0x15, 0x0028);
			MP_WritePhyUshort(sc, 0x19, 0x9f00);
			MP_WritePhyUshort(sc, 0x15, 0x0029);
			MP_WritePhyUshort(sc, 0x19, 0xa631);
			MP_WritePhyUshort(sc, 0x15, 0x002a);
			MP_WritePhyUshort(sc, 0x19, 0x9717);
			MP_WritePhyUshort(sc, 0x15, 0x002b);
			MP_WritePhyUshort(sc, 0x19, 0x302c);
			MP_WritePhyUshort(sc, 0x15, 0x002c);
			MP_WritePhyUshort(sc, 0x19, 0x4802);
			MP_WritePhyUshort(sc, 0x15, 0x002d);
			MP_WritePhyUshort(sc, 0x19, 0x58da);
			MP_WritePhyUshort(sc, 0x15, 0x002e);
			MP_WritePhyUshort(sc, 0x19, 0x400d);
			MP_WritePhyUshort(sc, 0x15, 0x002f);
			MP_WritePhyUshort(sc, 0x19, 0x4488);
			MP_WritePhyUshort(sc, 0x15, 0x0030);
			MP_WritePhyUshort(sc, 0x19, 0x9e00);
			MP_WritePhyUshort(sc, 0x15, 0x0031);
			MP_WritePhyUshort(sc, 0x19, 0x63c8);
			MP_WritePhyUshort(sc, 0x15, 0x0032);
			MP_WritePhyUshort(sc, 0x19, 0x6481);
			MP_WritePhyUshort(sc, 0x15, 0x0033);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0034);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0035);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0036);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0037);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0038);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0039);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x003a);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x003b);
			MP_WritePhyUshort(sc, 0x19, 0x63e8);
			MP_WritePhyUshort(sc, 0x15, 0x003c);
			MP_WritePhyUshort(sc, 0x19, 0x7d00);
			MP_WritePhyUshort(sc, 0x15, 0x003d);
			MP_WritePhyUshort(sc, 0x19, 0x59d4);
			MP_WritePhyUshort(sc, 0x15, 0x003e);
			MP_WritePhyUshort(sc, 0x19, 0x63f8);
			MP_WritePhyUshort(sc, 0x15, 0x0040);
			MP_WritePhyUshort(sc, 0x19, 0x64a1);
			MP_WritePhyUshort(sc, 0x15, 0x0041);
			MP_WritePhyUshort(sc, 0x19, 0x30de);
			MP_WritePhyUshort(sc, 0x15, 0x0044);
			MP_WritePhyUshort(sc, 0x19, 0x480f);
			MP_WritePhyUshort(sc, 0x15, 0x0045);
			MP_WritePhyUshort(sc, 0x19, 0x6800);
			MP_WritePhyUshort(sc, 0x15, 0x0046);
			MP_WritePhyUshort(sc, 0x19, 0x6680);
			MP_WritePhyUshort(sc, 0x15, 0x0047);
			MP_WritePhyUshort(sc, 0x19, 0x7c10);
			MP_WritePhyUshort(sc, 0x15, 0x0048);
			MP_WritePhyUshort(sc, 0x19, 0x63c8);
			MP_WritePhyUshort(sc, 0x15, 0x0049);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x004a);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x004b);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x004c);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x004d);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x004e);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x004f);
			MP_WritePhyUshort(sc, 0x19, 0x40ea);
			MP_WritePhyUshort(sc, 0x15, 0x0050);
			MP_WritePhyUshort(sc, 0x19, 0x4503);
			MP_WritePhyUshort(sc, 0x15, 0x0051);
			MP_WritePhyUshort(sc, 0x19, 0x58ca);
			MP_WritePhyUshort(sc, 0x15, 0x0052);
			MP_WritePhyUshort(sc, 0x19, 0x63c8);
			MP_WritePhyUshort(sc, 0x15, 0x0053);
			MP_WritePhyUshort(sc, 0x19, 0x63d8);
			MP_WritePhyUshort(sc, 0x15, 0x0054);
			MP_WritePhyUshort(sc, 0x19, 0x66a0);
			MP_WritePhyUshort(sc, 0x15, 0x0055);
			MP_WritePhyUshort(sc, 0x19, 0x9f00);
			MP_WritePhyUshort(sc, 0x15, 0x0056);
			MP_WritePhyUshort(sc, 0x19, 0x3000);
			MP_WritePhyUshort(sc, 0x15, 0x00a1);
			MP_WritePhyUshort(sc, 0x19, 0x3044);
			MP_WritePhyUshort(sc, 0x15, 0x00ab);
			MP_WritePhyUshort(sc, 0x19, 0x5820);
			MP_WritePhyUshort(sc, 0x15, 0x00ac);
			MP_WritePhyUshort(sc, 0x19, 0x5e04);
			MP_WritePhyUshort(sc, 0x15, 0x00ad);
			MP_WritePhyUshort(sc, 0x19, 0xb60c);
			MP_WritePhyUshort(sc, 0x15, 0x00af);
			MP_WritePhyUshort(sc, 0x19, 0x000a);
			MP_WritePhyUshort(sc, 0x15, 0x00b2);
			MP_WritePhyUshort(sc, 0x19, 0x30b9);
			MP_WritePhyUshort(sc, 0x15, 0x00b9);
			MP_WritePhyUshort(sc, 0x19, 0x4408);
			MP_WritePhyUshort(sc, 0x15, 0x00ba);
			MP_WritePhyUshort(sc, 0x19, 0x480b);
			MP_WritePhyUshort(sc, 0x15, 0x00bb);
			MP_WritePhyUshort(sc, 0x19, 0x5e00);
			MP_WritePhyUshort(sc, 0x15, 0x00bc);
			MP_WritePhyUshort(sc, 0x19, 0x405f);
			MP_WritePhyUshort(sc, 0x15, 0x00bd);
			MP_WritePhyUshort(sc, 0x19, 0x4448);
			MP_WritePhyUshort(sc, 0x15, 0x00be);
			MP_WritePhyUshort(sc, 0x19, 0x4020);
			MP_WritePhyUshort(sc, 0x15, 0x00bf);
			MP_WritePhyUshort(sc, 0x19, 0x4468);
			MP_WritePhyUshort(sc, 0x15, 0x00c0);
			MP_WritePhyUshort(sc, 0x19, 0x9c02);
			MP_WritePhyUshort(sc, 0x15, 0x00c1);
			MP_WritePhyUshort(sc, 0x19, 0x58a0);
			MP_WritePhyUshort(sc, 0x15, 0x00c2);
			MP_WritePhyUshort(sc, 0x19, 0xb605);
			MP_WritePhyUshort(sc, 0x15, 0x00c3);
			MP_WritePhyUshort(sc, 0x19, 0xc0d3);
			MP_WritePhyUshort(sc, 0x15, 0x00c4);
			MP_WritePhyUshort(sc, 0x19, 0x00e6);
			MP_WritePhyUshort(sc, 0x15, 0x00c5);
			MP_WritePhyUshort(sc, 0x19, 0xdaec);
			MP_WritePhyUshort(sc, 0x15, 0x00c6);
			MP_WritePhyUshort(sc, 0x19, 0x00fa);
			MP_WritePhyUshort(sc, 0x15, 0x00c7);
			MP_WritePhyUshort(sc, 0x19, 0x9df9);
			MP_WritePhyUshort(sc, 0x15, 0x0112);
			MP_WritePhyUshort(sc, 0x19, 0x6421);
			MP_WritePhyUshort(sc, 0x15, 0x0113);
			MP_WritePhyUshort(sc, 0x19, 0x7c08);
			MP_WritePhyUshort(sc, 0x15, 0x0114);
			MP_WritePhyUshort(sc, 0x19, 0x63f0);
			MP_WritePhyUshort(sc, 0x15, 0x0115);
			MP_WritePhyUshort(sc, 0x19, 0x4003);
			MP_WritePhyUshort(sc, 0x15, 0x0116);
			MP_WritePhyUshort(sc, 0x19, 0x4418);
			MP_WritePhyUshort(sc, 0x15, 0x0117);
			MP_WritePhyUshort(sc, 0x19, 0x9b00);
			MP_WritePhyUshort(sc, 0x15, 0x0118);
			MP_WritePhyUshort(sc, 0x19, 0x6461);
			MP_WritePhyUshort(sc, 0x15, 0x0119);
			MP_WritePhyUshort(sc, 0x19, 0x64e1);
			MP_WritePhyUshort(sc, 0x15, 0x011a);
			MP_WritePhyUshort(sc, 0x19, 0x0000);
			MP_WritePhyUshort(sc, 0x15, 0x0150);
			MP_WritePhyUshort(sc, 0x19, 0x6461);
			MP_WritePhyUshort(sc, 0x15, 0x0151);
			MP_WritePhyUshort(sc, 0x19, 0x4003);
			MP_WritePhyUshort(sc, 0x15, 0x0152);
			MP_WritePhyUshort(sc, 0x19, 0x4540);
			MP_WritePhyUshort(sc, 0x15, 0x0153);
			MP_WritePhyUshort(sc, 0x19, 0x9f00);
			MP_WritePhyUshort(sc, 0x15, 0x0155);
			MP_WritePhyUshort(sc, 0x19, 0x6421);
			MP_WritePhyUshort(sc, 0x15, 0x0156);
			MP_WritePhyUshort(sc, 0x19, 0x64a1);
			MP_WritePhyUshort(sc, 0x15, 0x03bd);
			MP_WritePhyUshort(sc, 0x19, 0x405e);
			MP_WritePhyUshort(sc, 0x16, 0x0306);
			MP_WritePhyUshort(sc, 0x16, 0x0300);
			MP_WritePhyUshort(sc, 0x1f, 0x0005);
			MP_WritePhyUshort(sc, 0x05, 0xfff6);
			MP_WritePhyUshort(sc, 0x06, 0x0080);
			MP_WritePhyUshort(sc, 0x05, 0x8000);
			MP_WritePhyUshort(sc, 0x06, 0x0280);
			MP_WritePhyUshort(sc, 0x06, 0x48f7);
			MP_WritePhyUshort(sc, 0x06, 0x00e0);
			MP_WritePhyUshort(sc, 0x06, 0xfff7);
			MP_WritePhyUshort(sc, 0x06, 0xa080);
			MP_WritePhyUshort(sc, 0x06, 0x02ae);
			MP_WritePhyUshort(sc, 0x06, 0xf602);
			MP_WritePhyUshort(sc, 0x06, 0x0200);
			MP_WritePhyUshort(sc, 0x06, 0x0202);
			MP_WritePhyUshort(sc, 0x06, 0x1102);
			MP_WritePhyUshort(sc, 0x06, 0x0224);
			MP_WritePhyUshort(sc, 0x06, 0x0202);
			MP_WritePhyUshort(sc, 0x06, 0x3402);
			MP_WritePhyUshort(sc, 0x06, 0x027f);
			MP_WritePhyUshort(sc, 0x06, 0x0202);
			MP_WritePhyUshort(sc, 0x06, 0x9202);
			MP_WritePhyUshort(sc, 0x06, 0x8074);
			MP_WritePhyUshort(sc, 0x06, 0xe08b);
			MP_WritePhyUshort(sc, 0x06, 0x88e1);
			MP_WritePhyUshort(sc, 0x06, 0x8b89);
			MP_WritePhyUshort(sc, 0x06, 0x1e01);
			MP_WritePhyUshort(sc, 0x06, 0xe18b);
			MP_WritePhyUshort(sc, 0x06, 0x8a1e);
			MP_WritePhyUshort(sc, 0x06, 0x01e1);
			MP_WritePhyUshort(sc, 0x06, 0x8b8b);
			MP_WritePhyUshort(sc, 0x06, 0x1e01);
			MP_WritePhyUshort(sc, 0x06, 0xe18b);
			MP_WritePhyUshort(sc, 0x06, 0x8c1e);
			MP_WritePhyUshort(sc, 0x06, 0x01e1);
			MP_WritePhyUshort(sc, 0x06, 0x8b8d);
			MP_WritePhyUshort(sc, 0x06, 0x1e01);
			MP_WritePhyUshort(sc, 0x06, 0xe18b);
			MP_WritePhyUshort(sc, 0x06, 0x8e1e);
			MP_WritePhyUshort(sc, 0x06, 0x01a0);
			MP_WritePhyUshort(sc, 0x06, 0x00c7);
			MP_WritePhyUshort(sc, 0x06, 0xaebb);
			MP_WritePhyUshort(sc, 0x06, 0xd480);
			MP_WritePhyUshort(sc, 0x06, 0xe4e4);
			MP_WritePhyUshort(sc, 0x06, 0x8b94);
			MP_WritePhyUshort(sc, 0x06, 0xe58b);
			MP_WritePhyUshort(sc, 0x06, 0x95bf);
			MP_WritePhyUshort(sc, 0x06, 0x8b88);
			MP_WritePhyUshort(sc, 0x06, 0xec00);
			MP_WritePhyUshort(sc, 0x06, 0x19a9);
			MP_WritePhyUshort(sc, 0x06, 0x8b90);
			MP_WritePhyUshort(sc, 0x06, 0xf9ee);
			MP_WritePhyUshort(sc, 0x06, 0xfff6);
			MP_WritePhyUshort(sc, 0x06, 0x00ee);
			MP_WritePhyUshort(sc, 0x06, 0xfff7);
			MP_WritePhyUshort(sc, 0x06, 0xffe0);
			MP_WritePhyUshort(sc, 0x06, 0xe140);
			MP_WritePhyUshort(sc, 0x06, 0xe1e1);
			MP_WritePhyUshort(sc, 0x06, 0x41f7);
			MP_WritePhyUshort(sc, 0x06, 0x2ff6);
			MP_WritePhyUshort(sc, 0x06, 0x28e4);
			MP_WritePhyUshort(sc, 0x06, 0xe140);
			MP_WritePhyUshort(sc, 0x06, 0xe5e1);
			MP_WritePhyUshort(sc, 0x06, 0x4104);
			MP_WritePhyUshort(sc, 0x06, 0xf8e0);
			MP_WritePhyUshort(sc, 0x06, 0x8b8e);
			MP_WritePhyUshort(sc, 0x06, 0xad20);
			MP_WritePhyUshort(sc, 0x06, 0x0ef6);
			MP_WritePhyUshort(sc, 0x06, 0x20e4);
			MP_WritePhyUshort(sc, 0x06, 0x8b8e);
			MP_WritePhyUshort(sc, 0x06, 0x0280);
			MP_WritePhyUshort(sc, 0x06, 0xb302);
			MP_WritePhyUshort(sc, 0x06, 0x1bf4);
			MP_WritePhyUshort(sc, 0x06, 0x022c);
			MP_WritePhyUshort(sc, 0x06, 0x9cad);
			MP_WritePhyUshort(sc, 0x06, 0x2211);
			MP_WritePhyUshort(sc, 0x06, 0xf622);
			MP_WritePhyUshort(sc, 0x06, 0xe48b);
			MP_WritePhyUshort(sc, 0x06, 0x8e02);
			MP_WritePhyUshort(sc, 0x06, 0x2c46);
			MP_WritePhyUshort(sc, 0x06, 0x022a);
			MP_WritePhyUshort(sc, 0x06, 0xc502);
			MP_WritePhyUshort(sc, 0x06, 0x2920);
			MP_WritePhyUshort(sc, 0x06, 0x022b);
			MP_WritePhyUshort(sc, 0x06, 0x91ad);
			MP_WritePhyUshort(sc, 0x06, 0x2511);
			MP_WritePhyUshort(sc, 0x06, 0xf625);
			MP_WritePhyUshort(sc, 0x06, 0xe48b);
			MP_WritePhyUshort(sc, 0x06, 0x8e02);
			MP_WritePhyUshort(sc, 0x06, 0x035a);
			MP_WritePhyUshort(sc, 0x06, 0x0204);
			MP_WritePhyUshort(sc, 0x06, 0x3a02);
			MP_WritePhyUshort(sc, 0x06, 0x1a59);
			MP_WritePhyUshort(sc, 0x06, 0x022b);
			MP_WritePhyUshort(sc, 0x06, 0xfcfc);
			MP_WritePhyUshort(sc, 0x06, 0x04f8);
			MP_WritePhyUshort(sc, 0x06, 0xfaef);
			MP_WritePhyUshort(sc, 0x06, 0x69e0);
			MP_WritePhyUshort(sc, 0x06, 0xe000);
			MP_WritePhyUshort(sc, 0x06, 0xe1e0);
			MP_WritePhyUshort(sc, 0x06, 0x01ad);
			MP_WritePhyUshort(sc, 0x06, 0x271f);
			MP_WritePhyUshort(sc, 0x06, 0xd101);
			MP_WritePhyUshort(sc, 0x06, 0xbf81);
			MP_WritePhyUshort(sc, 0x06, 0x3b02);
			MP_WritePhyUshort(sc, 0x06, 0x2f50);
			MP_WritePhyUshort(sc, 0x06, 0xe0e0);
			MP_WritePhyUshort(sc, 0x06, 0x20e1);
			MP_WritePhyUshort(sc, 0x06, 0xe021);
			MP_WritePhyUshort(sc, 0x06, 0xad20);
			MP_WritePhyUshort(sc, 0x06, 0x0ed1);
			MP_WritePhyUshort(sc, 0x06, 0x00bf);
			MP_WritePhyUshort(sc, 0x06, 0x813b);
			MP_WritePhyUshort(sc, 0x06, 0x022f);
			MP_WritePhyUshort(sc, 0x06, 0x50bf);
			MP_WritePhyUshort(sc, 0x06, 0x3d39);
			MP_WritePhyUshort(sc, 0x06, 0x022e);
			MP_WritePhyUshort(sc, 0x06, 0xb0ef);
			MP_WritePhyUshort(sc, 0x06, 0x96fe);
			MP_WritePhyUshort(sc, 0x06, 0xfc04);
			MP_WritePhyUshort(sc, 0x06, 0x0280);
			MP_WritePhyUshort(sc, 0x06, 0xe805);
			MP_WritePhyUshort(sc, 0x06, 0xf8fa);
			MP_WritePhyUshort(sc, 0x06, 0xef69);
			MP_WritePhyUshort(sc, 0x06, 0xe0e2);
			MP_WritePhyUshort(sc, 0x06, 0xfee1);
			MP_WritePhyUshort(sc, 0x06, 0xe2ff);
			MP_WritePhyUshort(sc, 0x06, 0xad2d);
			MP_WritePhyUshort(sc, 0x06, 0x1ae0);
			MP_WritePhyUshort(sc, 0x06, 0xe14e);
			MP_WritePhyUshort(sc, 0x06, 0xe1e1);
			MP_WritePhyUshort(sc, 0x06, 0x4fac);
			MP_WritePhyUshort(sc, 0x06, 0x2d22);
			MP_WritePhyUshort(sc, 0x06, 0xf603);
			MP_WritePhyUshort(sc, 0x06, 0x0203);
			MP_WritePhyUshort(sc, 0x06, 0x36f7);
			MP_WritePhyUshort(sc, 0x06, 0x03f7);
			MP_WritePhyUshort(sc, 0x06, 0x06bf);
			MP_WritePhyUshort(sc, 0x06, 0x8125);
			MP_WritePhyUshort(sc, 0x06, 0x022e);
			MP_WritePhyUshort(sc, 0x06, 0xb0ae);
			MP_WritePhyUshort(sc, 0x06, 0x11e0);
			MP_WritePhyUshort(sc, 0x06, 0xe14e);
			MP_WritePhyUshort(sc, 0x06, 0xe1e1);
			MP_WritePhyUshort(sc, 0x06, 0x4fad);
			MP_WritePhyUshort(sc, 0x06, 0x2d08);
			MP_WritePhyUshort(sc, 0x06, 0xbf81);
			MP_WritePhyUshort(sc, 0x06, 0x3002);
			MP_WritePhyUshort(sc, 0x06, 0x2eb0);
			MP_WritePhyUshort(sc, 0x06, 0xf606);
			MP_WritePhyUshort(sc, 0x06, 0xef96);
			MP_WritePhyUshort(sc, 0x06, 0xfefc);
			MP_WritePhyUshort(sc, 0x06, 0x04a7);
			MP_WritePhyUshort(sc, 0x06, 0x25e5);
			MP_WritePhyUshort(sc, 0x06, 0x0a1d);
			MP_WritePhyUshort(sc, 0x06, 0xe50a);
			MP_WritePhyUshort(sc, 0x06, 0x2ce5);
			MP_WritePhyUshort(sc, 0x06, 0x0a6d);
			MP_WritePhyUshort(sc, 0x06, 0xe50a);
			MP_WritePhyUshort(sc, 0x06, 0x1de5);
			MP_WritePhyUshort(sc, 0x06, 0x0a1c);
			MP_WritePhyUshort(sc, 0x06, 0xe50a);
			MP_WritePhyUshort(sc, 0x06, 0x2da7);
			MP_WritePhyUshort(sc, 0x06, 0x5500);
			MP_WritePhyUshort(sc, 0x06, 0xe234);
			Data = MP_ReadPhyUshort(sc, 0x01);
			Data |= 0x0001;
			MP_WritePhyUshort(sc, 0x01, Data);
			MP_WritePhyUshort(sc, 0x00, 0x0005);
			MP_WritePhyUshort(sc, 0x1f, 0x0000);
			MP_WritePhyUshort(sc, 0x1f, 0x0005);
			for(i=0;i<200;i++)
			{
				DELAY(100);
				Data = MP_ReadPhyUshort(sc, 0x00);
				if(Data & 0x0080)
					break;
			}
			MP_WritePhyUshort(sc, 0x1f, 0x0007);
			MP_WritePhyUshort(sc, 0x1e, 0x0023);
			MP_WritePhyUshort(sc, 0x17, 0x0116);
			MP_WritePhyUshort(sc, 0x1F, 0x0000);
		}

		MP_WritePhyUshort(sc, 0x1F, 0x0007);
		MP_WritePhyUshort(sc, 0x1E, 0x0023);
		MP_WritePhyUshort(sc, 0x17, 0x0116);
		MP_WritePhyUshort(sc, 0x1F, 0x0000);

		MP_WritePhyUshort(sc, 0x1f, 0x0005);
		MP_WritePhyUshort(sc, 0x05, 0x8b80);
		MP_WritePhyUshort(sc, 0x06, 0xc896);
		MP_WritePhyUshort(sc, 0x1f, 0x0000);

		MP_WritePhyUshort(sc, 0x1F, 0x0001);
		MP_WritePhyUshort(sc, 0x0B, 0x8C60);
		MP_WritePhyUshort(sc, 0x07, 0x2872);
		MP_WritePhyUshort(sc, 0x1C, 0xEFFF);
		MP_WritePhyUshort(sc, 0x1F, 0x0003);
		MP_WritePhyUshort(sc, 0x14, 0x94B0);
		MP_WritePhyUshort(sc, 0x1F, 0x0000);

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		Data = MP_ReadPhyUshort(sc, 0x08) & 0x00FF;
		MP_WritePhyUshort(sc, 0x08, Data | 0x8000);

		MP_WritePhyUshort(sc, 0x1F, 0x0007);
		MP_WritePhyUshort(sc, 0x1E, 0x002D);
		Data = MP_ReadPhyUshort(sc, 0x18);
		MP_WritePhyUshort(sc, 0x18, Data | 0x0010);
		MP_WritePhyUshort(sc, 0x1F, 0x0000);
		Data = MP_ReadPhyUshort(sc, 0x14);
		MP_WritePhyUshort(sc, 0x14, Data | 0x8000);

		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		MP_WritePhyUshort(sc, 0x00, 0x080B);
		MP_WritePhyUshort(sc, 0x0B, 0x09D7);
		MP_WritePhyUshort(sc, 0x1f, 0x0000);
		MP_WritePhyUshort(sc, 0x15, 0x1006);

		MP_WritePhyUshort(sc, 0x1F, 0x0003);
		MP_WritePhyUshort(sc, 0x19, 0x7F46);
		MP_WritePhyUshort(sc, 0x1F, 0x0005);
		MP_WritePhyUshort(sc, 0x05, 0x8AD2);
		MP_WritePhyUshort(sc, 0x06, 0x6810);
		MP_WritePhyUshort(sc, 0x05, 0x8AD4);
		MP_WritePhyUshort(sc, 0x06, 0x8002);
		MP_WritePhyUshort(sc, 0x05, 0x8ADE);
		MP_WritePhyUshort(sc, 0x06, 0x8025);
		MP_WritePhyUshort(sc, 0x1F, 0x0000);
	} else if (sc->rg_type == MACFG_41) {
		MP_WritePhyUshort(sc, 0x1F, 0x0000);
		MP_WritePhyUshort(sc, 0x11, MP_ReadPhyUshort(sc, 0x11) | 0x1000);
		MP_WritePhyUshort(sc, 0x1F, 0x0002);
		MP_WritePhyUshort(sc, 0x0F, MP_ReadPhyUshort(sc, 0x0F) | 0x0003);
		MP_WritePhyUshort(sc, 0x1F, 0x0000);

		for(Data_u32=0x800E0068; Data_u32<0x800E006D; Data_u32++)
		{
			CSR_WRITE_4(sc, 0xF8, Data_u32 );
			for(i=0; i<10; i++)
			{
				DELAY(400);
				if((CSR_READ_4(sc, 0xF8)&0x80000000)==0)
					break;
			}
		}
	} else if (sc->rg_type == MACFG_42) {
		CSR_WRITE_4(sc, RG_ERIAR, 0x000041D0 );
		for(i=0; i<10; i++)
		{
			DELAY(400);
			if(CSR_READ_4(sc, RG_ERIAR)&0x80000000)
				break;
		}
		Data_u32 = CSR_READ_4(sc, RG_ERIDR) & 0xFFFF0000;
		Data_u32 |= 0x4D02;
		CSR_WRITE_4(sc, RG_ERIDR, Data_u32 );
		CSR_WRITE_4(sc, RG_ERIAR, 0x000021D0 );
		for(i=0; i<10; i++)
		{
			DELAY(400);
			if((CSR_READ_4(sc, RG_ERIAR)&0x80000000)==0)
				break;
		}

		CSR_WRITE_4(sc, RG_ERIAR, 0x000041DC );
		for(i=0; i<10; i++)
		{
			DELAY(400);
			if(CSR_READ_4(sc, RG_ERIAR)&0x80000000)
				break;
		}
		Data_u32 = CSR_READ_4(sc, RG_ERIDR) & 0xFFFF0000;
		Data_u32 |= 0x0050;
		CSR_WRITE_4(sc, RG_ERIDR, Data_u32 );
		CSR_WRITE_4(sc, RG_ERIAR, 0x000021DC );
		for(i=0; i<10; i++)
		{
			DELAY(400);
			if((CSR_READ_4(sc, RG_ERIAR)&0x80000000)==0)
				break;
		}

		MP_WritePhyUshort(sc, 0x1f, 0x0004);
		MP_WritePhyUshort(sc, 0x1f, 0x0004);
		MP_WritePhyUshort(sc, 0x19, 0x7160);
		MP_WritePhyUshort(sc, 0x1c, 0x0600);
		MP_WritePhyUshort(sc, 0x1d, 0x9700);
		MP_WritePhyUshort(sc, 0x1d, 0x7d00);
		MP_WritePhyUshort(sc, 0x1d, 0x6900);
		MP_WritePhyUshort(sc, 0x1d, 0x7d00);
		MP_WritePhyUshort(sc, 0x1d, 0x6800);
		MP_WritePhyUshort(sc, 0x1d, 0x4899);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c01);
		MP_WritePhyUshort(sc, 0x1d, 0x8000);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x4000);
		MP_WritePhyUshort(sc, 0x1d, 0x4400);
		MP_WritePhyUshort(sc, 0x1d, 0x4800);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x5310);
		MP_WritePhyUshort(sc, 0x1d, 0x6000);
		MP_WritePhyUshort(sc, 0x1d, 0x6800);
		MP_WritePhyUshort(sc, 0x1d, 0x6736);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x571f);
		MP_WritePhyUshort(sc, 0x1d, 0x5ffb);
		MP_WritePhyUshort(sc, 0x1d, 0xaa03);
		MP_WritePhyUshort(sc, 0x1d, 0x5b58);
		MP_WritePhyUshort(sc, 0x1d, 0x301e);
		MP_WritePhyUshort(sc, 0x1d, 0x5b64);
		MP_WritePhyUshort(sc, 0x1d, 0xa6fc);
		MP_WritePhyUshort(sc, 0x1d, 0xdcdb);
		MP_WritePhyUshort(sc, 0x1d, 0x0014);
		MP_WritePhyUshort(sc, 0x1d, 0xd9a9);
		MP_WritePhyUshort(sc, 0x1d, 0x0013);
		MP_WritePhyUshort(sc, 0x1d, 0xd16b);
		MP_WritePhyUshort(sc, 0x1d, 0x0011);
		MP_WritePhyUshort(sc, 0x1d, 0xb40e);
		MP_WritePhyUshort(sc, 0x1d, 0xd06b);
		MP_WritePhyUshort(sc, 0x1d, 0x000c);
		MP_WritePhyUshort(sc, 0x1d, 0xb206);
		MP_WritePhyUshort(sc, 0x1d, 0x7c01);
		MP_WritePhyUshort(sc, 0x1d, 0x5800);
		MP_WritePhyUshort(sc, 0x1d, 0x7c04);
		MP_WritePhyUshort(sc, 0x1d, 0x5c00);
		MP_WritePhyUshort(sc, 0x1d, 0x301a);
		MP_WritePhyUshort(sc, 0x1d, 0x7c01);
		MP_WritePhyUshort(sc, 0x1d, 0x5801);
		MP_WritePhyUshort(sc, 0x1d, 0x7c04);
		MP_WritePhyUshort(sc, 0x1d, 0x5c04);
		MP_WritePhyUshort(sc, 0x1d, 0x301e);
		MP_WritePhyUshort(sc, 0x1d, 0x314d);
		MP_WritePhyUshort(sc, 0x1d, 0x31e7);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4c20);
		MP_WritePhyUshort(sc, 0x1d, 0x6004);
		MP_WritePhyUshort(sc, 0x1d, 0x5310);
		MP_WritePhyUshort(sc, 0x1d, 0x4833);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c08);
		MP_WritePhyUshort(sc, 0x1d, 0x8300);
		MP_WritePhyUshort(sc, 0x1d, 0x6800);
		MP_WritePhyUshort(sc, 0x1d, 0x6600);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0xb90c);
		MP_WritePhyUshort(sc, 0x1d, 0x30d3);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4de0);
		MP_WritePhyUshort(sc, 0x1d, 0x7c04);
		MP_WritePhyUshort(sc, 0x1d, 0x6000);
		MP_WritePhyUshort(sc, 0x1d, 0x6800);
		MP_WritePhyUshort(sc, 0x1d, 0x6736);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x5310);
		MP_WritePhyUshort(sc, 0x1d, 0x300b);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4c60);
		MP_WritePhyUshort(sc, 0x1d, 0x6803);
		MP_WritePhyUshort(sc, 0x1d, 0x6520);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0xaf03);
		MP_WritePhyUshort(sc, 0x1d, 0x6015);
		MP_WritePhyUshort(sc, 0x1d, 0x3059);
		MP_WritePhyUshort(sc, 0x1d, 0x6017);
		MP_WritePhyUshort(sc, 0x1d, 0x57e0);
		MP_WritePhyUshort(sc, 0x1d, 0x580c);
		MP_WritePhyUshort(sc, 0x1d, 0x588c);
		MP_WritePhyUshort(sc, 0x1d, 0x7ffc);
		MP_WritePhyUshort(sc, 0x1d, 0x5f83);
		MP_WritePhyUshort(sc, 0x1d, 0x4827);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c10);
		MP_WritePhyUshort(sc, 0x1d, 0x8400);
		MP_WritePhyUshort(sc, 0x1d, 0x7c30);
		MP_WritePhyUshort(sc, 0x1d, 0x6020);
		MP_WritePhyUshort(sc, 0x1d, 0x48bf);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c01);
		MP_WritePhyUshort(sc, 0x1d, 0xad09);
		MP_WritePhyUshort(sc, 0x1d, 0x7c03);
		MP_WritePhyUshort(sc, 0x1d, 0x5c03);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x4400);
		MP_WritePhyUshort(sc, 0x1d, 0xad2c);
		MP_WritePhyUshort(sc, 0x1d, 0xd6cf);
		MP_WritePhyUshort(sc, 0x1d, 0x0002);
		MP_WritePhyUshort(sc, 0x1d, 0x80f4);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4c80);
		MP_WritePhyUshort(sc, 0x1d, 0x7c20);
		MP_WritePhyUshort(sc, 0x1d, 0x5c20);
		MP_WritePhyUshort(sc, 0x1d, 0x481e);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c02);
		MP_WritePhyUshort(sc, 0x1d, 0xad0a);
		MP_WritePhyUshort(sc, 0x1d, 0x7c03);
		MP_WritePhyUshort(sc, 0x1d, 0x5c03);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x4400);
		MP_WritePhyUshort(sc, 0x1d, 0x5310);
		MP_WritePhyUshort(sc, 0x1d, 0x8d02);
		MP_WritePhyUshort(sc, 0x1d, 0x4401);
		MP_WritePhyUshort(sc, 0x1d, 0x81f4);
		MP_WritePhyUshort(sc, 0x1d, 0x3114);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4d00);
		MP_WritePhyUshort(sc, 0x1d, 0x4832);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c10);
		MP_WritePhyUshort(sc, 0x1d, 0x7c08);
		MP_WritePhyUshort(sc, 0x1d, 0x6000);
		MP_WritePhyUshort(sc, 0x1d, 0xa4b7);
		MP_WritePhyUshort(sc, 0x1d, 0xd9b3);
		MP_WritePhyUshort(sc, 0x1d, 0xfffe);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4d20);
		MP_WritePhyUshort(sc, 0x1d, 0x7e00);
		MP_WritePhyUshort(sc, 0x1d, 0x6200);
		MP_WritePhyUshort(sc, 0x1d, 0x3045);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4d40);
		MP_WritePhyUshort(sc, 0x1d, 0x7c40);
		MP_WritePhyUshort(sc, 0x1d, 0x6000);
		MP_WritePhyUshort(sc, 0x1d, 0x4401);
		MP_WritePhyUshort(sc, 0x1d, 0x5210);
		MP_WritePhyUshort(sc, 0x1d, 0x4836);
		MP_WritePhyUshort(sc, 0x1d, 0x7c08);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c08);
		MP_WritePhyUshort(sc, 0x1d, 0x4c08);
		MP_WritePhyUshort(sc, 0x1d, 0x8300);
		MP_WritePhyUshort(sc, 0x1d, 0x5f80);
		MP_WritePhyUshort(sc, 0x1d, 0x55e0);
		MP_WritePhyUshort(sc, 0x1d, 0xc06f);
		MP_WritePhyUshort(sc, 0x1d, 0x0005);
		MP_WritePhyUshort(sc, 0x1d, 0xd9b3);
		MP_WritePhyUshort(sc, 0x1d, 0xfffd);
		MP_WritePhyUshort(sc, 0x1d, 0x7c40);
		MP_WritePhyUshort(sc, 0x1d, 0x6040);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4d60);
		MP_WritePhyUshort(sc, 0x1d, 0x57e0);
		MP_WritePhyUshort(sc, 0x1d, 0x4814);
		MP_WritePhyUshort(sc, 0x1d, 0x7c04);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c04);
		MP_WritePhyUshort(sc, 0x1d, 0x4c04);
		MP_WritePhyUshort(sc, 0x1d, 0x8200);
		MP_WritePhyUshort(sc, 0x1d, 0x7c03);
		MP_WritePhyUshort(sc, 0x1d, 0x5c03);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0xad02);
		MP_WritePhyUshort(sc, 0x1d, 0x4400);
		MP_WritePhyUshort(sc, 0x1d, 0xc0e9);
		MP_WritePhyUshort(sc, 0x1d, 0x0003);
		MP_WritePhyUshort(sc, 0x1d, 0xadd8);
		MP_WritePhyUshort(sc, 0x1d, 0x30c6);
		MP_WritePhyUshort(sc, 0x1d, 0x3078);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4dc0);
		MP_WritePhyUshort(sc, 0x1d, 0x6730);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0xd09d);
		MP_WritePhyUshort(sc, 0x1d, 0x0002);
		MP_WritePhyUshort(sc, 0x1d, 0xb4fe);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4d80);
		MP_WritePhyUshort(sc, 0x1d, 0x6802);
		MP_WritePhyUshort(sc, 0x1d, 0x6600);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x7c08);
		MP_WritePhyUshort(sc, 0x1d, 0x6000);
		MP_WritePhyUshort(sc, 0x1d, 0x486c);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c01);
		MP_WritePhyUshort(sc, 0x1d, 0x9503);
		MP_WritePhyUshort(sc, 0x1d, 0x7e00);
		MP_WritePhyUshort(sc, 0x1d, 0x6200);
		MP_WritePhyUshort(sc, 0x1d, 0x571f);
		MP_WritePhyUshort(sc, 0x1d, 0x5fbb);
		MP_WritePhyUshort(sc, 0x1d, 0xaa03);
		MP_WritePhyUshort(sc, 0x1d, 0x5b58);
		MP_WritePhyUshort(sc, 0x1d, 0x30e9);
		MP_WritePhyUshort(sc, 0x1d, 0x5b64);
		MP_WritePhyUshort(sc, 0x1d, 0xcdab);
		MP_WritePhyUshort(sc, 0x1d, 0xff5b);
		MP_WritePhyUshort(sc, 0x1d, 0xcd8d);
		MP_WritePhyUshort(sc, 0x1d, 0xff59);
		MP_WritePhyUshort(sc, 0x1d, 0xd96b);
		MP_WritePhyUshort(sc, 0x1d, 0xff57);
		MP_WritePhyUshort(sc, 0x1d, 0xd0a0);
		MP_WritePhyUshort(sc, 0x1d, 0xffdb);
		MP_WritePhyUshort(sc, 0x1d, 0xcba0);
		MP_WritePhyUshort(sc, 0x1d, 0x0003);
		MP_WritePhyUshort(sc, 0x1d, 0x80f0);
		MP_WritePhyUshort(sc, 0x1d, 0x30f6);
		MP_WritePhyUshort(sc, 0x1d, 0x3109);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4ce0);
		MP_WritePhyUshort(sc, 0x1d, 0x7d30);
		MP_WritePhyUshort(sc, 0x1d, 0x6530);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x7ce0);
		MP_WritePhyUshort(sc, 0x1d, 0x5400);
		MP_WritePhyUshort(sc, 0x1d, 0x4832);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c08);
		MP_WritePhyUshort(sc, 0x1d, 0x7c08);
		MP_WritePhyUshort(sc, 0x1d, 0x6008);
		MP_WritePhyUshort(sc, 0x1d, 0x8300);
		MP_WritePhyUshort(sc, 0x1d, 0xb902);
		MP_WritePhyUshort(sc, 0x1d, 0x30d3);
		MP_WritePhyUshort(sc, 0x1d, 0x308f);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4da0);
		MP_WritePhyUshort(sc, 0x1d, 0x57a0);
		MP_WritePhyUshort(sc, 0x1d, 0x590c);
		MP_WritePhyUshort(sc, 0x1d, 0x5fa2);
		MP_WritePhyUshort(sc, 0x1d, 0xcba4);
		MP_WritePhyUshort(sc, 0x1d, 0x0005);
		MP_WritePhyUshort(sc, 0x1d, 0xcd8d);
		MP_WritePhyUshort(sc, 0x1d, 0x0003);
		MP_WritePhyUshort(sc, 0x1d, 0x80fc);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4ca0);
		MP_WritePhyUshort(sc, 0x1d, 0xb603);
		MP_WritePhyUshort(sc, 0x1d, 0x7c10);
		MP_WritePhyUshort(sc, 0x1d, 0x6010);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x541f);
		MP_WritePhyUshort(sc, 0x1d, 0x7ffc);
		MP_WritePhyUshort(sc, 0x1d, 0x5fb3);
		MP_WritePhyUshort(sc, 0x1d, 0x9403);
		MP_WritePhyUshort(sc, 0x1d, 0x7c03);
		MP_WritePhyUshort(sc, 0x1d, 0x5c03);
		MP_WritePhyUshort(sc, 0x1d, 0xaa05);
		MP_WritePhyUshort(sc, 0x1d, 0x7c80);
		MP_WritePhyUshort(sc, 0x1d, 0x5800);
		MP_WritePhyUshort(sc, 0x1d, 0x5b58);
		MP_WritePhyUshort(sc, 0x1d, 0x3128);
		MP_WritePhyUshort(sc, 0x1d, 0x7c80);
		MP_WritePhyUshort(sc, 0x1d, 0x5800);
		MP_WritePhyUshort(sc, 0x1d, 0x5b64);
		MP_WritePhyUshort(sc, 0x1d, 0x4827);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c10);
		MP_WritePhyUshort(sc, 0x1d, 0x8400);
		MP_WritePhyUshort(sc, 0x1d, 0x7c10);
		MP_WritePhyUshort(sc, 0x1d, 0x6000);
		MP_WritePhyUshort(sc, 0x1d, 0x4824);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c04);
		MP_WritePhyUshort(sc, 0x1d, 0x8200);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4cc0);
		MP_WritePhyUshort(sc, 0x1d, 0x7d00);
		MP_WritePhyUshort(sc, 0x1d, 0x6400);
		MP_WritePhyUshort(sc, 0x1d, 0x7ffc);
		MP_WritePhyUshort(sc, 0x1d, 0x5fbb);
		MP_WritePhyUshort(sc, 0x1d, 0x4824);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c04);
		MP_WritePhyUshort(sc, 0x1d, 0x8200);
		MP_WritePhyUshort(sc, 0x1d, 0x7e00);
		MP_WritePhyUshort(sc, 0x1d, 0x6a00);
		MP_WritePhyUshort(sc, 0x1d, 0x4824);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c00);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c04);
		MP_WritePhyUshort(sc, 0x1d, 0x8200);
		MP_WritePhyUshort(sc, 0x1d, 0x7e00);
		MP_WritePhyUshort(sc, 0x1d, 0x6800);
		MP_WritePhyUshort(sc, 0x1d, 0x30f6);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4e00);
		MP_WritePhyUshort(sc, 0x1d, 0x4000);
		MP_WritePhyUshort(sc, 0x1d, 0x4400);
		MP_WritePhyUshort(sc, 0x1d, 0x5310);
		MP_WritePhyUshort(sc, 0x1d, 0x6800);
		MP_WritePhyUshort(sc, 0x1d, 0x6736);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x570f);
		MP_WritePhyUshort(sc, 0x1d, 0x5fff);
		MP_WritePhyUshort(sc, 0x1d, 0xaa03);
		MP_WritePhyUshort(sc, 0x1d, 0x585b);
		MP_WritePhyUshort(sc, 0x1d, 0x315c);
		MP_WritePhyUshort(sc, 0x1d, 0x5867);
		MP_WritePhyUshort(sc, 0x1d, 0x9402);
		MP_WritePhyUshort(sc, 0x1d, 0x6200);
		MP_WritePhyUshort(sc, 0x1d, 0xcda3);
		MP_WritePhyUshort(sc, 0x1d, 0x0096);
		MP_WritePhyUshort(sc, 0x1d, 0xcd85);
		MP_WritePhyUshort(sc, 0x1d, 0x0094);
		MP_WritePhyUshort(sc, 0x1d, 0xd96b);
		MP_WritePhyUshort(sc, 0x1d, 0x0092);
		MP_WritePhyUshort(sc, 0x1d, 0x96e9);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4e20);
		MP_WritePhyUshort(sc, 0x1d, 0x6800);
		MP_WritePhyUshort(sc, 0x1d, 0x6736);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x96e2);
		MP_WritePhyUshort(sc, 0x1d, 0x8b03);
		MP_WritePhyUshort(sc, 0x1d, 0x7c08);
		MP_WritePhyUshort(sc, 0x1d, 0x5008);
		MP_WritePhyUshort(sc, 0x1d, 0x6801);
		MP_WritePhyUshort(sc, 0x1d, 0x6776);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0xdb7c);
		MP_WritePhyUshort(sc, 0x1d, 0xfff3);
		MP_WritePhyUshort(sc, 0x1d, 0x7c08);
		MP_WritePhyUshort(sc, 0x1d, 0x5000);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe1);
		MP_WritePhyUshort(sc, 0x1d, 0x4e40);
		MP_WritePhyUshort(sc, 0x1d, 0x4837);
		MP_WritePhyUshort(sc, 0x1d, 0x4418);
		MP_WritePhyUshort(sc, 0x1d, 0x4001);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4e40);
		MP_WritePhyUshort(sc, 0x1d, 0x7c40);
		MP_WritePhyUshort(sc, 0x1d, 0x5400);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c01);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c01);
		MP_WritePhyUshort(sc, 0x1d, 0x8fc9);
		MP_WritePhyUshort(sc, 0x1d, 0xd2a0);
		MP_WritePhyUshort(sc, 0x1d, 0x0041);
		MP_WritePhyUshort(sc, 0x1d, 0x9203);
		MP_WritePhyUshort(sc, 0x1d, 0xa038);
		MP_WritePhyUshort(sc, 0x1d, 0x3184);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe1);
		MP_WritePhyUshort(sc, 0x1d, 0x4e60);
		MP_WritePhyUshort(sc, 0x1d, 0x489c);
		MP_WritePhyUshort(sc, 0x1d, 0x4628);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4e60);
		MP_WritePhyUshort(sc, 0x1d, 0x7e28);
		MP_WritePhyUshort(sc, 0x1d, 0x4628);
		MP_WritePhyUshort(sc, 0x1d, 0x7c40);
		MP_WritePhyUshort(sc, 0x1d, 0x5400);
		MP_WritePhyUshort(sc, 0x1d, 0x7c01);
		MP_WritePhyUshort(sc, 0x1d, 0x5800);
		MP_WritePhyUshort(sc, 0x1d, 0x7c04);
		MP_WritePhyUshort(sc, 0x1d, 0x5c00);
		MP_WritePhyUshort(sc, 0x1d, 0x4002);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c01);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c01);
		MP_WritePhyUshort(sc, 0x1d, 0x8fb0);
		MP_WritePhyUshort(sc, 0x1d, 0xb238);
		MP_WritePhyUshort(sc, 0x1d, 0xa021);
		MP_WritePhyUshort(sc, 0x1d, 0x319d);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4ea0);
		MP_WritePhyUshort(sc, 0x1d, 0x7c02);
		MP_WritePhyUshort(sc, 0x1d, 0x4402);
		MP_WritePhyUshort(sc, 0x1d, 0x4448);
		MP_WritePhyUshort(sc, 0x1d, 0x4894);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c01);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c03);
		MP_WritePhyUshort(sc, 0x1d, 0x4824);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c07);
		MP_WritePhyUshort(sc, 0x1d, 0x4003);
		MP_WritePhyUshort(sc, 0x1d, 0x8f9e);
		MP_WritePhyUshort(sc, 0x1d, 0x92de);
		MP_WritePhyUshort(sc, 0x1d, 0xa10f);
		MP_WritePhyUshort(sc, 0x1d, 0xd480);
		MP_WritePhyUshort(sc, 0x1d, 0x0008);
		MP_WritePhyUshort(sc, 0x1d, 0xd580);
		MP_WritePhyUshort(sc, 0x1d, 0xffc2);
		MP_WritePhyUshort(sc, 0x1d, 0xa202);
		MP_WritePhyUshort(sc, 0x1d, 0x31af);
		MP_WritePhyUshort(sc, 0x1d, 0x7c04);
		MP_WritePhyUshort(sc, 0x1d, 0x4404);
		MP_WritePhyUshort(sc, 0x1d, 0x31af);
		MP_WritePhyUshort(sc, 0x1d, 0xd484);
		MP_WritePhyUshort(sc, 0x1d, 0xfff3);
		MP_WritePhyUshort(sc, 0x1d, 0xd484);
		MP_WritePhyUshort(sc, 0x1d, 0xfff1);
		MP_WritePhyUshort(sc, 0x1d, 0x314d);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4ee0);
		MP_WritePhyUshort(sc, 0x1d, 0x7c40);
		MP_WritePhyUshort(sc, 0x1d, 0x5400);
		MP_WritePhyUshort(sc, 0x1d, 0x4488);
		MP_WritePhyUshort(sc, 0x1d, 0x4004);
		MP_WritePhyUshort(sc, 0x1d, 0x314d);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4ec0);
		MP_WritePhyUshort(sc, 0x1d, 0x48f3);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c01);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c09);
		MP_WritePhyUshort(sc, 0x1d, 0x4508);
		MP_WritePhyUshort(sc, 0x1d, 0x4005);
		MP_WritePhyUshort(sc, 0x1d, 0x8f26);
		MP_WritePhyUshort(sc, 0x1d, 0xd218);
		MP_WritePhyUshort(sc, 0x1d, 0x0024);
		MP_WritePhyUshort(sc, 0x1d, 0xd2a4);
		MP_WritePhyUshort(sc, 0x1d, 0xffa8);
		MP_WritePhyUshort(sc, 0x1d, 0x31d0);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4e80);
		MP_WritePhyUshort(sc, 0x1d, 0x4832);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c01);
		MP_WritePhyUshort(sc, 0x1d, 0x7c1f);
		MP_WritePhyUshort(sc, 0x1d, 0x4c11);
		MP_WritePhyUshort(sc, 0x1d, 0x4428);
		MP_WritePhyUshort(sc, 0x1d, 0x7c40);
		MP_WritePhyUshort(sc, 0x1d, 0x5440);
		MP_WritePhyUshort(sc, 0x1d, 0x7c01);
		MP_WritePhyUshort(sc, 0x1d, 0x5801);
		MP_WritePhyUshort(sc, 0x1d, 0x7c04);
		MP_WritePhyUshort(sc, 0x1d, 0x5c04);
		MP_WritePhyUshort(sc, 0x1d, 0x4006);
		MP_WritePhyUshort(sc, 0x1d, 0xa4bc);
		MP_WritePhyUshort(sc, 0x1d, 0x31e5);
		MP_WritePhyUshort(sc, 0x1d, 0x7fe0);
		MP_WritePhyUshort(sc, 0x1d, 0x4f20);
		MP_WritePhyUshort(sc, 0x1d, 0x6800);
		MP_WritePhyUshort(sc, 0x1d, 0x6736);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x0000);
		MP_WritePhyUshort(sc, 0x1d, 0x570f);
		MP_WritePhyUshort(sc, 0x1d, 0x5fff);
		MP_WritePhyUshort(sc, 0x1d, 0xaa03);
		MP_WritePhyUshort(sc, 0x1d, 0x585b);
		MP_WritePhyUshort(sc, 0x1d, 0x31f3);
		MP_WritePhyUshort(sc, 0x1d, 0x5867);
		MP_WritePhyUshort(sc, 0x1d, 0xbcf4);
		MP_WritePhyUshort(sc, 0x1d, 0x300b);
		MP_WritePhyUshort(sc, 0x1d, 0x300b);
		MP_WritePhyUshort(sc, 0x1d, 0x314d);
		MP_WritePhyUshort(sc, 0x1f, 0x0004);
		MP_WritePhyUshort(sc, 0x1c, 0x0200);
		MP_WritePhyUshort(sc, 0x19, 0x7120);
		MP_WritePhyUshort(sc, 0x1f, 0x0000);

		if(CSR_READ_1(sc, 0xEF)&0x08)
		{
			MP_WritePhyUshort(sc, 0x1F, 0x0005);
			MP_WritePhyUshort(sc, 0x1A, 0x0004);
			MP_WritePhyUshort(sc, 0x1F, 0x0000);
		}
		else
		{
			MP_WritePhyUshort(sc, 0x1F, 0x0005);
			MP_WritePhyUshort(sc, 0x1A, 0x0000);
			MP_WritePhyUshort(sc, 0x1F, 0x0000);
		}

		if(CSR_READ_1(sc, 0xEF)&0x10)
		{
			MP_WritePhyUshort(sc, 0x1F, 0x0004);
			MP_WritePhyUshort(sc, 0x1C, 0x0000);
			MP_WritePhyUshort(sc, 0x1F, 0x0000);
		}
		else
		{
			MP_WritePhyUshort(sc, 0x1F, 0x0004);
			MP_WritePhyUshort(sc, 0x1C, 0x0200);
			MP_WritePhyUshort(sc, 0x1F, 0x0000);
		}

		MP_WritePhyUshort(sc, 0x1F, 0x0001);
		MP_WritePhyUshort(sc, 0x15, 0x7701);
		MP_WritePhyUshort(sc, 0x1F, 0x0000);
	}
	MP_WritePhyUshort(sc, 0x1F, 0x0000);
}

void MP_WritePhyUshort(struct rg_softc *sc,u_int8_t RegAddr,u_int16_t RegData)
{
	u_int32_t		TmpUlong=0x80000000;
	u_int32_t		Timeout=0;

	TmpUlong |= ( ((u_int32_t)RegAddr)<<16 | (u_int32_t)RegData );

	CSR_WRITE_4(sc, RG_PHYAR, TmpUlong );

	/* Wait for writing to Phy ok */
	for(Timeout=0; Timeout<5; Timeout++)
	{
		DELAY( 1000 );
		if((CSR_READ_4(sc, RG_PHYAR)&PHYAR_Flag)==0)
			break;
	}
}

u_int16_t MP_ReadPhyUshort(struct rg_softc *sc,u_int8_t RegAddr)
{
	u_int16_t		RegData;
	u_int32_t		TmpUlong;
	u_int32_t		Timeout=0;

	TmpUlong = ( (u_int32_t)RegAddr << 16);
	CSR_WRITE_4(sc, RG_PHYAR, TmpUlong );

	/* Wait for writing to Phy ok */
	for(Timeout=0; Timeout<5; Timeout++)
	{
		DELAY( 1000 );
		TmpUlong = CSR_READ_4(sc, RG_PHYAR);
		if((TmpUlong&PHYAR_Flag)!=0)
			break;
	}

	RegData = (u_int16_t)(TmpUlong & 0x0000ffff);

	return RegData;
}

void MP_WriteEPhyUshort(struct rg_softc *sc, u_int8_t RegAddr, u_int16_t RegData)
{
	u_int32_t		TmpUlong=0x80000000;
	u_int32_t		Timeout=0;

	TmpUlong |= ( ((u_int32_t)RegAddr<<16) | (u_int32_t)RegData );

	CSR_WRITE_4(sc, RG_EPHYAR, TmpUlong );

	/* Wait for writing to Phy ok */
	for(Timeout=0; Timeout<5; Timeout++)
	{
		DELAY( 1000 );
		if((CSR_READ_4(sc, RG_EPHYAR)&PHYAR_Flag)==0)
			break;
	}
}

u_int16_t MP_ReadEPhyUshort(struct rg_softc *sc, u_int8_t RegAddr)
{
	u_int16_t		RegData;
	u_int32_t		TmpUlong;
	u_int32_t		Timeout=0;

	TmpUlong = ( (u_int32_t)RegAddr << 16);
	CSR_WRITE_4(sc, RG_EPHYAR, TmpUlong );

	/* Wait for writing to Phy ok */
	for(Timeout=0; Timeout<5; Timeout++)
	{
		DELAY( 1000 );
		TmpUlong = CSR_READ_4(sc, RG_EPHYAR);
		if((TmpUlong&PHYAR_Flag)!=0)
			break;
	}

	RegData = (u_int16_t)(TmpUlong & 0x0000ffff);

	return RegData;
}

u_int8_t MP_ReadEfuse(struct rg_softc *sc, u_int16_t RegAddr)
{
	u_int8_t		RegData;
	u_int32_t		TmpUlong;
	u_int32_t		Timeout=0;

	RegAddr &= 0x3FF;
	TmpUlong = ( (u_int32_t)RegAddr << 8);
	CSR_WRITE_4(sc, 0xDC, TmpUlong );

	/* Wait for writing to Phy ok */
	for(Timeout=0; Timeout<5; Timeout++)
	{
		DELAY( 1000 );
		TmpUlong = CSR_READ_4(sc, 0xDC);
		if((TmpUlong&PHYAR_Flag)!=0)
			break;
	}

	RegData = (u_int8_t)(TmpUlong & 0x000000ff);

	return RegData;
}

/*----------------------------------------------------------------------------*/
/*	8139 (CR9346) 9346 command register bits (offset 0x50, 1 byte)*/
/*----------------------------------------------------------------------------*/
#define CR9346_EEDO				0x01			/* 9346 data out*/
#define CR9346_EEDI				0x02			/* 9346 data in*/
#define CR9346_EESK				0x04			/* 9346 serial clock*/
#define CR9346_EECS				0x08			/* 9346 chip select*/
#define CR9346_EEM0				0x40			/* select 8139 operating mode*/
#define CR9346_EEM1				0x80			/* 00: normal*/
#define CR9346_CFGRW			0xC0			/* Config register write*/
#define CR9346_NORM			0x00

/*----------------------------------------------------------------------------*/
/*	EEPROM bit definitions(EEPROM control register bits)*/
/*----------------------------------------------------------------------------*/
#define EN_TRNF					0x10			/* Enable turnoff*/
#define EEDO						CR9346_EEDO	/* EEPROM data out*/
#define EEDI						CR9346_EEDI		/* EEPROM data in (set for writing data)*/
#define EECS						CR9346_EECS		/* EEPROM chip select (1=high, 0=low)*/
#define EESK						CR9346_EESK		/* EEPROM shift clock (1=high, 0=low)*/

/*----------------------------------------------------------------------------*/
/*	EEPROM opcodes*/
/*----------------------------------------------------------------------------*/
#define EEPROM_READ_OPCODE	06
#define EEPROM_WRITE_OPCODE	05
#define EEPROM_ERASE_OPCODE	07
#define EEPROM_EWEN_OPCODE	19				/* Erase/write enable*/
#define EEPROM_EWDS_OPCODE	16				/* Erase/write disable*/

#define	CLOCK_RATE				50				/* us*/

#define RaiseClock(_sc,_x)				\
	(_x) = (_x) | EESK;					\
	CSR_WRITE_1((_sc), RG_EECMD, (_x));	\
	DELAY(CLOCK_RATE);

#define LowerClock(_sc,_x)				\
	(_x) = (_x) & ~EESK;					\
	CSR_WRITE_1((_sc), RG_EECMD, (_x));	\
	DELAY(CLOCK_RATE);

/*
 * Shift out bit(s) to the EEPROM.
 */
static void rg_eeprom_ShiftOutBits(sc, data, count)
	struct rg_softc		*sc;
	int			data;
	int 			count;
{
	u_int16_t x,mask;

	mask = 0x01 << (count - 1);
	x = CSR_READ_1(sc, RG_EECMD);

	x &= ~(EEDO | EEDI);

	do
	{
		x &= ~EEDI;
		if(data & mask)
			x |= EEDI;

		CSR_WRITE_1(sc, RG_EECMD, x);
		DELAY(CLOCK_RATE);
		RaiseClock(sc,x);
		LowerClock(sc,x);
		mask = mask >> 1;
	} while(mask);

	x &= ~EEDI;
	CSR_WRITE_1(sc, RG_EECMD, x);
}

/*
 * Shift in bit(s) from the EEPROM.
 */
static u_int16_t rg_eeprom_ShiftInBits(sc)
	struct rg_softc		*sc;
{
	u_int16_t x,d,i;
	x = CSR_READ_1(sc, RG_EECMD);

	x &= ~( EEDO | EEDI);
	d = 0;

	for(i=0; i<16; i++)
	{
		d = d << 1;
		RaiseClock(sc, x);

		x = CSR_READ_1(sc, RG_EECMD);

		x &= ~(EEDI);
		if(x & EEDO)
			d |= 1;

		LowerClock(sc, x);
	}

	return d;
}

/*
 * Clean up EEprom read/write setting
 */
static void rg_eeprom_EEpromCleanup(sc)
	struct rg_softc		*sc;
{
	u_int16_t x;
	x = CSR_READ_1(sc, RG_EECMD);

	x &= ~(EECS | EEDI);
	CSR_WRITE_1(sc, RG_EECMD, x);

	RaiseClock(sc, x);
	LowerClock(sc, x);
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void rg_eeprom_getword(sc, addr, dest)
	struct rg_softc		*sc;
	int			addr;
	u_int16_t		*dest;
{
	u_int16_t x;

	/* select EEPROM, reset bits, set EECS*/
	x = CSR_READ_1(sc, RG_EECMD);

	x &= ~(EEDI | EEDO | EESK | CR9346_EEM0);
	x |= CR9346_EEM1 | EECS;
	CSR_WRITE_1(sc, RG_EECMD, x);

	/* write the read opcode and register number in that order*/
	/* The opcode is 3bits in length, reg is 6 bits long*/
	rg_eeprom_ShiftOutBits(sc, EEPROM_READ_OPCODE, 3);

	if (CSR_READ_4(sc, RG_RXCFG) & RG_RXCFG_RX_9356SEL)
		rg_eeprom_ShiftOutBits(sc, addr,8);	/*93c56=8*/
	else
		rg_eeprom_ShiftOutBits(sc, addr,6);	/*93c46=6*/

	/* Now read the data (16 bits) in from the selected EEPROM word*/
	*dest=rg_eeprom_ShiftInBits(sc);

	rg_eeprom_EEpromCleanup(sc);
	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void rg_read_eeprom(sc, dest, off, cnt, swap)
	struct rg_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++)
	{
		rg_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}

	return;
}
