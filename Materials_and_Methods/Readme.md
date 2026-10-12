This section contains the materials, methods and cost involved in building the system.  
The system is tested against different Cognitive Behavioral Therapy tasks commonly associated with treating Anxiety and Stress in adolescents.  

# __Materials__. 
Table 3. fNIRs Probe System Materials, Quantity and Cost

<img width="468" height="579" alt="image" src="https://github.com/user-attachments/assets/821a75c0-b2ae-4bf5-8f2e-1efb842072eb" />. 
  
<img width="468" height="134" alt="image" src="https://github.com/user-attachments/assets/d7257b9e-a629-4722-bd9c-4832896f5f3b" />. 
  

## Mobile device and Software libraries
- Laptop with 8GB memory, 512GB hard-disk, Chrome web-browser, MacOSx operating system
- Software libraries include Arduino IDE, IC LP55231 driver, MAX30003 library (40) , WiFi Async TCP server library, CBT tasks from (29), EasyEDA PCB web-based design software.
  
## Tools
- Soldering iron with temperature control, tin/lead solder, wire clipper, tweezer, Exacto knife, measuring tape, ruler, multimeter, safety gloves, pencil, scissors, super-glue, forehead temple thermometer
- Total cost excluding tools, software and mobile device is $220.


    
# __Design Criteria__. 
## Optode Sensors and Forehead Strap (fNIRs System):  

- Each optode probe consists of 2 near-infrared light-emitting diodes (LEDs), L1 and L2 of wavelengths 740 nm and 850 nm (nanometers) with two photodiodes PD1 and PD2.

  
__Fig 5__. _fNIRs probes design.
The probes are housed in a silicone sheet with straps extending to the back of the head. The probes are designed with 1mm thickness PCB. It consists of two LEDs S1 and S2 for the two wavelengths 740nm and 850nm. Two photodiodes, D1 and D2, with an inbuilt trans impedance amplifier is placed 10mm for the short channel and 33 mm for the long channel detection. With this configuration 4 channels per probe S1-D1, S2-D1, S1-D2 and S2-D2 are formed with a total of 8 channels._.   

<img width="723" height="374" alt="image" src="https://github.com/user-attachments/assets/2b67a342-3839-4ab7-9218-c405081f5cf4" />.
  
- With the probe is placed on the forehead, for different light intensities (0 – 20 mA current (milliamps)) of LEDs L1 and L2, photodiodes PD1 and PD2 should produce a final output voltage in the range 0 to 3.3 Volts. This voltage is measured after amplification from the trans-impedance amplifier (TIA).
- The temperature measured on the forehead at the contact point of LEDs should be less than 40<sup>o</sup> Celsius, after 1000 seconds (IEC 80601-2-71 safety standards for FNIRS devices).
- The photodiodes in the optode module should not saturate during both short-channel (10 mm separation) and long-channel (33 mm separation) acquisition.
- The optical probes should be adjustable in the strap for 4 inches.
- The strap position on the forehead should remain the same for 15 min.

## ECG Electrode Sensors, Biosensing Module and Chest Strap (ECG System). 

- The placement of electrodes on the anterior part of the chest should produce a valid ECG signal waveform (range 0 – 3.3 Volts) from the heart-rate sensor board, with each heartbeat (Figure 6).
- The placement of electrodes on the anterior part of the chest should produce a valid ECG signal waveform (range 0 – 3.3 Volts) from the heart-rate sensor board, with each heartbeat (Figure 6).
- The biosensing module must initiate the controlling sequence of optode modules during the ECG R-R interval and convert the signals to produce a waveform as shown in Figure 7, convert the captured analog signals into a digital signal in the range of 12 bit analog-to-digital converter (ADC) and convert the digital signals into analog voltage to control the intensities of the LEDs.

  
__Fig 6__. _ECG waveform with Regional Cerebral Blood Oxygenation and Deoxygenation waveforms.
HRVrmssd is calculated from the R-R peak interval changes with each heartbeat. The R peak is evaluated from detecting the PQRST part of the waveform. The task evoked changes in the regional cerebral blood oxygenation and deoxygenation are seen in the ‘A’ plot over many cycles. The HbO2 rises as the task proceeds while HHb decreases for nearly 60 seconds. The enlarged plot ‘B’ shows one cycle of the hemodynamic response with a few peaks of HbO2. Plot ‘C’ highlights the concentration changes as a heat map showing higher changes near the center of the forehead._    
  
<img width="2146" height="1050" alt="image" src="https://github.com/user-attachments/assets/2727a378-f437-4562-a047-c83214aae3ff" />. 



__Fig 7__. _Optode LED firing and Photodiode detecting timing sequence over the 8-channels.
 In Period 1 all four LEDs are fired, and both short channel detectors are used. During Period 2, the long channel detectors are at full gain, and LEDs are not fired and during Period 3, all four LEDs are again fired with long channel detectors being used. The sampling frequency is around 50Hz._  
  
<img width="803" height="282" alt="image" src="https://github.com/user-attachments/assets/fe059705-6197-422f-bce7-8766a7837b9a" />.  

## Microcontroller (µC):  

- Data Acquisition (DAS) - The microcontroller should execute the firmware on receiving a signal from the laptop to initiate the capture of the ECG signal, then start activating the optode and receive the photodiode signals between subsequent R-R intervals of the ECG waveform (Figure 6).
- The microcontroller firmware must execute the state machine flow as indicated in Figure 8.

__Fig 8__.  _The control flow for acquiring fNIRs and ECG signals, processing and transmission sequence.
The sequence starts with capturing the R-Peak of the ECG waveform to synchronize with the fNIRs signals. The optode sequence is then initiated and fNIRs signals are captured. Both R-Peak intervals and fNIRs signals are computed for 60 seconds, processed and CRS score calculated and transmitted to the laptop. The cycle is reset and again repeated by capturing the R-Peak to initiate optode sequence_.  
 
<img width="333" height="613" alt="image" src="https://github.com/user-attachments/assets/641b551a-beba-4241-b340-7c6b7d00a2d6" />. 

## Power Management:  

- The lithium-ion (LiPo) battery should last at least for 1 hour, i.e. stable readings noted for 1 hour before recharging.

## Mobile device Software:  
- The software should be able to run on Mac OSx platform and run the CBT tasks on a web browser.
- Take user inputs to start and stop the cognitive task, send the optode signal acquisition sequence, start and stop the recording of ECG and cerebral hemodynamic response signals.
- Display metrics like HRV (heart rate variability) and ΔrSO2 (regional cerebral oxygen saturation change).

## Microcontroller Firmware:  
- The firmware must be able to get signals from the software and initiate the signal acquisition sequence.
- The firmware must be able to acquire signals from both the optodes and ECG board in the designed sequence and prefilter signals from ambient light, motion-artifacts and heartbeat signal noise.

## Cognitive Tasks:  
- The set of identified cognitive tasks must have a positive CRS that shows a positive improvement in HRVrmssd and ΔrSO2 (Table-2).
  
<img width="468" height="270" alt="image" src="https://github.com/user-attachments/assets/ac238fb6-eed8-43c9-830e-d0b31c83271e" />. 


# __Methods__. 

## System Design, Construction and Testing Methods. 
  
_fNIRs Initial Design and Construction_.  
- Create the probe housing template on a paper as shown in Figure 9. Wear safety gloves and cut out the shape of the template from the silicone sheet along with the holes for the IC OPT101 (39) adapter and near-infrared LEDs.
- Design the PCB in EasyCAD (Figure 11) as per schematic in Figure 10 and have it manufactured. 
- Solder the 6-pin connectors, LEDs and the OPT101 IC adapters to the PCB boards designed and produced as per Figure 11.
- Insert the 6-pin connectors with cable in the two PCBs.
- Insert the OPT101 ICs and LEDs in the PCBs into the silicone sheet cutout holes for the probes as shown in Figure 12.
- Make the connections to the LED driver board LP55231 (38) using 26-gauge wires as shown in Figure 13. Make the connections from the LP55231 board to the microcontroller using 26-gauge wires as shown in Figure 13.
- Connect the photodiode outputs from the fNIRs probe to the microcontroller GPIO (general purpose I/O pins) pins as shown in Figure 13 with 26-guage wires.  

__Fig 9__.  _Probe housing template used to create the probe enclosure from Silicone sheet.
The left and the right probes are symmetrical. The holes for the four LEDs are round and 5mm in diameter. The holes for the photodiode amplifier adapter are rectangular 12mm x 8mm. The flat cable wires run along the probe enclosure straps that are 75mm long on each side._  
  
<img width="1124" height="325" alt="image" src="https://github.com/user-attachments/assets/d3db09ab-92e8-4d45-a5c4-f8a486fb2b48" />. 

__Fig 10__. _Schematic for the fNIRs optode probe connections.
The schematic is the same for the right and left probes. Three 2-pin connectors, H1, H2 and H3 form the connection header at the edge of the probe where the cable is attached. The LEDs are connected in a common-cathode fashion. Capacitor C1 is added to reduce power line noise. OPT101 contains both photodiode and the trans impedance amplifier._. 
  

<img width="398" height="142" alt="image" src="https://github.com/user-attachments/assets/a8f824df-5b0c-4a00-b4d5-f91be2cb43a0" />. 

__Fig 11__.  _PCB for the optode probes designed with EasyEDA software.
The PCB is 1mm in thickness and is double sided. The right and the left probe PCBs are shown with the top layer connections to the LEDs, OPT101 photodiode amplifier 8-pin adapters and headers._  
  
<img width="650" height="202" alt="image" src="https://github.com/user-attachments/assets/479c0113-b92c-4b6c-8078-b8bb13a66524" />. 

__Fig 12__.  _The silicone sheet housing for the Optode probes with LEDs and Photodiodes inserted.
The silicone sheet is 2mm in thickness. The LEDs and Photodiode IC OPT101 are inserted in the housing cutout, and the wires are laid along the strap length._. 
  
<img width="1246" height="200" alt="image" src="https://github.com/user-attachments/assets/8a026dff-a134-4787-8c80-0028a4b505ba" />. 

__Fig 13__.  _Optode probes, LED-driver IC LP55231 and Microcontroller connections.
The optode probe LEDs and connected to the current-sink LED-driver LP55231 and the photodiode amplifier outputs are connected to the microcontroller GPIO pins. The SDA and SCL pins of the microcontroller are connected to the LP55231 SDA/SCL pins to control the firing sequence._  
  
<img width="1503" height="980" alt="image" src="https://github.com/user-attachments/assets/10551943-3316-4807-9b97-93b6d2e2cdeb" />. 

_ECG System Initial Design and Construction_. 
- Connect the MAX30003 Protocentral ECG board (40) to the microcontroller (µC) as shown in Figure 14.
- Insert the 2-electrode cable plug into the 3.5mm jack on the MAX30003 board.
- Glue two snap pins to the MX30003 board plastic housing and attach it to the chest-strap.  
  
__Fig 14__.  _ECG electrodes, signal processing board MAX30003 and Microcontroller connections.
The electrodes are connected to the MAX30003 board through a 3.5mm audio jack. The MAX30003 board is controlled by the CS, SCK and the MOSI pins. The digitized serial data is passed from the MAX30003 board to the microcontroller via the MISO pin.__  
  
<img width="3043" height="1310" alt="image" src="https://github.com/user-attachments/assets/d51018e1-b0df-4f33-93ce-9d028906ed18" />. 

_Cognitive Tasks Design_. 
- Task-1: Video-based, watch video on CBT-Vicious Cycles from (32) for 15 minutes. This video explains what CBT is and how the vicious cycle of anxiety can be broken.
- Task-2: Video-based, watch the “NOW: Mindfulness exercise”, “Think: Thought Challenging” and “Fact or Opinion” videos under the “Long Solution videos” in (32).
- Task-3: Text-based, take the five tests, “Generalized Anxiety Disorder Test”, ‘Performance Anxiety Test”, “Social Anxiety Test”, “How Stressed Are You” and “Your Mental Health Today” in (33) for 3 minutes each.
- Task-4:  Audio-based, listen to the audio guides, “How to cope with anxiety” and “Unhelpful Thinking” in (34) for a total of 15 minutes.
- Task-5: Audio-based, listen to the audio guide and follow the instructions for a breathing exercise in (35) for 15 minutes.
- Task-6: Text-based, take the quiz on “Create your own mind plan” in (35) for 15 minutes.
- Task-7: Video-based, watch the seven short videos in each section that covers several CBT techniques for a total of 15 minutes in (36).

_Microcontroller Firmware Design_. 
- In the Arduino integrated development environment (IDE), implement the ECG data capturing waveform from the MAX30003 board for R-R interval, HRV calculation.
- In the Arduino IDE, implement the flow-chart in Figure 8, that sequences the LEDs and captures the output of OPT101 photodiode-trans-impedance-amplifier (TIA)
- In the Arduino IDE, implement the transmission of this data to a mobile device using multicast domain name service (mDNS) and WebSocket server libraries with HTML/CSS and JAVASCRIPT for displaying values and implementing “Start acquisition” and “Stop acquisition” buttons (Figure 15).
- The C++ Arduino code in (37) shows the various functions for acquiring, filtering, processing and calculating ΔHbO2, ΔHHb, ΔrSO2, HRVrmssd and CRS (Figure 15).

<img width="892" height="556" alt="image" src="https://github.com/user-attachments/assets/bdc94d71-4a4f-450e-aaf6-c52572a79c1f" />. 

_Initial Design Testing-fNIRs_. 
- STEP-1: With OPT101 photodiode gain set at default, place the probe system on the forehead.
- STEP-2: Run the program with the chip LP55231 current setting for the LEDs from 12mA to 24mA in steps of 6mA and varying pulse width modulation (PWM) setting between 2047 (50%) to 4095 (100%) in steps of 1024.
- STEP-3: Measure voltage values of OPT101 at the general-purpose input-output (GPIO) pins of the microcontroller for the settings in STEP-2. Results are tabulated in Table-6
- STEP-4: After 300 seconds, measure the temperature of the forehead using the thermometer. Take readings every 300 seconds for 15 minutes (3 readings). Repeat this in the morning, afternoon and evening spaced equally. Results are tabulated in Table-8.
- STEP-5: With the microcontroller analog-to-digital converter (ADC) gain setting at 50% (6dB), repeat STEP-2. Results are tabulated in Table-7.  
  
  
<img width="468" height="279" alt="image" src="https://github.com/user-attachments/assets/e7a80f2c-4617-4da0-801d-e398d480ce38" />. 
  
<img width="468" height="484" alt="image" src="https://github.com/user-attachments/assets/4d341316-640f-4395-b1d9-7904cee6e872" />. 
  
<img width="468" height="283" alt="image" src="https://github.com/user-attachments/assets/5cd28a0d-baef-4d70-9a68-e0c52a9b953c" />. 

_fNIRs – Modified Design and Testing_. 
- Modify the fNIRs probe by creating a similar probe housing as in Figure 9, but with a spacing of 10mm instead of 20mm between the probes.
- Repeat the same steps STEP-1, STEP-2, STEP-3 and STEP-5 of the initial design testing in section 2.5.5. Results for the default and 50% gain setting in ADC are in Table-9 and Table-10.  
  
  
<img width="468" height="339" alt="image" src="https://github.com/user-attachments/assets/b28c03a1-887a-4ccf-95f8-f038703536ce" />. 
  
<img width="468" height="280" alt="image" src="https://github.com/user-attachments/assets/98f4bff2-168b-45f2-92b7-c1e491ff86f4" />. 

  
_ECG – Initial Design_. 
- STEP-1: In the seated position, place the two-electrode probe in lead-1 configuration under the right-clavicle (marked RA on the electrode) and left-clavicle (marked LA).
- STEP-2: Run the R-R peak calculation program in the microcontroller (section 2.5.4) to calculate the R-R interval mean and heart rate variability (HRV) mean using the RMSSD method.
- STEP-3: Since it is an instantaneous reading, note the mean value every 3 minutes for 15 minutes. Results are tabulated in Table-11.
  
<img width="468" height="282" alt="image" src="https://github.com/user-attachments/assets/6583316e-acd3-4664-9ee4-a02a1b0fb537" />. 
  
  
_Modified System Design_. 
- The three-electrode probe in the modified design was used, which has a third lead to cancel common-mode noise and baseline wander. Place the three–electrode in lead-II configuration, red under the right-clavicle (RA), yellow under left clavicle (LA) and green under left rib cage.
- Repeat the test as in STEP-1 to STEP-3 from section 2.5.7. Results are tabulated in Table-12.
  
<img width="468" height="297" alt="image" src="https://github.com/user-attachments/assets/540c6546-d2c3-40d0-9fbd-96ee5e5ad251" />. 

_Battery Testing_. 
- Attach the fNIRs probe system on the forehead and ECG system with two-electrode on the right and left wrists underside.
- With a fully charged battery, power up the microcontroller and start data collection. Start the data transmission of values (HRVrmssd, ΔHbO2, ΔHHb, ΔrSO2) to the mobile device web browser.
- Record battery voltage (using the firmware battery management library for ESP32-S3) every 5 minutes for a total of 15 minutes.
- Repeat this in the morning, afternoon and evening, for a total of 9 readings. Results are shown in Table-13.  
  
<img width="468" height="282" alt="image" src="https://github.com/user-attachments/assets/caf5f98d-1360-4cc7-bdee-e01dd4693da7" />. 
  
_Complete System Testing with Microcontroller Firmware._. 
- The complete system consists of the fNIRs probes connected to the LED driver IC LP55231 (38) and the microcontroller along with the ECG electrode leads connected to the MAX30003 board which again is connected to the microcontroller. A LiPo battery connected to the microcontroller powers the whole system. A laptop device is used to display various metrics calculated by the microcontroller.
- In the seated position, the fNIRs probes are connected to the person’s forehead such that the right and left probes are directly above the eyebrows. The ECG 3-electrode leads are placed as detailed in STEP-1 of 2.5.7. The microcontroller is powered up using the battery. 
- STEP-1: On the web-browser, enter the local LAN address created for this using the mDNS service. Click on the “Start acquisition” button to start the resting-state data capture for ΔrSO2 and HRVrmssd. Click on the “Stop acquisition” button to stop recording the data after 60 seconds. This will log the ΔHbO2, ΔHHb data at 50Hz sampling rate. The ΔrSO2 during this time should be a very small number close to 0 since the change in HbO2 is very small and the algorithm drops absolute values of ΔHbT below a threshold of 0.03µM. The fNIRs signals are sampled at 50Hz, and mean values are logged every 5 seconds. The ECG signals are sampled at 256Hz and HRVrmssd mean values are logged every 5 seconds.
- STEP-2: Open the CBT task as shown in Table-15 for the day and session time. Click the “Start acquisition” button to start capturing both fNIRs data and ECG data. Start the task and continue for 15 minutes. The running average of ΔrSO2 and HRVrmssd (over 60 second windows) is displayed on the web browser on the local LAN address page. Click on the “Stop acquisition” button at the end of 15 minutes. A sample of the unfiltered and filtered signal waveforms during the 60-second window is shown in Figure 16.
   
<img width="620" height="440" alt="image" src="https://github.com/user-attachments/assets/3439ec16-d379-4a22-af13-59f9879abfa8" />. 

- Repeat STEP-1 and STEP-2 mentioned above for different sessions three times every alternate day for seven days, completing 21 trials. All data for fNIRs and ECG are logged in the laptop device in the path specified in the code (37). The 8-channel average optode coupling SNR (OCSNR) is calculated and displayed from STEP-1 resting-state data. The results are shown in Table-14.
- The CRS score is calculated and updated every 60 seconds since the hemodynamic response cycle span is around 60 seconds. The mean CRS score for each trial is shown in Table-15
- The functional task SNR is calculated by post processing the filtered ΔHbO2 data that is stored for the 21 trials. Results are shown in Table-15.
  
<img width="468" height="625" alt="image" src="https://github.com/user-attachments/assets/db021c85-56b4-406d-8bf3-d20a6848d2c0" />. 
  
<img width="468" height="653" alt="image" src="https://github.com/user-attachments/assets/e89d8c00-f99f-4e23-83de-d755be30ee57" />. 
  













































