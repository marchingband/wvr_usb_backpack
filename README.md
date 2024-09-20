(This code was based on a [fork](https://github.com/studiohsoftware/USB_Host_Library_SAMD/blob/master/examples/USBH_MIDI/USB_MIDI_Using_Interrupts/USB_MIDI_Using_Interrupts.ino) by [studio h software](https://github.com/studiohsoftware), of [gdsports/USB_Host_Library_SAMD](https://github.com/gdsports/USB_Host_Library_SAMD). I also drew heavily on this [article](http://www.plzi.com/aanta/usbmidihost.asp). You can read it if you are interested in setting up Arduino to hack on this code.)


# wvr_usb_backpack
Firmware for the WVR Midi USB Host Backpack

# how to update the firmware
* Make sure the WVR is not powered in any way! Then ...
* Connect the USB Backpack to a computer using the USB-C jack on the backpack (not the one on the WVR). The WVR should turn on, and the LED on the WVR should light.
* open a Finder window on your computer
* Using a jumper wire or a pair of tweezers, you must double tap the small copper pads beside the USB port. This can be very challegning. You can find videos online by googling "Seeed XIAO bootloader". This will put the USB Backpack into bootloader mode, and it should show up in your Finder Window as a drive called "Trinket M0". If it doesn't show up, keep trying the double click, the double click must be quite fast. Sometimes a long first press with a very fast second press works best.
* Download this file https://github.com/marchingband/wvr_usb_backpack/blob/main/CURRENT.UF2
* copy it onto the Seeed XIAO drive. Replace the old version, if you are prompted with the option, it should overwrite the old version.
* the drive should disapear at this point
* you are done.
