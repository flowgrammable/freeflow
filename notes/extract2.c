
#include <string.h>
//#include <netinet/in.h>  // temporary for ntoh calls, replace later!
#include "util.h"
#include "key.h"  // definition of Key struct

#define DEBUG
#ifdef DEBUG
#include <stdio.h>  // temporary for debug printf's
#endif

// function to extract packet into global key
// Arguments:
// - pkt: pointer to a linear, virtually contiguous packet
// - pktLen: length of packet (in bytes); max is implicitly 64kB
// Returns by Argument Manipulation:
// - payload: indicating the end of the last extracted header.  Indicates
//   start of effective payload (if one exists, be sure to check pktLen)
// - gKey: extracted fields to key
void extract(View* payload, Key* gKey, void* pkt, u16 pktLen) {
	// Silly hack for run-time endianess check:
	endianTest();

	// effectively provides a view of packet payload:
  payload->begin = pkt;      // indicates beginning of actual payload
  payload->end = pkt+pktLen; // indicates end of actual payload

  void* cur = pkt;   // tracks position in packet during extract

  //// Extract Ethernet ////
  // - assumes first header is a valid ethernet frame
  void* ethBegin = cur;
	gKey->ethDst = betoh64( *(u64*)cur ) & 0x0000FFFFFFFFFFFFUll;
	//memcpy(&gKey->ethDst, cur, 6);	// choosing to omit network byte-order
	cur += 6;
	gKey->ethSrc = betoh64( *(u64*)cur ) & 0x0000FFFFFFFFFFFFUll;
	//memcpy(&gKey->ethSrc, cur, 6); // choosing to omit network byte-order
	cur += 6;
	// differ extract of ethertype until after VLAN check/extract
	u16 etherType = betoh16( *(u16*)cur );
	cur += 2;

	// grab frame check sequence at end of packet
	u32 ethCRC = betoh32( *(u32*)(pkt+(pktLen-4)) );
	//memcpy(&ethCRC, pkt+(pktLen-4), 4); // ignoring byte-order for now

	// extract top VLAN frame:
	if (etherType == VLAN) {
	  //uint8_t priority = (0xE0 & *v.get())>>5;
    //bool dei = (0x10 & *v.get()) == 0x10;
		gKey->vlanID = betoh16( *(u16*)cur ) & 0x1FFF;
		//memcpy(gKey->vlanID, &id, 2);
    cur += 2;
    etherType = betoh16( *(u16*)cur );
    cur += 2;

		// skip any remaining VLAN tags
		while (etherType == VLAN) {
			cur += 2;
   		etherType = betoh16( *(u16*)cur );
   		cur += 2;
		}
	}
#ifdef DEBUG
	printf("extract: etherType=%X\n", etherType);
#endif
	//memcpy(gKey->ethType, &etherType, 2);  // finally resolved ethertype
	gKey->ethType = etherType;  // finally resolved ethertype

	// restrict payload view after extracting ethernet + [VLAN/s]:
	payload->begin += (cur - pkt);
	payload->end -= 4;


  //// Extract IPv4 ////
  if (etherType == IPV4) {
    void* ipv4Begin = cur;

    u8 tmp = *(u8*)cur;
    u8 ipv4Version = (tmp & 0xF0)>>4;
    u8 ihl = tmp & 0x0F;  // number of 32-bit words in header
    cur += 1;
    if (ipv4Version != 4) {
#ifdef DEBUG
			printf("extract: Abort IPv4! ipv4Version=%i\n", ipv4Version);
#endif
      goto Abort;   // yes, I know. you are appalled...
    }
    // check ihl field for consistency
    // min: 5
    // max: remaining payload (view)
    if (ihl < 5 || ihl*4 > (payload->end - payload->begin)) {
#ifdef DEBUG
			printf("extract: Abort IPv4! ihl=%i\n", ihl);
#endif
      goto Abort; // malformed ipv4
    }

    //uint8_t dscp = (0xFC & *v.get())>>2;
    //uint8_t ecn = 0x02 & *v.get();
    cur += 1;

    u16 ipv4Length = betoh16( *(u16*)cur );		// bytes in packet including ipv4 header
    cur += 2;
    // check ipv4Length field for consistency, should exactly match view
    if (ipv4Length != (payload->end - payload->begin)) {
#ifdef DEBUG
			printf("extract: Abort IPv4! ipv4Length=%i\n", ipv4Length);
#endif
      goto Abort; // malformed ipv4
    }

    //uint16_t id = bs16(*v.get());
    cur += 2;

    //bool df = (0x40 & *v.get()) == 0x40;
    //bool mf = (0x20 & *v.get()) == 0x20;
    //uint16_t frag_offset = 0x1FFF & bs16(*(uint16_t*)v.get());	// 64-bit block offset relative to original
    cur += 2;

    //uint8_t ttl = *v.get();
    cur += 1;

    u8 proto = *(u8*)cur;
		gKey->ipv4Proto = proto;
    //memcpy(gKey->ipv4Proto, &proto, 1);
    cur += 1;

    //uint16_t checksum = bs16(*(uint16_t*)v.get());
    cur += 2;

    gKey->ipv4Src = betoh32( *(u32*)cur );
		//memcpy(gKey->ipv4Src, &source, 4);
    cur += 4;

    gKey->ipv4Dst = betoh32( *(u32*)cur );
		//memcpy(gKey->ipv4Dst, &dest, 4);
    cur += 4;

    // BEGIN IPv4 options
    if (ihl > 5) {
			// do special IPv4 options handling here as needed
      cur += (ihl-5)*4;
    }
    // END IPv4 options

    // restrict payload view after extracting IPv4:
    payload->begin += (cur - ipv4Begin);
    payload->end += 0;  // no change


    //// Extract TCP ////
    if (proto == TCP) {
      void* tcpBegin = cur;

      gKey->tcpSrcPort = betoh16( *(u16*)cur );
      //memcpy(gKey->tcpSrcPort, &source, 2);
      cur += 2;
      gKey->tcpDstPort = betoh16( *(u16*)cur );
      //memcpy(gKey->tcpDstPort, &dest, 2);
      cur += 2;

      //uint32_t seq_num = bs32(*(uint32_t*)v.get());
      cur += 4;

      //uint32_t ack_num = bs32(*(uint32_t*)v.get());
      cur += 4;

      u8 offset = (0xF0 & *(u8*)cur)>>4;	// size of TCP header in 4B words
      cur += 2; // {offset, flags}
      // END flags
			
			// check TCP header offset is consistant with packet size
      if (offset < 4 || offset*4 > (payload->end - payload->begin)) {
#ifdef DEBUG
				printf("extract: Abort TCP! offset=%i\n", offset);
#endif
        goto Abort;  // Malformed TCP
      }

      //uint16_t window = bs16(*(uint16_t*)v.get());
      cur += 2;
      //uint16_t checksum = bs16(*(uint16_t*)v.get());
      cur += 2;
      //uint16_t urg_num = bs16(*(uint16_t*)v.get());
      cur += 2;

      // BEGIN TCP options
      if (offset > 5) {
        // handle TCP options as needed...
        cur += (offset-5)*4;
      }
      // END TCP options

      // restrict payload view after extracting TCP:
      payload->begin += (cur - tcpBegin);
      payload->end += 0;  // no change

      //return payload; // Success!
			return;
    }
    else if (proto == UDP) {
      void* udpBegin = cur;

      gKey->udpSrcPort = betoh16( *(u16*)cur );
      //memcpy(gKey->udpSrcPort, &source, 2);
      cur += 2;

      gKey->udpDstPort = betoh16( *(u16*)cur );
      //memcpy(gKey->udpDstPort, &dest, 2);
      cur += 2;

      u16 udpLen = betoh16( *(u16*)cur );  // length (in bytes) of UDP header and Payload
      cur += 2;
      if (udpLen < 8 || udpLen > (payload->end - payload->begin)) {
#ifdef DEBUG
				printf("extract: Abort UDP! udpLen=%i\n", udpLen);
#endif
        goto Abort;  // Malformed UDP
      }

      //uint16_t checksum = bs16(*(uint16_t*)v.get());
      cur += 2;

      // restrict payload view after extracting TCP:
      payload->begin += (cur - udpBegin);
      payload->end += 0;  // no change

      //return payload; // Success!
			return;
    }

		// Unknown IPv4 Protocol, don't know how to handle...
#ifdef DEBUG
		printf("extract: Abort IPv4! proto=%i\n", proto);
#endif
		goto Abort;
  }

	
  // Unknown etherType, don't know how to handle...
#ifdef DEBUG
	printf("extract: Abort Ethernet! etherType=%i\n", etherType);
#endif

  Abort:
  // NULLs out safe range of payload before returning - indicates an error
  payload->begin = NULL;
  payload->end = NULL;
  //return payload;
	return;
}
