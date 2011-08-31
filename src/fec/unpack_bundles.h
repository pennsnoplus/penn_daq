/** \file unpack_bundles.h */

#ifndef __UNPACK_BUNDLES_H
#define __UNPACK_BUNDLES_H

/*  unpacking "routines" by definition! Looks awful, but is fast...
 *  "a" in the following is a pointer to 3 longwords as read out from
 *  the FEC32.  from SNODAQ distribution */
#define UNPK_MISSED_COUNT(a)    ( (*(a+1) >> 28) & 0x1 )
#define UNPK_NC_CC(a)           ( (*(a+1) >> 29) & 0x1 )
#define UNPK_LGI_SELECT(a)      ( (*(a+1) >> 30) & 0x1 )
#define UNPK_CMOS_ES_16(a)      ( (*(a+1) >> 31) & 0x1 )
#define UNPK_CGT_ES_16(a)       ( (*(a) >> 30) & 0x1 )
#define UNPK_CGT_ES_24(a)       ( (*(a) >> 31) & 0x1 )
#define MY_UNPK_QLX(a)   ( (*(a+1) & 0x7FFUL) | ( ~ (*(a+1)) & 0x800UL))
#define UNPK_QLX(a)     ( (*(a+1) & 0x00000800) == 0x800 ? \
                          (*(a+1) & 0x000007FF) : \
                          (*(a+1) & 0x000007FF) + 2048 )
#define UNPK_QHS(a)     ( ((*(a+1) >>16) & 0x00000800) == 0x800 ? \
                          ((*(a+1)>>16) & 0x000007FF) : \
                          ((*(a+1) >>16) & 0x000007FF) + 2048 )
#define UNPK_QHL(a)     ( (*(a+2) & 0x00000800)  == 0x800 ? \
                          (*(a+2) &0x000007FF) : (*(a+2) & 0x000007FF) + 2048 )
#define UNPK_TAC(a)     ( ((*(a+2) >>16) & 0x00000800)  == 0x800 ? \
                          ((*(a+2)>>16) & 0x000007FF) : \
                          ((*(a+2) >>16) & 0x000007FF) + 2048 )
#define UNPK_CELL_ID(a)         ( (*(a+1) >> 12) & 0x0000000F )
#define UNPK_CHANNEL_ID(a)      ( (*(a) >> 16) & 0x0000001F )
#define UNPK_BOARD_ID(a)        ( (*(a) >> 26) & 0x0000000F )
#define UNPK_CRATE_ID(a)        ( (*(a) >> 21) & 0x0000001F )
#define UNPK_FEC_GT24_ID(a)  ( *(a) & 0x0000FFFF ) | \
                                ( (*(a+2) << 4) &0x000F0000 )  | \
                                ( (*(a+2) >> 8) & 0x00F00000 )
#define UNPK_FEC_GT16_ID(a)  ( *(a) & 0x0000FFFF ) 
#define UNPK_FEC_GT8_ID(a)   ( (*(a+2) >> 24) &0x000000F0 ) | \
                                ( (*(a+2) >> 12) & 0x0000000F )

#ifdef NO_RUSHDY_BOARDS
#       define UNPK_FEC_GT_ID(a)    ( *(a) & 0x0000FFFF ) //this is for Peter
                                  // (when CGT was fubar'd)
#else                                  
#       define UNPK_FEC_GT_ID(a)    ( *(a) & 0x0000FFFF ) | \
                                  ( (*(a+2) << 4) &0x000F0000 )  | \
                                  ( (*(a+2) >> 8) & 0x00F00000 )
#endif


#endif
