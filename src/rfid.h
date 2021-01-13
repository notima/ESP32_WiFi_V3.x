#include <DFRobot_PN532.h>

#define  SCAN_FREQ            1000
#define  RFID_BLOCK_SIZE      16
#define  PN532_IRQ            (2)
#define  PN532_INTERRUPT      (1)
#define  PN532_POLLING        (0)

void rfid_setup();
void rfid_loop();