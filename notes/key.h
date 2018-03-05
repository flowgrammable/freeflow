#ifndef KEY_H
#define KEY_H

// example key with byte-aligned fields
// - may need gathering when sending to table
typedef struct {
 u64 ethSrc;
 u64 ethDst;
 u16 ethType;
 u16 vlanID;
 u8  ipv4Proto;
 u32 ipv4Src;
 u32 ipv4Dst;
 u16 tcpSrcPort;
 u16 tcpDstPort;
 u16 udpSrcPort;
 u16 udpDstPort;
} Key;

#endif /* KEY_H */

