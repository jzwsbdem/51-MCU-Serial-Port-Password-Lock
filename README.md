# SecondProject — STC89C52 Smart Lock Firmware

A cooperative-multitasking firmware for the **STC89C52** microcontroller implementing a password-based electronic lock.  
The project is split into two layers:

| Folder | Purpose |
|--------|---------|
| `Tools/` | Reusable hardware-abstraction drivers (this document) |
| `Core/`  | Application logic — lock state machine, input handling, display, UART protocol, EEPROM persistence |

---

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Module: Timer](#module-timer-timerc--timerh)
- [Module: UART](#module-uart-uartc--uarth)
- [Module: I2C](#module-i2c-i2cc--i2ch)
- [Module: E2PROM](#module-e2prom-e2promc--e2promh)
- [Module: LEDDisplay](#module-leddisplay-leddisplayc--leddisplayh)
- [Module: MatrixKey](#module-matrixkey-matrixkeyc--matrixkeyh)
- [Dependencies](#dependencies)

---

## Architecture Overview

```
main()
  ├── Timer0Init()          — start cooperative scheduler (Timer2 ISR)
  ├── UARTInit()            — start serial port (Timer1 baud, UART ISR)
  ├── MatrixKeyRegister()   — register key-scan task
  └── while(1)
        ├── TaskProcess()   — dispatch all due tasks
        ├── InputMatrixKey()
        └── LockUARTProcess()
```

The scheduler drives every periodic action via `TaskAdd` / `TaskProcess`.  
No `Delay()` calls are used in the main loop; blocking delays are only used during initialisation.

---

## Module: Timer (`Timer.c` / `Timer.h`)

Cooperative task scheduler backed by **Timer 2** (1 ms tick, accuracy-corrected).  
Up to `MAX_TASKS` (15) tasks can be registered simultaneously.

### Public API

#### `void Timer0Init()`
Initialises Timer 2 and the internal task table.  
**Must be called once before any other Timer / Task function.**  
Configures the reload values to produce a nominal 1 ms interrupt.

---

#### `void Timer0Routine()` *(ISR, interrupt 5)*
Timer 2 interrupt service routine.  
- Increments the global tick counter.
- Drives the LED multiplex loop (`ShowLoop`) and feedback blink (`TaskFeedback`) at half rate (every 2 ticks).
- **Not called directly** — invoked automatically by hardware.

---

#### `unsigned long TaskGetTick()`
Returns the current tick count (ms since `Timer0Init()`).  
Disables interrupts atomically while reading the 32-bit value.

```c
unsigned long now = TaskGetTick();
```

---

#### `unsigned long TaskGetNextTick(TaskFunction function)`
Returns the tick value at which `function` is scheduled to run next.  
Returns `0` if the function is not registered.

---

#### `void TaskProcess()`
**Call in the main loop.** Iterates every registered task and calls those whose next-tick deadline has passed.  
Re-entrant guard prevents nested execution.

```c
while (1) {
    TaskProcess();
    /* other work */
}
```

---

#### `unsigned char TaskAdd(unsigned int period, TaskFunction function)`
Registers `function` to be called every `period` milliseconds.  
- Returns `1` on success, `0` on failure (table full or bad arguments).  
- If `function` is already registered, returns `1` without duplicating it.  
- Increments `taskAddFailures` on table-full conditions.

```c
TaskAdd(20, MatrixKeyScan);   // scan keys every 20 ms
TaskAdd(1,  TaskSavePending); // check pending save every ms
```

---

#### `void TaskDelete(TaskFunction function)`
Removes `function` from the scheduler. Safe to call with a NULL or unregistered function.

---

#### `void TaskSetPeriod(unsigned int period, TaskFunction function)`
Changes the execution period of an already-registered task and resets its next deadline.

---

#### `unsigned int TaskGetPeriod(TaskFunction function)`
Returns the current period (ms) of a registered task, or `0` if not found.

---

#### `unsigned char TaskExist(TaskFunction function)`
Returns `1` if `function` is currently registered, `0` otherwise.

---

#### `unsigned char TaskCount()`
Returns the number of active tasks in the scheduler.

---

#### `void Delay(unsigned int ms)`
Blocking software delay in milliseconds.  
Uses `_nop_()` loops calibrated for 11.0592 MHz.  
**Avoid in the main loop** — use the task scheduler instead.

---

#### Global: `unsigned char taskAddFailures`
Counts the number of times `TaskAdd` failed due to a full task table. Useful for debugging.

---

## Module: UART (`UART.c` / `UART.h`)

Interrupt-driven, ring-buffer UART driver.  
TX buffer size: `UART_TX_SIZE` (64 bytes). Baud rate: **9600** (11.0592 MHz, SMOD=1, Timer 1 auto-reload).  
Received bytes are forwarded directly to `LockUART_ReceiveISR()` in the Core layer.

### Public API

#### `void UARTInit()`
Configures Timer 1 as the baud-rate generator, sets SCON to mode 1 (8-bit variable baud), and enables the serial interrupt.  
**Call once before transmitting or receiving.**

---

#### `void UARTRoutine()` *(ISR, interrupt 4)*
Serial interrupt handler.  
- **RX**: clears `RI`, passes `SBUF` to `LockUART_ReceiveISR()`.  
- **TX**: clears `TI`, sends the next byte from the ring buffer or clears the busy flag.  
- **Not called directly.**

---

#### `unsigned char UARTTxFree()`
Returns the number of free bytes remaining in the TX ring buffer.  
Use to check whether a string will fit before sending.

```c
if (UARTTxFree() >= len) { /* safe to send */ }
```

---

#### `unsigned char UARTTransmitIdle()`
Returns `1` if the TX hardware is idle (buffer empty and no byte being shifted out), `0` otherwise.  
Useful for detecting end-of-transmission before entering low-power mode.

---

#### `unsigned char UARTByteSender(unsigned char byte)`
Enqueues a single byte for transmission.  
Returns `1` on success, `0` if the buffer is full.

```c
UARTByteSender('A');
```

---

#### `unsigned char UARTStringSender(char *string)`
Enqueues a null-terminated string for transmission atomically (all-or-nothing).  
Returns `1` on success, `0` if the string is too long or the buffer has insufficient space.

```c
UARTStringSender("OK\r\n");
```

---

## Module: I2C (`I2C.c` / `I2C.h`)

Bit-banged I²C master on **P2.0 (SDA)** and **P2.1 (SCL)**.  
Used exclusively by the E2PROM module; not intended for direct use from application code.

### Public API

#### `void I2CStart()`
Generates an I²C START condition (SDA falls while SCL is high) and clamps the bus.

#### `void I2CStop()`
Generates an I²C STOP condition (SDA rises while SCL is high) and releases the bus.

#### `void I2CByteSender(unsigned char byte)`
Clocks out 8 bits MSB-first on SDA.

#### `unsigned char I2CByteReceiver()`
Clocks in 8 bits MSB-first from SDA. Releases SDA before reading.  
Returns the received byte.

#### `void I2CSendACK(unsigned char ACKBit)`
Sends an ACK (`0`) or NACK (`1`) after receiving a byte.

#### `unsigned char I2CReceiveACK()`
Reads the ACK/NACK bit from the slave.  
Returns `0` = ACK (slave acknowledged), `1` = NACK.

---

## Module: E2PROM (`E2PROM.c` / `E2PROM.h`)

Non-blocking driver for an **AT24C02** EEPROM (256 bytes, I²C address `0xA0`).  
Writes are asynchronous: `E2PROMAsyncByteWriter` initiates the write cycle, and `E2PROMWriteTask` (registered with the scheduler) polls for completion.

### Public API

#### `bit E2PROMIsReady()`
Polls the EEPROM via I²C to check whether its internal write cycle has finished.  
Returns `1` if the device acknowledges (ready), `0` if still busy.

---

#### `bit E2PROMIsBusy()`
Returns `1` if a write or erase operation is in progress (i.e., the driver's state machine is active), `0` if idle.  
Use this to avoid issuing a new write before the previous one completes.

```c
if (!E2PROMIsBusy()) {
    E2PROMAsyncByteWriter(addr, data);
}
```

---

#### `bit E2PROMAsyncByteWriter(unsigned char address, unsigned char _data)`
Initiates a non-blocking byte write to `address`.  
Returns `1` if the write was started successfully, `0` if the driver is busy or the hardware did not acknowledge.  
After calling this, `E2PROMWriteTask` must be running in the scheduler to finalise the cycle.

---

#### `unsigned char E2PROMByteReader(unsigned char address)`
Blocking byte read from `address`.  
Returns the byte value, or `0xFF` on communication error.  
Check `E2PROMReadOK()` after the call to confirm success.

```c
unsigned char val = E2PROMByteReader(0x00);
if (E2PROMReadOK()) { /* val is valid */ }
```

---

#### `bit E2PROMReadOK()`
Returns `1` if the last `E2PROMByteReader` call completed without error, `0` otherwise.

---

#### `void E2PROMWriteTask()`
Task function to be registered with the scheduler (called periodically).  
Advances the internal write-cycle state machine: waits for the EEPROM to become ready after a write, then continues any pending erase operation.  
Register with: `TaskAdd(1, E2PROMWriteTask)`.

---

#### `void ClearE2PROM()`
Schedules an asynchronous full-chip erase (fills all 256 bytes with `0xFF`).  
The erase runs byte-by-byte through `E2PROMWriteTask`; it is non-blocking.  
Safe to call while other writes are not pending.

---

## Module: LEDDisplay (`LEDDisplay.c` / `LEDDisplay.h`)

8-digit 7-segment LED display driver using a 3-bit address latch (P2.2–P2.4) and segment data on P0.  
Display is refreshed by the Timer ISR at ~500 Hz (every 2 ms per digit cycling through 8 digits).

### Public API

#### `void LEDDisplay(unsigned char pos, unsigned char num)`
Directly drives segment `pos` (1–8) with digit `num` (0–9, or `10` for a dash `–`).  
Called internally by `ShowLoop`; can also be called directly for one-shot display.

---

#### `void ShowLoop()`
Advances the multiplexed display by one digit per call.  
**Called from the Timer ISR** every 2 ms — do not call from application code.

---

#### `void SetBuf(unsigned char location, unsigned char number)`
Writes `number` (0–10) into display buffer position `location` (1–8).  
The change will appear on the next `ShowLoop` cycle.

```c
SetBuf(1, 5);  // show digit '5' on the first display segment
SetBuf(5, 10); // show '–' (blank/dash) on segment 5
```

---

#### Global: `volatile unsigned char buf[9]`
Raw display buffer (index 1–8 correspond to display positions 1–8).  
Prefer `SetBuf()` over direct access.

#### Global: `unsigned char isRegisterLEDDisplay`
Flag indicating whether the LED display task has been registered. Currently unused by the driver itself; available for external checks.

---

## Module: MatrixKey (`MatrixKey.c` / `MatrixKey.h`)

4×4 matrix keypad driver. Keys are numbered 0–15; value `16` means "no key pressed / key released".  
Scanning is registered as a periodic task (every 20 ms) to provide software debouncing.

### Public API

#### `void MatrixKeyRegister()`
Registers `MatrixKeyScan` with the task scheduler (20 ms period) if not already registered.  
Sets `isMatrixKeyRegistered` to `1` on success.  
**Call once during initialisation.**

```c
MatrixKeyRegister();
```

---

#### `unsigned char MatrixKeyGet()`
Returns the last detected key number (0–15) and clears the pending event.  
Returns `16` if no new key has been pressed since the last call.  
Use this in the main loop to consume key events one at a time.

```c
unsigned char key = MatrixKeyGet();
if (key != 16) { /* handle key */ }
```

---

#### `void MatrixKeyScan()` *(task function)*
Scans the 4×4 matrix and updates `globalMatrixKey` on a new key-down edge.  
Registered automatically by `MatrixKeyRegister()`; **do not call directly**.

---

#### Global: `volatile unsigned char globalMatrixKey`
Holds the most recent key number. Reset to `16` by `MatrixKeyGet()` after consumption.

#### Global: `unsigned char isMatrixKeyRegistered`
`1` if the scan task was registered successfully, `0` otherwise.

---

## Dependencies

```
Tools/Timer    ←─ Tools/LEDDisplay
                ←─ Tools/MatrixKey
                ←─ Tools/UART (Timer1 for baud only)
                ←─ Core/LockFeedback  (TaskFeedback called in ISR)

Tools/I2C      ←─ Tools/E2PROM

Tools/UART     ←─ Core/LockUART  (LockUART_ReceiveISR called in ISR)
```

> **Build target:** STC89C52RC, 11.0592 MHz crystal, Keil C51 toolchain.
