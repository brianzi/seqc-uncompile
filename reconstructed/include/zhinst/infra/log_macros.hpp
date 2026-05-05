// ============================================================================
// No-op logging macros.
//
// The original binary uses boost::log for LOG_WARNING / LOG_ERROR.
// Reconstructed as no-ops; wire to real boost::log if needed.
// ============================================================================

#ifndef ZHINST_LOG_MACROS_HPP
#define ZHINST_LOG_MACROS_HPP

#ifndef LOG_WARNING
#define LOG_WARNING(msg) (void)0
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(msg) (void)0
#endif

#endif // ZHINST_LOG_MACROS_HPP
