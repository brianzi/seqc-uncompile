// Serial number predicates — device family identification by serial range
// Reconstructed from _seqc_compiler.so
//
// Each device family has a "short" serial range and a "long" serial range
// (10× the base, 5-10× the count). PQSC is the only single-range family.

#pragma once

#include <cstdint>

namespace zhinst {

//! \brief Test whether `serial` belongs to the **HF2** device
//!        family.
//!
//! \details Matches the half-open ranges
//! `[0, 2000)` and `[18000, 20000)`.
//!
//! \param serial  Numeric device serial.
//! \return  `true` iff `serial` falls in the HF2 family ranges.
bool isHf2Serial(unsigned int serial);

//! \brief Test whether `serial` belongs to the **UHF** device
//!        family (UHFLI / UHFQA / UHFAWG).
//!
//! \details Matches `[2000, 3000)` and `[20000, 22000)`.
//!
//! \param serial  Numeric device serial.
//! \return  `true` iff `serial` is a UHF-family serial.
bool isUhfSerial(unsigned int serial);

//! \brief Test whether `serial` belongs to the **MF** device
//!        family.
//!
//! \details Matches `[3000, 8000)` and `[30000, 40000)`.
//!
//! \param serial  Numeric device serial.
//! \return  `true` iff `serial` is an MF-family serial.
bool isMfSerial(unsigned int serial);

//! \brief Test whether `serial` belongs to the **HDAWG**
//!        device family.
//!
//! \details Matches `[8000, 10000)` and `[80000, 90000)`.
//!
//! \param serial  Numeric device serial.
//! \return  `true` iff `serial` is an HDAWG-family serial.
bool isHdawgSerial(unsigned int serial);

//! \brief Test whether `serial` belongs to the **PQSC**
//!        device family.
//!
//! \details Matches the single range `[10000, 12000)` —
//! PQSC is the only family without an extended high range.
//!
//! \param serial  Numeric device serial.
//! \return  `true` iff `serial` is a PQSC serial.
bool isPqscSerial(unsigned int serial);

//! \brief Test whether `serial` belongs to the **SHF**
//!        device family (base SHF: SHFQA / SHFSG / SHFQC).
//!
//! \details Matches `[12000, 13000)` and
//! `[120000, 130000)`.  Excludes the SHF-Acc extension —
//! see `isShfaccSerial`.
//!
//! \param serial  Numeric device serial.
//! \return  `true` iff `serial` is a base-SHF serial.
bool isShfSerial(unsigned int serial);

//! \brief Test whether `serial` belongs to the **SHF-Acc**
//!        device family (SHF-line accessory units).
//!
//! \details Matches `[16000, 17000)` and
//! `[160000, 170000)`.
//!
//! \param serial  Numeric device serial.
//! \return  `true` iff `serial` is an SHF-Acc serial.
bool isShfaccSerial(unsigned int serial);

//! \brief Test whether `serial` belongs to the **GHF**
//!        device family (GHFLI).
//!
//! \details Matches `[22000, 23000)` and
//! `[220000, 230000)`.
//!
//! \param serial  Numeric device serial.
//! \return  `true` iff `serial` is a GHF-family serial.
bool isGhfSerial(unsigned int serial);

//! \brief Test whether `serial` belongs to the **QHub**
//!        device family.
//!
//! \details Matches `[24000, 25000)` and
//! `[240000, 250000)`.
//!
//! \param serial  Numeric device serial.
//! \return  `true` iff `serial` is a QHub-family serial.
bool isQhubSerial(unsigned int serial);

//! \brief Test whether `serial` belongs to the **VHF**
//!        device family (VHFLI).
//!
//! \details Matches `[13000, 14000)` and
//! `[130000, 140000)`.
//!
//! \param serial  Numeric device serial.
//! \return  `true` iff `serial` is a VHF-family serial.
bool isVhfSerial(unsigned int serial);

} // namespace zhinst
