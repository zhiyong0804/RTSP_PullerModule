//=============================================================================
/**
*  @file	cpu_arch.h
*
*  @author	S.Y.W  
*  @date	2010/09/15
*
*  @brief	CPU architecture
*/
//=============================================================================

#ifndef __CPU_ARCH_H__
#define __CPU_ARCH_H__

#if defined (_WIN32) || defined (_WIN64)
#define _OS_WIN
#else
#define _OS_LINUX
#endif

#if defined (_WIN64)
#define _CPU_64
#elif defined (__x86_64__)
#define _CPU_64
#else
#define _CPU_32
#endif

#ifndef INTEL
// current only support intel arch
#define INTEL
#endif

typedef char		char_t;
//typedef char		int8_t;
typedef unsigned char	byte_t;
typedef unsigned char	uint8_t;

typedef short		int16_t;
typedef unsigned short	uint16_t;	

#ifdef _CPU_64
    typedef int			int32_t;
    typedef unsigned int	uint32_t;
    typedef long 		int64_t;
    typedef unsigned long	uint64_t;
#else
    typedef int			int32_t;
    typedef unsigned int	uint32_t;
    typedef long long		int64_t;
    typedef unsigned long long	uint64_t;
#endif

#endif // __CPU_ARCH_H__
