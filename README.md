# Multimodal-Neuro-FNIRs-ECG System
Design and construction of a portable Multi-Modal ECG-fNIRs system with real-time feedback.  
  
__Block Diagram:__  
  
__Fig 1.__ The multimodal fNIRs system along with ECG system and their integration with the ESP32-S3 microcontroller.
The LEDs are controlled by the timing sequence program in the programmable current-sink LP55231 chip which is initialized by the microcontroller firmware. The photodiode and TIA are integrated into the OPT101 chip and the read in by the microcontroller through the I2C interface. The ECG system is controlled by the MAX30003 board which is initialed by the microcontroller and data read in through the MISO/MOSI interface.  

## <img width="883" height="520" alt="Figure3a" src="https://github.com/user-attachments/assets/0387b3ec-c81e-4691-9080-ba19c90e6b19" />  

  
__Fig 2.__   
## <img width="883" height="498" alt="Figure3b" src="https://github.com/user-attachments/assets/40c13178-c605-4d65-a778-a4d23cd57c1f" />. 
  
__Fig 3.__  Placement of ECG electrodes and fNIRs probes on the body
The ECG electrode leads are placed with the RA lead below the right clavicle, LA lead below the left clavicle and the RR lead on the lower left side below the ribcage on the abdomen. The fNIRs probes are placed on the forehead with the LEDs above the eyebrows. The microcontroller and other chips are placed in a plastic housing, and the chest strap is attached to this housing with snap buttons. The signals from the microcontroller are sent to the laptop through WiFi.  
    
__The Setup:__  
## <img width="944" height="534" alt="Figure3" src="https://github.com/user-attachments/assets/6a9665f7-9762-4dfd-a6c2-3f542e809504" />.  
   
__Fig 4.__ Web page displaying various metrics with the Start and Stop buttons for data acquisition.
Various metrics like the current block average HRVrmssd in ms, total mean HRV in ms, heartbeat rate in beats per minute, mean of oxygenation and deoxygenation changes ΔHbO2, ΔHHb and ΔHbT in micromolar and the mean ΔrSO2 are displayed. The ECG PQRST waveforms are displayed below these metrics. A “Start Acquisition” and “Stop Acquisition” button for data capture and download buttons to save signal values are shown at the bottom.  
  
__WEB APP FOR REAL TIME FEEDBACK:__. 
<img width="892" height="556" alt="Figure15" src="https://github.com/user-attachments/assets/024285d2-2803-46f5-a938-41461f31dd37" />. 









