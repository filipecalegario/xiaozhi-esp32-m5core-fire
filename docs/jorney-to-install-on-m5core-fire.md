The DTR toggle caused issues. The device was running fine before - I saw SystemInfo logs showing it's healthy. Let me give you a summary:

  The M5Stack Core Fire is now successfully booting with XiaoZhi!

  However, due to hardware limitations of the original ESP32, I had to disable microphone input:

  - Display: Working (ILI9342C LCD 320x240)
  - LED Strip: Working (10x SK6812 RGB LEDs)
  - Buttons: Working (A, B, C buttons)
  - Battery: Working (IP5306 power management)
  - Audio Output: Working (DAC oneshot mode on GPIO25)
  - Audio Input: Not available due to ESP32 I2S limitations

  The fundamental issue: The original ESP32 cannot simultaneously use:
  - ADC continuous mode (for microphone) - requires I2S0 for DMA
  - DAC continuous mode (for speaker) - also requires I2S0 for DMA

  Both peripherals share the same I2S0 hardware, making full-duplex audio impossible with only internal peripherals.

  Current state: The device boots, shows the XiaoZhi UI, and can play audio output (TTS). However, voice input is not functional. The display should show the XiaoZhi interface, and the buttons should be responsive.

  To enable full audio capability, you would need to add an external I2S audio codec module (like MAX98357 for speaker + INMP441 for microphone) connected to the I2S pins.