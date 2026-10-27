/*!
 * @file OpenFIRETracking.h
 * @brief Selects between the classic and the advanced IR tracking.
 *
 * Advanced tracking (roll tracking, rebuilding of hidden emitters, smooth transitions between
 * visible-emitter counts, lens correction) runs float-heavy maths on every camera frame.
 * It is only built for RP2350 (ARM), which has a hardware FPU:
 *  - RP2350 (ARM): enabled automatically. Define NO_ADVANCED_TRACKING as a build flag to use the
 *    classic tracking there instead.
 *  - RP2040: has no FPU, so it can't select advanced tracking. Defining ADVANCED_TRACKING there
 *    is a build error.
 *
 * The classic and advanced classes share the same names and public interface, so only one of the
 * two is ever compiled; see the #ifdef ADVANCED_TRACKING guards in OpenFIRE_*.h/.cpp.
 *
 * @copyright GNU Lesser General Public License
 */

#ifndef _OPENFIRETRACKING_H_
#define _OPENFIRETRACKING_H_

#if defined(ARDUINO_ARCH_RP2040) && !(defined(PICO_RP2350) && defined(__ARM_FP))
    // RP2040, or an RP2350 build without an FPU (RISC-V)
    #ifdef ADVANCED_TRACKING
        #error "ADVANCED_TRACKING needs a hardware FPU: it is only available on RP2350 (ARM), not on RP2040."
    #endif // ADVANCED_TRACKING
#elif !defined(NO_ADVANCED_TRACKING) && !defined(ADVANCED_TRACKING)
    #define ADVANCED_TRACKING
#endif // RP2040 / RP2350

#endif // _OPENFIRETRACKING_H_
