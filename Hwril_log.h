#ifndef HWRIL_LOG_H
#define HWRIL_LOG_H

/*===========================================================================

                           INCLUDE FILES

===========================================================================*/

 #include <utils/Log.h> 

/*===========================================================================

                        DEFINITIONS AND TYPES

===========================================================================*/


#define HWRIL_LOG_TAG "RIL" //"RIL" for test, for release version, it should be "HWRIL"

#define HWRIL_LOG_ASSERT(tag, xx_exp)                                                                          \
   if ( !( xx_exp ) )                                                                                          \
   {                                                                                                           \
     HWRIL_LOG_WARN( tag, "*********************************** ASSERTION *********************************" ); \
     HWRIL_LOG_WARN( tag, "File: %s, Line: %d, Cond: %s", __FILE__, __LINE__, #xx_exp );                       \
     HWRIL_LOG_WARN( tag, "*******************************************************************************" ); \
   }

#define HWRIL_LOG_ERROR(tag, ...)          { LOG( LOG_ERROR, tag, __VA_ARGS__ ); }

#define HWRIL_LOG_WARN(tag, ...)           { LOG( LOG_WARN, tag, __VA_ARGS__ ); }

#define HWRIL_LOG_DEBUG(tag, ...)          { LOG( LOG_DEBUG, tag, __VA_ARGS__ ); }

#define HWRIL_LOG_INFO(tag, ...)             { LOG( LOG_INFO, tag, __VA_ARGS__ ); }

#define HWRIL_LOG_VERBOSE(tag, ...)       { LOG( LOG_VERBOSE, tag, __VA_ARGS__ ); }

#define HWRIL_LOG_NULL(tag,...)

#define HWRIL_LOG_TEST  HWRIL_LOG_NULL
#endif /* HWRIL_LOG_H */

