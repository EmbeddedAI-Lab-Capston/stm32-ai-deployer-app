#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

// STM32N6 images live in AXISRAM at kLoadAddress. The chip has no internal
// flash: under LRUN boot the FSBL copies the signed image there, and the "ram"
// deploy mode writes it there directly over SWD.
//
// Two places start such an image - PipelineRunner after a build, and Backend
// when it restarts the firmware so the board re-emits its boot JSON - so the
// addresses and the argument order live here instead of in either of them.
// Everything in this namespace is pure, so it is covered by TestN6RamImage.
namespace N6RamImage {

constexpr quint32 kLoadAddress     = 0x34000400U;
constexpr quint32 kBootsrAddress   = 0x46008100U;  // SYSCFG_BOOTSR: bit0=BOOT0, bit1=BOOT1
constexpr quint32 kVtorAddress     = 0xE000ED08U;  // SCB->VTOR
constexpr quint32 kCpacrAddress    = 0xE000ED88U;  // SCB->CPACR
constexpr quint32 kCpacrFullAccess = 0x00F00000U;  // CP10 + CP11 full access

// AXISRAM window an image and its stack must sit inside.
constexpr quint32 kRamBegin = 0x34000000U;
constexpr quint32 kRamEnd   = 0x34400000U;

QString hex32(quint32 value);

// Whether a vector table's first two words could belong to an image in
// AXISRAM. Guards the restart path: after a power cycle RAM holds whatever it
// holds, and pointing the core at that would fault instead of reporting that
// the image is gone. The reset handler carries the Thumb bit.
bool vectorLooksValid(quint32 initialSp, quint32 resetHandler);

// First "0xADDRESS : VALUE" word of STM32_Programmer_CLI -r32 output.
bool parseCliWord32(const QString &output, quint32 &value);

// First two words of a vector-table read.
bool parseCliVector(const QString &output, quint32 &initialSp, quint32 &resetHandler);

// Connect arguments with any existing mode= replaced by `mode`.
QStringList connectArgsWithMode(const QStringList &connectArgs, const QString &mode);

// A real hardware reset that leaves the core running. Deliberately not `-rst`:
// that leaves the N6 in a state HOTPLUG can no longer attach to.
QStringList resetArgs(const QStringList &connectArgs);

// Reads SYSCFG_BOOTSR. mode=UR is the only mode that reaches the target whether
// the boot ROM is idle or a firmware is running; failing to read it at all is
// the signal that the board is in flash boot, where the ROM's secure boot has
// closed debug memory access.
QStringList bootPinsArgs(const QStringList &connectArgs);

// Reads the image's initial SP and reset handler out of AXISRAM.
QStringList vectorReadArgs(const QStringList &connectArgs);

// Points the core at the image, optionally writing binPath there first. VTOR
// selects our vector table; CPACR enables CP10/CP11, which the CubeN6 SDK's
// SystemInit leaves to the FSBL that a RAM image does not have.
QStringList armArgs(const QStringList &connectArgs,
                    quint32 initialSp,
                    quint32 resetHandler,
                    const QString &binPath = QString());

}  // namespace N6RamImage
