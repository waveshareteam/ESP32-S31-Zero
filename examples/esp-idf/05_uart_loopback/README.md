# 05_uart_loopback

Connect H2 pin 2 (GPIO58, UART1 TX) to H2 pin 1 (GPIO59, UART1 RX).
Disconnect other devices driving these pins before fitting the jumper.

The schematic labels these pins RXD0 and TXD0 for their native UART0 functions.
This example explicitly maps UART1 TX to GPIO58 and UART1 RX to GPIO59.

See [build instructions](../../../docs/examples.md), [framework status](../../../docs/versions.md) and [hardware evidence](../../../docs/hardware.md).

Compilation and physical-board operation have not been verified in this audit.
