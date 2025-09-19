//
//  defines.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-19.
//

#define APL_SHORT_MIN -32768
#define APL_SHORT_MAX +32767

// comment out to get 16bit internal format.
#define APL_32
#ifdef APL_32 // float (32 bit)
#define APL_SAMPLE_TYPE float // (32 bit)
#define APL_SAMPLE_MIN -1.f
#define APL_SAMPLE_MAX +1.f
#else
#define APL_SAMPLE_TYPE short // (16 bit)
#define APL_SAMPLE_MIN APL_SHORT_MIN
#define APL_SAMPLE_MAX APL_SHORT_MAX
#endif
