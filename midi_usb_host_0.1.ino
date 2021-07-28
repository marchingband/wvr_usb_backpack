#include <usbh_midi.h>
#include <usbhub.h>
#include <MIDI.h>       // MIDI Library by Forty Seven Effects

// #define SerialDebug Serial1

#define Is_uhd_in_received0(p)                    ((USB->HOST.HostPipe[p].PINTFLAG.reg&USB_HOST_PINTFLAG_TRCPT0) == USB_HOST_PINTFLAG_TRCPT0)
#define Is_uhd_in_received1(p)                    ((USB->HOST.HostPipe[p].PINTFLAG.reg&USB_HOST_PINTFLAG_TRCPT1) == USB_HOST_PINTFLAG_TRCPT1)
#define uhd_ack_in_received0(p)                   USB->HOST.HostPipe[p].PINTFLAG.reg = USB_HOST_PINTFLAG_TRCPT0
#define uhd_ack_in_received1(p)                   USB->HOST.HostPipe[p].PINTFLAG.reg = USB_HOST_PINTFLAG_TRCPT1
#define uhd_byte_count0(p)                        usb_pipe_table[p].HostDescBank[0].PCKSIZE.bit.BYTE_COUNT
#define uhd_byte_count1(p)                        usb_pipe_table[p].HostDescBank[1].PCKSIZE.bit.BYTE_COUNT
#define Is_uhd_in_ready0(p)                       ((USB->HOST.HostPipe[p].PSTATUS.reg&USB_HOST_PSTATUS_BK0RDY) == USB_HOST_PSTATUS_BK0RDY)  
#define uhd_ack_in_ready0(p)                       USB->HOST.HostPipe[p].PSTATUSCLR.reg = USB_HOST_PSTATUSCLR_BK0RDY
#define Is_uhd_in_ready1(p)                       ((USB->HOST.HostPipe[p].PSTATUS.reg&USB_HOST_PSTATUS_BK1RDY) == USB_HOST_PSTATUS_BK1RDY)  
#define uhd_ack_in_ready1(p)                       USB->HOST.HostPipe[p].PSTATUSCLR.reg = USB_HOST_PSTATUSCLR_BK1RDY
#define uhd_current_bank(p)                       ((USB->HOST.HostPipe[p].PSTATUS.reg&USB_HOST_PSTATUS_CURBK) == USB_HOST_PSTATUS_CURBK)  
#define Is_uhd_toggle(p)                          ((USB->HOST.HostPipe[p].PSTATUS.reg&USB_HOST_PSTATUS_DTGL) == USB_HOST_PSTATUS_DTGL)  
#define Is_uhd_toggle_error0(p)                   usb_pipe_table[p].HostDescBank[0].STATUS_PIPE.bit.DTGLER
#define Is_uhd_toggle_error1(p)                   usb_pipe_table[p].HostDescBank[1].STATUS_PIPE.bit.DTGLER

#define MIDI_SERIAL_PORT Serial1

struct MySettings : public midi::DefaultSettings
{
//  static const bool Use1ByteParsing = false;
 static const bool Use1ByteParsing = true;
 static const unsigned SysExMaxSize = 1026; // Accept SysEx messages up to 1024 bytes long.
 static const long BaudRate = 31250;
};

MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, MIDI_SERIAL_PORT, MIDIUART, MySettings);


USBHost UsbH;
USBHub Hub(&UsbH);
USBH_MIDI  Midi(&UsbH);

bool doPipeConfig = false;
bool usbConnected = false;

//SAMD21 datasheet pg 836. ADDR location needs to be aligned. 
uint8_t bufBk0[64] __attribute__ ((aligned (4))); //Bank0
uint8_t bufBk1[64] __attribute__ ((aligned (4))); //Bank1

void setup()
{
  //bFirst = true;
  //vid = pid = 0;
  // SerialDebug.begin(115200);
//  Serial1.begin(31250);
  MIDIUART.begin(MIDI_CHANNEL_OMNI);
  if (UsbH.Init()) {
    // SerialDebug.println("USB host did not start");
    while (1); //halt
  }
  USB_SetHandler(&CUSTOM_UHD_Handler);
  delay( 200 );
  pinMode(2,INPUT);
}

void loop()
{
  //Note that Task() polls a hub if present, and we want to avoid polling.
  //So these conditions carry out enumeration only, and then stop running.
  //The idea is that except for enumeration (and release) this loop should 
  //be quiescent. 
  if (doPipeConfig || (!usbConnected && (UsbH.getUsbTaskState() != USB_DETACHED_SUBSTATE_WAIT_FOR_DEVICE))) {
    UsbH.Task();
  } else if (usbConnected && (UsbH.getUsbTaskState() != USB_STATE_RUNNING)){
    UsbH.Task();
  }
  
  if (usbConnected && (UsbH.getUsbTaskState() == USB_STATE_RUNNING) ) {
    if ( Midi && (Midi.GetAddress() != Hub.GetAddress()) && (Midi.GetAddress() != 0)) {
      if (doPipeConfig) {
        //There is a chance that a disconnect interrupt may happen in the middle of this
        //and result in instability. Various tests here on usbConnected to hopefully
        //reduce the chance of it.
        uint32_t epAddr = Midi.GetEpAddress();
        doPipeConfig = false;
        uint16_t rcvd;
        while (usbConnected && (USB->HOST.HostPipe[Midi.GetEpAddress()].PCFG.bit.PTYPE != 0x03)) {
          UsbH.Task(); 
          Midi.RecvData(&rcvd,  bufBk0);
        }
        USB->HOST.HostPipe[epAddr].BINTERVAL.reg = 0x01;//Zero here caused bus resets.
        usb_pipe_table[epAddr].HostDescBank[0].ADDR.reg = (uint32_t)bufBk0;
        usb_pipe_table[epAddr].HostDescBank[1].ADDR.reg = (uint32_t)bufBk1;
        USB->HOST.HostPipe[epAddr].PCFG.bit.PTOKEN = tokIN;
        USB->HOST.HostPipe[epAddr].PSTATUSCLR.reg = USB_HOST_PSTATUSCLR_BK0RDY; 
        uhd_unfreeze_pipe(epAddr); //launch the transfer
        USB->HOST.HostPipe[epAddr].PINTENSET.reg = 0x3; //Enable pipe interrupts

        // SerialDebug.println("Pipe Started");
        // SerialDebug.print("Dump:");
        // SerialDebug.print("ADDR0:");
        // SerialDebug.print(usb_pipe_table[epAddr].HostDescBank[0].ADDR.reg,HEX);
        // SerialDebug.print(":");
        // SerialDebug.print("ADDR1:");
        // SerialDebug.print(usb_pipe_table[epAddr].HostDescBank[1].ADDR.reg,HEX);
        // SerialDebug.print(":");
        // SerialDebug.print(USB->HOST.INTFLAG.reg,HEX);
        // SerialDebug.print(":");
        // SerialDebug.print(USB->HOST.HostPipe[epAddr].PINTFLAG.reg,HEX);
        // SerialDebug.print(":");
        // SerialDebug.println(USB->HOST.HostPipe[epAddr].PSTATUS.reg,HEX);
      }
    }
  } else {
    USB_SetHandler(&CUSTOM_UHD_Handler);
    USB->HOST.HostPipe[Midi.GetEpAddress()].PINTENCLR.reg = 0xFF; //Disable pipe interrupts
  }
}

void CUSTOM_UHD_Handler(void)
{
  uint32_t epAddr = Midi.GetEpAddress();
  if (USB->HOST.INTFLAG.reg == USB_HOST_INTFLAG_DCONN) {
    // SerialDebug.println("Connected");
    doPipeConfig = true;
    usbConnected = true;
  } else if (USB->HOST.INTFLAG.reg == USB_HOST_INTFLAG_DDISC) {
    // SerialDebug.println("Disconnected");
    usbConnected = false;
    USB->HOST.HostPipe[epAddr].PINTENCLR.reg = 0xFF; //Disable pipe interrupts
  }
  UHD_Handler();
  uhd_freeze_pipe(epAddr);
  /*
  SerialDebug.print(USB->HOST.INTFLAG.reg,HEX);
  SerialDebug.print(":");
  SerialDebug.print(USB->HOST.HostPipe[epAddr].PINTFLAG.reg,HEX);
  SerialDebug.print(":");
  SerialDebug.print(USB->HOST.HostPipe[epAddr].PSTATUS.reg,HEX);
  SerialDebug.print(":");
  SerialDebug.print("|STATUS0:");
  SerialDebug.print(usb_pipe_table[epAddr].HostDescBank[0].STATUS_PIPE.reg,HEX);
  SerialDebug.print("|STATUS1:");
  SerialDebug.print(usb_pipe_table[epAddr].HostDescBank[1].STATUS_PIPE.reg,HEX);
  SerialDebug.print("|STATUS_BK0:");
  SerialDebug.print(usb_pipe_table[epAddr].HostDescBank[0].STATUS_BK.reg,HEX);
  SerialDebug.print("|STATUS_BK1:");
  SerialDebug.print(usb_pipe_table[epAddr].HostDescBank[1].STATUS_BK.reg,HEX);
  SerialDebug.print("|BYTECOUNT0:");
  SerialDebug.print(uhd_byte_count0(epAddr),HEX);
  SerialDebug.print("|BYTECOUNT1:");
  SerialDebug.print(uhd_byte_count1(epAddr),HEX);
  SerialDebug.print("|TRCPT0:");
  SerialDebug.print(Is_uhd_in_received0(epAddr),HEX);
  SerialDebug.print("|TRCPT1:");
  SerialDebug.print(Is_uhd_in_received1(epAddr),HEX);
  SerialDebug.print("|READY0:");
  SerialDebug.print(Is_uhd_in_ready0(epAddr),HEX);
  SerialDebug.print("|READY1:");
  SerialDebug.print(Is_uhd_in_ready1(epAddr),HEX);
  SerialDebug.print("|CURRBK:");
  SerialDebug.print(uhd_current_bank(epAddr),HEX);
  SerialDebug.print("|TOGGLE:");
  SerialDebug.print(Is_uhd_toggle(epAddr),HEX);
  SerialDebug.print("|TOGGLE_ERROR0:");
  SerialDebug.print(Is_uhd_toggle_error0(epAddr),HEX);
  SerialDebug.print("|TOGGLE_ERROR1:");
  SerialDebug.print(Is_uhd_toggle_error1(epAddr),HEX);
  SerialDebug.print("|NAK:");
  SerialDebug.print(Is_uhd_nak_received(epAddr),HEX);
  SerialDebug.print("|INTSUMMARY:");
  SerialDebug.print(uhd_endpoint_interrupt(),HEX);
  SerialDebug.print("|");
  */

  //Both banks full and bank1 is oldest, so process first. 
  if (Is_uhd_in_received0(epAddr) && Is_uhd_in_received1(epAddr) && uhd_current_bank(epAddr)) {
    handleBank1(epAddr);
  }
  if(Is_uhd_in_received0(epAddr)) {
    handleBank0(epAddr);
  } 
  if (Is_uhd_in_received1(epAddr)) {
    handleBank1(epAddr);
  }
  uhd_unfreeze_pipe(epAddr);    
}

/*
void pipeConfig(uint32_t addr, uint32_t epAddr) {
    SerialDebug.println("pipeConfig called");
    //UHD_Pipe_Alloc Pipe Configuration Datasheet 31.8.6
    //Note usb_pipe_table and DESCADDR done by UHD_Init() already.
    USB->HOST.HostPipe[epAddr].PCFG.reg = 0; //clear
    USB->HOST.HostPipe[epAddr].PCFG.bit.PTYPE = 0x3; //pipe enabled as BULK and frozen
    USB->HOST.HostPipe[epAddr].PCFG.bit.BK = USB_HOST_NB_BK_1; //dual bank
    USB->HOST.HostPipe[epAddr].BINTERVAL.reg = 0x01;//Zero here caused bus resets.
    USB->HOST.HostPipe[epAddr].PCFG.bit.PTOKEN = USB_HOST_PCFG_PTOKEN_IN;
    USB->HOST.HostPipe[epAddr].PSTATUSSET.reg  = USB_HOST_PSTATUSSET_BK0RDY;
    //Not sure why this is here. It's in UHD_Init() as well. Maybe to clear it.
    memset((uint8_t *)&usb_pipe_table[epAddr], 0, sizeof(usb_pipe_table[epAddr]));
    usb_pipe_table[epAddr].HostDescBank[0].CTRL_PIPE.bit.PDADDR = addr;
    usb_pipe_table[epAddr].HostDescBank[0].CTRL_PIPE.bit.PEPNUM = epAddr;
    usb_pipe_table[epAddr].HostDescBank[0].PCKSIZE.bit.SIZE     = USB_PCKSIZE_SIZE_64_BYTES;
    //End Pipe Configuration
}
*/

void handleBank0(uint32_t epAddr){
  int rcvd = uhd_byte_count0(epAddr);
  for(int i=0;i<rcvd;i++)
  {
    MIDI_SERIAL_PORT.write(bufBk0[i]);
//    if(bufBk0[i] != 0)
//    {
//      MIDI_SERIAL_PORT.write(bufBk0[i++]);
//      MIDI_SERIAL_PORT.write(bufBk0[i++]);
//      MIDI_SERIAL_PORT.write(bufBk0[i++]);
//      MIDI_SERIAL_PORT.write(bufBk0[i]);
//      break;
//    }
  }
  // int i=0;
  // while(bufBk0[i] == 0)
  // {
  //   i++;
  // }
  // MIDI_SERIAL_PORT.write(bufBk0[i++]);
  // MIDI_SERIAL_PORT.write(bufBk0[i++]);
  // MIDI_SERIAL_PORT.write(bufBk0[i++]);
  // MIDI_SERIAL_PORT.write(bufBk0[i++]);
  uhd_ack_in_received0(epAddr);
  uhd_ack_in_ready0(epAddr);
}

void handleBank1(uint32_t epAddr){
  int rcvd = uhd_byte_count1(epAddr);
  for(int i=0;i<rcvd;i++)
  {
    MIDI_SERIAL_PORT.write(bufBk1[i]);
//    if(bufBk1[i] != 0)
//    {
//      MIDI_SERIAL_PORT.write(bufBk1[i++]);
//      MIDI_SERIAL_PORT.write(bufBk1[i++]);
//      MIDI_SERIAL_PORT.write(bufBk1[i++]);
//      MIDI_SERIAL_PORT.write(bufBk1[i]);
//      break;
//    }
  }
  // int i=0;
  // while(bufBk1[i] == 0)
  // {
  //   i++;
  // }
  // MIDI_SERIAL_PORT.write(bufBk1[i++]);
  // MIDI_SERIAL_PORT.write(bufBk1[i++]);
  // MIDI_SERIAL_PORT.write(bufBk1[i++]);
  // MIDI_SERIAL_PORT.write(bufBk1[i++]);
  uhd_ack_in_received1(epAddr);
  uhd_ack_in_ready1(epAddr);
}
