/*
 * The IKE Scanner (ike-scan) is Copyright (C) 2003 Roy Hills, NTA Monitor Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * If this license is unacceptable to you, I may be willing to negotiate
 * alternative licenses (contact ike-scan@nta-monitor.com).
 *
 * You are encouraged to send comments, improvements or suggestions to
 * me at ike-scan@nta-monitor.com.
 *
 * $Id$
 *
 * Author: Roy Hills
 * Date: 7 November 2003
 *
 * Functions to construct ISAKMP headers and payloads.
 *
 */

#include "ike-scan.h"

static char rcsid[] = "$Id$";	/* RCS ID for ident(1) */

/*
 *	make_isakmp_hdr -- Construct an ISAKMP Header
 *
 *	Inputs:
 *
 *	xchg	Exchange Type (e.g. ISAKMP_XCHG_IDPROT for main mode)
 *	next	Next Payload Type
 *	length	ISAKMP Message total length
 *
 *	Returns:
 *
 *	Pointer to created ISAKMP Header.
 *
 *	This constructs an ISAKMP header.  It fills in the static values.
 *	The initator cookie should be changed to a unique per-host value
 *	before the packet is sent.
 */
struct isakmp_hdr*
make_isakmp_hdr(uint8_t xchg, uint8_t next, uint32_t length) {
   struct isakmp_hdr* hdr;

   hdr = Malloc(sizeof(struct isakmp_hdr));
   memset(hdr, '\0', sizeof(struct isakmp_hdr));

   hdr->isa_icookie[0] = 0xdeadbeef;	/* Initiator cookie */
   hdr->isa_icookie[1] = 0xdeadbeef;
   hdr->isa_rcookie[0] = 0;		/* Set responder cookie to 0 */
   hdr->isa_rcookie[1] = 0;
   hdr->isa_np = next;			/* Next Payload Type */
   hdr->isa_version = 0x10;		/* v1.0 */
   hdr->isa_xchg = xchg;		/* Exchange type */
   hdr->isa_flags = 0;			/* No flags */
   hdr->isa_msgid = 0;			/* MBZ for phase-1 */
   hdr->isa_length = htonl(length);	/* Total ISAKMP message length */

   return hdr;
}

/*
 *	make_sa_hdr -- Construct an SA Header
 *
 *	Inputs:
 *
 *	next    Next Payload Type
 *	length	SA payload length
 *
 *	Returns:
 *
 *	Pointer to SA Header.
 *
 *	This constructs an SA header.  It fills in the static values.
 */
struct isakmp_sa*
make_sa_hdr(uint8_t next, uint32_t length) {
   struct isakmp_sa* hdr;

   hdr = Malloc(sizeof(struct isakmp_sa));
   memset(hdr, '\0', sizeof(struct isakmp_sa));

   hdr->isasa_np = next;		/* Next Payload Type */
   hdr->isasa_length = htons(length);		/* SA Payload length */
   hdr->isasa_doi = htonl(ISAKMP_DOI_IPSEC);	/* IPsec DOI */
   hdr->isasa_situation = htonl(SIT_IDENTITY_ONLY);	/* Exchange type */

   return hdr;
}

/*
 *	make_prop -- Construct a proposal payload
 *
 *	Inputs:
 *
 *	length	Proposal payload length
 *	notrans	Number of transforms in this proposal
 *
 *	Returns:
 *
 *	Pointer to proposal payload.
 *
 *	This constructs a proposal payload.  It fills in the static values.
 *	We assume only one proposal will be created.  I think that ISAKMP SAs
 *	are only allowed to have one proposal anyway.
 */
struct isakmp_proposal*
make_prop(uint32_t length, uint8_t notrans) {
   struct isakmp_proposal* hdr;

   hdr = Malloc(sizeof(struct isakmp_proposal));
   memset(hdr, '\0', sizeof(struct isakmp_proposal));

   hdr->isap_np = 0;			/* No more proposals */
   hdr->isap_length = htons(length);	/* Proposal payload length */
   hdr->isap_proposal = 1;		/* Proposal #1 */
   hdr->isap_protoid = PROTO_ISAKMP;
   hdr->isap_spisize = 0;		/* No SPI */
   hdr->isap_notrans = notrans;		/* Number of transforms */

   return hdr;
}

/*
 *	make_trans -- Construct a single transform payload
 *
 *	Inputs:
 *
 *	length	(output) length of entire transform payload.
 *	next    Next Payload Type (3 = More transforms; 0=No more transforms)
 *	number	Transform number
 *	cipher	The encryption algorithm
 *	keylen	Key length for variable length keys (0=fixed key length)
 *	hash	Hash algorithm
 *	auth	Authentication method
 *	group	DH Group number
 *	lifetime	Lifetime in seconds (0=no lifetime)
 *
 *	Returns:
 *
 *	Pointer to transform payload.
 *
 *	This constructs a single transform payload.
 *	Most of the values are defined in RFC 2409 Appendix A.
 */
unsigned char*
make_trans(int *length, uint8_t next, uint8_t number, uint16_t cipher,
           uint16_t keylen, uint16_t hash, uint16_t auth, uint16_t group,
           uint32_t lifetime) {

   struct isakmp_transform* hdr;	/* Transform header */
   struct isakmp_attribute* attr1;	/* Mandatory attributes */
   struct isakmp_attribute* attr2=NULL;	/* Optional keylen attribute */
   struct isakmp_attribute* attr3=NULL;	/* Optional lifetype attribute */
   struct isakmp_attribute_l32* attr4=NULL; /* Optional lifetime attribute */
   unsigned char *payload;
   unsigned char *cp;
   int len;				/* Payload Length */

/* Allocate and initialise the transform header */

   hdr = Malloc(sizeof(struct isakmp_transform));
   memset(hdr, '\0', sizeof(struct isakmp_transform));

   hdr->isat_np = next;			/* Next payload type */
   hdr->isat_transnum = number;		/* Transform Number */
   hdr->isat_transid = KEY_IKE;

/* Allocate and initialise the mandatory attributes */

   attr1 = Malloc(4 * sizeof(struct isakmp_attribute));

   attr1[0].isaat_af_type = htons(0x8001);	/* Encryption Algorithm */
   attr1[0].isaat_lv = htons(cipher);
   attr1[1].isaat_af_type = htons(0x8002);	/* Hash Algorithm */
   attr1[1].isaat_lv = htons(hash);
   attr1[2].isaat_af_type = htons(0x8003);	/* Authentication Method */
   attr1[2].isaat_lv = htons(auth);
   attr1[3].isaat_af_type = htons(0x8004);	/* Group Description */
   attr1[3].isaat_lv = htons(group);

   len = sizeof(struct isakmp_transform) + 4 * sizeof(struct isakmp_attribute);

/* Allocate and initialise the optional attributes */

   if (keylen) {
      attr2 = Malloc(sizeof(struct isakmp_attribute));
      attr2->isaat_af_type = htons(0x800e);	/* Key Length */
      attr2->isaat_lv = htons(keylen);
      len += sizeof(struct isakmp_attribute);
   }

   if (lifetime) {
      attr3 = Malloc(sizeof(struct isakmp_attribute));
      attr4 = Malloc(sizeof(struct isakmp_attribute_l32));
      attr3->isaat_af_type = htons(0x800b);	/* Life Type */
      attr3->isaat_lv = htons(1);		/* Seconds */
      attr4->isaat_af_type = htons(0x000c);	/* Life Duratiion */
      attr4->isaat_l = htons(4);		/* 4 Bytes- CANT CHANGE */
      attr4->isaat_v = htonl(lifetime);		/* Lifetime in seconds */
      len += sizeof(struct isakmp_attribute) +
             sizeof(struct isakmp_attribute_l32);
   }

/* Fill in length value now we know it */

   hdr->isat_length = htons(len);	/* Transform length */
   *length = len;

/* Allocate memory for payload and copy structures to payload */

   payload = Malloc(len);

   cp = payload;
   memcpy(cp, hdr, sizeof(struct isakmp_transform));
   free(hdr);
   cp += sizeof(struct isakmp_transform);
   memcpy(cp, attr1, 4 * sizeof(struct isakmp_attribute));
   free(attr1);
   cp += 4 * sizeof(struct isakmp_attribute);
   if (keylen) {
      memcpy(cp, attr2, sizeof(struct isakmp_attribute));
      free(attr2);
      cp += sizeof(struct isakmp_attribute);
   }
   if (lifetime) {
      memcpy(cp, attr3, sizeof(struct isakmp_attribute));
      free(attr3);
      cp += sizeof(struct isakmp_attribute);
      memcpy(cp, attr4, sizeof(struct isakmp_attribute_l32));
      free(attr4);
      cp += sizeof(struct isakmp_attribute_l32);
   }


   return payload;
}

/*
 *	add_trans -- Add a transform payload onto the set of transforms.
 *
 *	Inputs:
 *
 *	finished	0 if adding a new transform; 1 if finalising.
 *	length	(output) length of entire transform payload.
 *	next    Next Payload Type (3 = More transforms; 0=No more transforms)
 *	number	Transform number
 *	cipher	The encryption algorithm
 *	keylen	Key length for variable length keys (0=fixed key length)
 *	hash	Hash algorithm
 *	auth	Authentication method
 *	group	DH Group number
 *	lifetime	Lifetime in seconds (0=no lifetime)
 *
 *	Returns:
 *
 *	Pointer to new set of transform payloads.
 */
unsigned char*
add_trans(int finished, int *length,
          uint16_t cipher, uint16_t keylen, uint16_t hash, uint16_t auth,
          uint16_t group, uint32_t lifetime) {

   static int first_transform = 1;
   static unsigned char *trans_start=NULL;	/* Start of set of transforms */
   static int cur_offset;			/* Start of current transform */
   static int end_offset;			/* End of transforms */
   static int trans_no=1;
   unsigned char *trans;			/* Transform payload */
   int len;					/* Transform length */
/*
 * Construct a transform if we are not finalising.
 * Set next to 3 (more transforms), and increment trans_no for next time round.
 */
   if (!finished) {
      trans = make_trans(&len, 3, trans_no, cipher, keylen, hash, auth,
                         group, lifetime);
      trans_no++;
      if (first_transform) {
         cur_offset = 0;
         end_offset = len;
         trans_start = Malloc(end_offset);
         memcpy(trans_start, trans, len);
         first_transform = 0;
      } else {
         cur_offset = end_offset;
         end_offset += len;
         trans_start = Realloc(trans_start, end_offset);
         memcpy(trans_start+cur_offset, trans, len);
      }
      return NULL;
   } else {
      struct isakmp_transform* hdr =
         (struct isakmp_transform*) (trans_start+cur_offset);	/* Overlay */

      hdr->isat_np = 0;		/* No more transforms */
      *length = end_offset;
      return trans_start;
   }
}

/*
 *	make_vid -- Construct a vendor id payload
 *
 *	Inputs:
 *
 *	length	(output) length of entire transform payload.
 *	next		Next Payload Type
 *	vid_data	Vendor ID data
 *	vid_data_len	Vendor ID data length
 *
 *	Returns:
 *
 *	Pointer to vendor id payload.
 *
 *	This constructs a vendor id payload.  It fills in the static values.
 *	The next pointer value must be filled in later.
 */
unsigned char*
make_vid(int *length, uint8_t next, unsigned char *vid_data, int vid_data_len) {
   unsigned char *payload;
   struct isakmp_vid* hdr;

   payload = Malloc(sizeof(struct isakmp_vid)+vid_data_len);
   hdr = (struct isakmp_vid*) payload;	/* Overlay vid struct on payload */
   memset(hdr, '\0', sizeof(struct isakmp_vid));

   hdr->isavid_np = next;		/* Next payload type */
   hdr->isavid_length = htons(sizeof(struct isakmp_vid)+vid_data_len);

   memcpy(payload+sizeof(struct isakmp_vid), vid_data, vid_data_len);
   *length = sizeof(struct isakmp_vid) + vid_data_len;

   return payload;
}

/*
 *	add_vid -- Add a vendor ID payload to the set of VIDs.
 *
 *	Inputs:
 *
 *      finished        0 if adding a new VIDs; 1 if finalising.
 *      length  (output) length of entire VID payload set.
 *      vid_data        Vendor ID data
 *      vid_data_len    Vendor ID data length
 */
unsigned char*
add_vid(int finished, int *length, unsigned char *vid_data, int vid_data_len) {
   static int first_vid = 1;
   static unsigned char *vid_start=NULL;	/* Start of set of VIDs */
   static int cur_offset;			/* Start of current VID */
   static int end_offset;			/* End of VIDs */
   unsigned char *vid;				/* VID payload */
   int len;					/* VID length */
/*
 * Construct a VID if we are not finalising.
 */
   if (!finished) {
      vid = make_vid(&len, ISAKMP_NEXT_VID, vid_data, vid_data_len);
      if (first_vid) {
         cur_offset = 0;
         end_offset = len;
         vid_start = Malloc(end_offset);
         memcpy(vid_start, vid, len);
         first_vid = 0;
      } else {
         cur_offset = end_offset;
         end_offset += len;
         vid_start = Realloc(vid_start, end_offset);
         memcpy(vid_start+cur_offset, vid, len);
      }
      return NULL;
   } else {
      struct isakmp_vid* hdr =
         (struct isakmp_vid*) (vid_start+cur_offset);   /* Overlay */

      hdr->isavid_np = ISAKMP_NEXT_NONE;         /* No more payloads */
      *length = end_offset;
      return vid_start;
   }
}

void isakmp_use_rcsid(void) {
   printf("%s\n", rcsid);	/* Use rcsid to stop compiler optimising away */
}