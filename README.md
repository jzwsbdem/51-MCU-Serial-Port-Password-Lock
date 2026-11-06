# STC89C52 Smart Lock Firmware

## Feature Specification

1. **Input operations:** Supports digit input, delete, clear, confirm, lock, cancel-modify, and change-password. Keyboard and serial port share the same input pipeline and can be used interchangeably. Fewer than 6 digits entered does not count as an error; more than 6 digits cannot overwrite the existing entry. A correct password unlocks; a wrong password clears the input and increments the error counter.

2. **Password change:** Only permitted after unlocking. The new password must be entered twice consecutively and match to be saved; leading zeros are preserved. Canceling, locking, or a power loss mid-change does not commit a partial password. If a save-success prompt has already been shown before power is cut, the new password remains valid.

3. **Idle auto-lock:** During unlock or password-change, 15 seconds of inactivity automatically re-locks the device. Status / version queries and invalid operations do not reset the idle timer. Holding a key fires only one event; prompts respond normally while the timer is counting.

4. **Error lockout:** The 3rd consecutive error triggers a **30-second pause** (error count retained on expiry). The 5th consecutive error triggers a **60-second lockout** *(unified receipt value)*; the error count clears on expiry. A correct unlock also clears the error count. During lockout only status queries are allowed — the lock cannot be released early. **Error count and lockout state must survive a power cycle.** Restarting during lockout requires waiting out the full remaining duration.

5. **7-segment display (left to right):** Digit 1 = status code, Digit 2 = error count, Digits 3–8 = current input in real time (unentered positions show `–`). Status codes 0–6 correspond to: Locked / Unlocked / First-new-password / Re-enter-new-password / Paused / Protected / ISP-ready. While unlocked, an auto-lock countdown is shown in the form `----15`. During lockout the remaining seconds are shown zero-padded to 6 digits. Every key press or complete serial command triggers an audible beep starting within **100 ms**; long blocking delays must not be used for this purpose.

6. **ISP upgrade:** Only a normal (password-correct) unlock enables ISP entry. After a non-power-off software reset, the chip enters its ISP bootloader; the PC tool completes the download and verification. The new firmware locks the device on first boot and retains the previously saved password, ready for the next upgrade cycle.

---

A cooperative-multitasking firmware for the **STC89C52** microcontroller implementing a password-based electronic lock.

| Folder | Purpose |
|--------|---------|
| `Tools/` | Hardware-abstraction drivers |
| `Core/`  | Application logic — lock state machine, input, display, UART protocol, EEPROM persistence |

> **Build:** STC89C52RC · 11.0592 MHz · Keil C51

---

## Tools/Timer — Cooperative Task Scheduler

Backed by Timer 2 (1 ms tick). Supports up to `MAX_TASKS` (15) concurrent tasks.

| Function | Description |
|----------|-------------|
| `Timer0Init()` | Initialize Timer 2 and the task table. Call once at startup. |
| `Timer0Routine()` | *(ISR, interrupt 5)* Increments tick counter; drives LED refresh and feedback blink. |
| `TaskProcess()` | Dispatch all due tasks. **Call in the main loop.** |
| `TaskAdd(period, fn)` | Register `fn` to run every `period` ms. Returns `1` on success. |
| `TaskDelete(fn)` | Remove `fn` from the scheduler. |
| `TaskSetPeriod(period, fn)` | Change the period of a registered task. |
| `TaskGetPeriod(fn)` | Return the current period of a task (ms). |
| `TaskExist(fn)` | Return `1` if `fn` is registered. |
| `TaskCount()` | Return the number of active tasks. |
| `TaskGetTick()` | Return current tick count (ms), read atomically. |
| `TaskGetNextTick(fn)` | Return the next scheduled tick for `fn`. |
| `Delay(ms)` | Blocking software delay. Avoid in the main loop. |

---

## Tools/UART — Interrupt-Driven Serial Driver

Ring-buffer TX (64 bytes), 9600 baud. Received bytes are forwarded to `LockUART_ReceiveISR()`.

| Function | Description |
|----------|-------------|
| `UARTInit()` | Configure baud rate (Timer 1) and enable the serial interrupt. Call once at startup. |
| `UARTRoutine()` | *(ISR, interrupt 4)* Handles RX forwarding and TX ring-buffer draining. |
| `UARTByteSender(byte)` | Enqueue one byte for TX. Returns `1` on success, `0` if buffer full. |
| `UARTStringSender(str)` | Enqueue a null-terminated string atomically. Returns `1` on success. |
| `UARTTxFree()` | Return the number of free bytes in the TX buffer. |
| `UARTTransmitIdle()` | Return `1` if the TX hardware is idle. |

---

## Tools/I2C — Bit-Banged I²C Master

Operates on **P2.0 (SDA)** and **P2.1 (SCL)**. Used internally by E2PROM.

| Function | Description |
|----------|-------------|
| `I2CStart()` | Generate START condition. |
| `I2CStop()` | Generate STOP condition. |
| `I2CByteSender(byte)` | Clock out 8 bits MSB-first. |
| `I2CByteReceiver()` | Clock in 8 bits MSB-first; return received byte. |
| `I2CSendACK(bit)` | Send ACK (`0`) or NACK (`1`). |
| `I2CReceiveACK()` | Read slave ACK bit. Returns `0` = ACK, `1` = NACK. |

---

## Tools/E2PROM — AT24C02 EEPROM Driver

Non-blocking write driver (256 bytes, I²C address `0xA0`). Requires `E2PROMWriteTask` to be registered in the scheduler.

| Function | Description |
|----------|-------------|
| `E2PROMIsReady()` | Poll the chip; return `1` if the internal write cycle is done. |
| `E2PROMIsBusy()` | Return `1` if a write/erase is in progress. |
| `E2PROMAsyncByteWriter(addr, data)` | Start a non-blocking byte write. Returns `1` if accepted. |
| `E2PROMByteReader(addr)` | Blocking byte read. Returns `0xFF` on error. |
| `E2PROMReadOK()` | Return `1` if the last read succeeded. |
| `E2PROMWriteTask()` | Task function — advances the write-cycle state machine. Register with `TaskAdd`. |
| `ClearE2PROM()` | Schedule an async full-chip erase (all bytes → `0xFF`). |

---

## Tools/LEDDisplay — 8-Digit 7-Segment Display

Multiplexed via a 3-bit latch (P2.2–P2.4) and segment data on P0. Refreshed by the Timer ISR.

| Function | Description |
|----------|-------------|
| `SetBuf(pos, num)` | Write digit `num` (0–10) to display buffer position `pos` (1–8). |
| `LEDDisplay(pos, num)` | Directly drive one digit. Called internally by `ShowLoop`. |
| `ShowLoop()` | Advance the multiplex cycle by one digit. Called from the Timer ISR. |

---

## Tools/MatrixKey — 4×4 Matrix Keypad

Keys numbered 0–15; `16` = no key / released. Scan registered as a 20 ms task.

| Function | Description |
|----------|-------------|
| `MatrixKeyRegister()` | Register the scan task with the scheduler. Call once at startup. |
| `MatrixKeyGet()` | Return the latest key (0–15) and clear the event; returns `16` if none. |
| `MatrixKeyScan()` | *(task function)* Scan the matrix and update `globalMatrixKey`. Registered automatically. |
