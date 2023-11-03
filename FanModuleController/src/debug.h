/*
 * debug.h
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "config.h"

#ifdef CFG_DEBUG_ENABLE
#define debug_printf(args...) printf(args)
#else
#define debug_printf(args...)
#endif

#endif /* __DEBUG_H__ */