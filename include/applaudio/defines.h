//
//  defines.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-09-19.
//

#define APL_SHORT_LIMIT 32768
#define APL_SHORT_LIMIT_F static_cast<float>(APL_SHORT_LIMIT)
#define APL_SHORT_MIN -APL_SHORT_LIMIT
#define APL_SHORT_MAX +(APL_SHORT_LIMIT - 1)
#define APL_SHORT_MIN_F static_cast<float>(APL_SHORT_MIN)
#define APL_SHORT_MAX_F static_cast<float>(APL_SHORT_MAX)
#define APL_SHORT_MIN_L static_cast<long>(APL_SHORT_MIN)
#define APL_SHORT_MAX_L static_cast<long>(APL_SHORT_MAX)

// comment out to get 16bit internal format.
#define APL_32
#ifdef APL_32 // float (32 bit)
#define APL_SAMPLE_TYPE float // (32 bit)
#define APL_SAMPLE_MIN -1.f
#define APL_SAMPLE_MAX +1.f
#define APL_SAMPLE_SCALE APL_SHORT_LIMIT_F
#else
#define APL_SAMPLE_TYPE short // (16 bit)
#define APL_SAMPLE_MIN APL_SHORT_MIN
#define APL_SAMPLE_MAX APL_SHORT_MAX
#define APL_SAMPLE_SCALE 1.f
#endif
