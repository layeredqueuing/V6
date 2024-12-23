/* drand48.h	-- Greg Franks
 *
 * $HeadURL$
 * $Id: drand48.h 17458 2024-11-12 11:54:17Z greg $
 */

#ifndef _DRAND48_H
#define _DRAND48_H

#if defined(__cplusplus)
extern "C" {
#endif
    
extern double drand48();
extern void srand48( long );

#if defined(__cplusplus)
}
#endif
#endif	/* _DRAND48_H */
